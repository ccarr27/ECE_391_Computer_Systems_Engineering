//            vioblk.c - VirtIO serial port (console)
//           

#include "virtio.h"
#include "intr.h"
#include "halt.h"
#include "heap.h"
#include "io.h"
#include "device.h"
#include "error.h"
#include "string.h"
#include "thread.h"

//            COMPILE-TIME PARAMETERS
//           

#define VIOBLK_IRQ_PRIO 1

//            INTERNAL CONSTANT DEFINITIONS
//           

//            VirtIO block device feature bits (number, *not* mask)

#define VIRTIO_BLK_F_SIZE_MAX 1
#define VIRTIO_BLK_F_SEG_MAX 2
#define VIRTIO_BLK_F_GEOMETRY 4
#define VIRTIO_BLK_F_RO 5
#define VIRTIO_BLK_F_BLK_SIZE 6
#define VIRTIO_BLK_F_FLUSH 9
#define VIRTIO_BLK_F_TOPOLOGY 10
#define VIRTIO_BLK_F_CONFIG_WCE 11
#define VIRTIO_BLK_F_MQ 12
#define VIRTIO_BLK_F_DISCARD 13
#define VIRTIO_BLK_F_WRITE_ZEROES 14

//            INTERNAL TYPE DEFINITIONS
//           

//            All VirtIO block device requests consist of a request header, defined below,
//            followed by data, followed by a status byte. The header is device-read-only,
//            the data may be device-read-only or device-written (depending on request
//            type), and the status byte is device-written.

struct vioblk_request_header
{
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
};

//            Request type (for vioblk_request_header)

#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_T_OUT 1

//            Status byte values

#define VIRTIO_BLK_S_OK 0
#define VIRTIO_BLK_S_IOERR 1
#define VIRTIO_BLK_S_UNSUPP 2

//            Main device structure.
//           
//            FIXME You may modify this structure in any way you want. It is given as a
//            hint to help you, but you may have your own (better!) way of doing things.



struct vioblk_device
{
    volatile struct virtio_mmio_regs *regs;
    struct io_intf io_intf;
    uint16_t instno;
    uint16_t irqno;
    int8_t opened;
    int8_t readonly;

    //            optimal block size
    uint32_t blksz;
    //            current position
    uint64_t pos;
    //            sizeo of device in bytes
    uint64_t size;
    //            size of device in blksz blocks
    uint64_t blkcnt;

    struct
    {
        //            signaled from ISR
        struct condition used_updated;

        //            We use a simple scheme of one transaction at a time.

        union
        {
            struct virtq_avail avail;
            char _avail_filler[VIRTQ_AVAIL_SIZE(1)];
        };

        union
        {
            volatile struct virtq_used used;
            char _used_filler[VIRTQ_USED_SIZE(1)];
        };

        //            The first descriptor is an indirect descriptor and is the one used in
        //            the avail and used rings. The second descriptor points to the header,
        //            the third points to the data, and the fourth to the status byte.

        struct virtq_desc desc[4];
        struct vioblk_request_header req_header;
        uint8_t req_status;
    } vq;

    //            Block currently in block buffer
    uint64_t bufblkno;
    //            Block buffer
    char *blkbuf;
};

//            INTERNAL FUNCTION DECLARATIONS
//           

static int vioblk_open(struct io_intf **ioptr, void *aux);

static void vioblk_close(struct io_intf *io);

static long vioblk_read(
    struct io_intf *restrict io,
    void *restrict buf,
    unsigned long bufsz);

static long vioblk_write(
    struct io_intf *restrict io,
    const void *restrict buf,
    unsigned long n);

static int vioblk_ioctl(
    struct io_intf *restrict io, int cmd, void *restrict arg);

static void vioblk_isr(int irqno, void *aux);

//            IOCTLs

static int vioblk_getlen(const struct vioblk_device *dev, uint64_t *lenptr);
static int vioblk_getpos(const struct vioblk_device *dev, uint64_t *posptr);
static int vioblk_setpos(struct vioblk_device *dev, const uint64_t *posptr);
static int vioblk_getblksz(
    const struct vioblk_device *dev, uint32_t *blkszptr);

//            EXPORTED FUNCTION DEFINITIONS
//           

//            Attaches a VirtIO block device. Declared and called directly from virtio.c.

static const struct io_ops vioblk_ops = {
    .close = vioblk_close,
    .read = vioblk_read, 
    .write = vioblk_write,
    .ctl = vioblk_ioctl
};

void vioblk_attach(volatile struct virtio_mmio_regs *regs, int irqno)
{
    //            FIXME add additional declarations here if needed

    virtio_featset_t enabled_features, wanted_features, needed_features;
    struct vioblk_device *dev;
    uint_fast32_t blksz;
    int result;

    assert(regs->device_id == VIRTIO_ID_BLOCK);

    //            Signal device that we found a driver

    regs->status |= VIRTIO_STAT_DRIVER;
    //            fence o,io
    __sync_synchronize();

    //            Negotiate features. We need:
    //             - VIRTIO_F_RING_RESET and
    //             - VIRTIO_F_INDIRECT_DESC
    //            We want:
    //             - VIRTIO_BLK_F_BLK_SIZE and
    //             - VIRTIO_BLK_F_TOPOLOGY.

    virtio_featset_init(needed_features);
    virtio_featset_add(needed_features, VIRTIO_F_RING_RESET);
    virtio_featset_add(needed_features, VIRTIO_F_INDIRECT_DESC);
    virtio_featset_init(wanted_features);
    virtio_featset_add(wanted_features, VIRTIO_BLK_F_BLK_SIZE);
    virtio_featset_add(wanted_features, VIRTIO_BLK_F_TOPOLOGY);
    result = virtio_negotiate_features(regs,
                                       enabled_features, wanted_features, needed_features);

    if (result != 0)
    {
        kprintf("%p: virtio feature negotiation failed\n", regs);
        return;
    }

    //            If the device provides a block size, use it. Otherwise, use 512.

    if (virtio_featset_test(enabled_features, VIRTIO_BLK_F_BLK_SIZE))
        blksz = regs->config.blk.blk_size;
    else
        blksz = 512;

    debug("%p: virtio block device block size is %lu", regs, (long)blksz);

    //            Allocate initialize device struct

    dev = kmalloc(sizeof(struct vioblk_device) + blksz);
    memset(dev, 0, sizeof(struct vioblk_device));

    //            FIXME Finish initialization of vioblk device here
    uint64_t capacity = regs->config.blk.capacity;
    uint64_t size_in_bytes = capacity * 512ULL;
    dev->regs = regs;
    dev->irqno = irqno;
    dev->blksz = blksz;
    dev->size = size_in_bytes;
    dev->blkcnt = capacity;
    dev->readonly = virtio_featset_test(enabled_features, VIRTIO_BLK_F_RO);
    dev->opened = 0;
    dev->pos = 0;
    dev->blkbuf = kmalloc(blksz);
    if (!dev->blkbuf)
    {
        kprintf("Failed to allocate memory for block buffer\n");
        kfree(dev);
        return;
    }

    // Initialize IO interface
    dev->io_intf.ops = &vioblk_ops;

    // Select the queue (queue 0)
    regs->queue_sel = 0;
    // Memory barrier
    __sync_synchronize();

    // Get the maximum queue size
    uint32_t queue_num_max = regs->queue_num_max;
    if (queue_num_max == 0)
    {
        kprintf("Device does not support queue 0\n");
        kfree(dev->blkbuf);
        kfree(dev);
        return;
    }

    // Set the queue size to 1 for simplicity
    uint16_t queue_size = 1;
    regs->queue_num = queue_size;

    // Allocate memory for the virtqueue
    dev = kmalloc(sizeof(struct vioblk_device));
    if (!dev) {
        kprintf("Failed to allocate memory for vioblk_device\n");
        return;
    }
    memset(dev, 0, sizeof(struct vioblk_device));

    // Initialize avail ring
    memset(&dev->vq.avail, 0, sizeof(dev->vq.avail));
    dev->vq.avail.idx = 0;

    // Initialize used ring
    memset((void *)&dev->vq.used, 0, sizeof(dev->vq.used));
    dev->vq.used.idx = 0;

    // Attach the virtqueue to the device
    virtio_attach_virtq(
        regs,
        0, // queue id
        queue_size,
        (uint64_t)dev->vq.desc,  // desc_addr
        (uint64_t)&dev->vq.used, // used_addr
        (uint64_t)&dev->vq.avail // avail_addr
    );

    // Enable the virtqueue
    virtio_enable_virtq(regs, 0);

    condition_init(&dev->vq.used_updated, *(&dev->vq.used_updated.name));

    // Register the ISR
    intr_register_isr(dev->irqno, VIOBLK_IRQ_PRIO, vioblk_isr, dev);

    regs->status |= VIRTIO_STAT_DRIVER_OK;
    //            fence o,oi
    __sync_synchronize();
}

int vioblk_open(struct io_intf **ioptr, void *aux)
{
    //            FIXME your code here
    struct vioblk_device *dev = (struct vioblk_device *)aux;

    if (dev->opened)
    {
        return -EBUSY; // Device is already opened
    }

    dev->opened = 1;
    *ioptr = &dev->io_intf;
    return 0;
}

//            Must be called with interrupts enabled to ensure there are no pending
//            interrupts (ISR will not execute after closing).

void vioblk_close(struct io_intf *io)
{
    //            FIXME your code here
    struct vioblk_device *dev = (struct vioblk_device *)((char *)io - offsetof(struct vioblk_device, io_intf));
    dev->opened = 0;
    virtio_reset_virtq(dev->regs, 0);
}

long vioblk_read(
    struct io_intf *restrict io,
    void *restrict buf,
    unsigned long bufsz)
{
    //            FIXME your code here
    unsigned long total_read = 0;
    uint64_t sector, sector_offset;
    unsigned long to_read;
    struct vioblk_device *dev = (struct vioblk_device *)((char *)io - offsetof(struct vioblk_device, io_intf));
    while (total_read < bufsz)
    {
        // Calculate current sector and offset
        sector = dev->pos / dev->blksz;
        sector_offset = dev->pos % dev->blksz;

        // Determine how much to read in this iteration
        to_read = dev->blksz - sector_offset;
        if (to_read > (bufsz - total_read))
        {
            to_read = bufsz - total_read;
        }

        // Prepare request header
        dev->vq.req_header.type = VIRTIO_BLK_T_IN;
        dev->vq.req_header.sector = sector;
        dev->vq.req_header.reserved = 0;

        // Prepare descriptors
        // Descriptor 0: Request header
        // Descriptor 1: Data buffer (device writes into this)
        // Descriptor 2: Status byte

        // Add descriptor index to avail ring
        // Notify device
        // Wait for completion (use condition variable)

        // Check status byte
        if (dev->vq.req_status != VIRTIO_BLK_S_OK)
        {
            return -EIO;
        }

        // Copy data from block buffer to user buffer
        memcpy((char *)buf + total_read, dev->blkbuf + sector_offset, to_read);

        // Update position and counters
        dev->pos += to_read;
        total_read += to_read;
    }
    return total_read;
}

long vioblk_write(
    struct io_intf *restrict io,
    const void *restrict buf,
    unsigned long n)
{
    //            FIXME your code here
    unsigned long total_written = 0;
    uint64_t sector, sector_offset;
    unsigned long to_write;
    struct vioblk_device *dev = (struct vioblk_device *)((char *)io - offsetof(struct vioblk_device, io_intf));
    if (dev->readonly)
    {
        return -ENOTSUP; // Read-only file system
    }
    while (total_written < n)
    {
        // Calculate current sector and offset
        sector = dev->pos / dev->blksz;
        sector_offset = dev->pos % dev->blksz;

        // Determine how much to write in this iteration
        to_write = dev->blksz - sector_offset;
        if (to_write > (n - total_written))
        {
            to_write = n - total_written;
        }

        // If not a full block, read existing data first
        if (sector_offset != 0 || to_write != dev->blksz)
        {
            // Prepare read request to fill blkbuf
        }

        // Copy data from user buffer to blkbuf
        memcpy(dev->blkbuf + sector_offset, (char *)buf + total_written, to_write);

        // Prepare write request
        dev->vq.req_header.type = VIRTIO_BLK_T_OUT;
        dev->vq.req_header.sector = sector;
        dev->vq.req_header.reserved = 0;

        // Prepare descriptors
        // Descriptor 0: Request header
        // Descriptor 1: Data buffer (device reads from this)
        // Descriptor 2: Status byte

        // Add descriptor index to avail ring
        // Notify device
        // Wait for completion

        // Check status byte
        if (dev->vq.req_status != VIRTIO_BLK_S_OK)
        {
            return -EIO;
        }

        // Update position and counters
        dev->pos += to_write;
        total_written += to_write;
    }
    return total_written;
}

int vioblk_ioctl(struct io_intf *restrict io, int cmd, void *restrict arg)
{
    struct vioblk_device *const dev = (void *)io -
                                      offsetof(struct vioblk_device, io_intf);

    trace("%s(cmd=%d,arg=%p)", __func__, cmd, arg);

    switch (cmd)
    {
    case IOCTL_GETLEN:
        return vioblk_getlen(dev, arg);
    case IOCTL_GETPOS:
        return vioblk_getpos(dev, arg);
    case IOCTL_SETPOS:
        return vioblk_setpos(dev, arg);
    case IOCTL_GETBLKSZ:
        return vioblk_getblksz(dev, arg);
    default:
        return -ENOTSUP;
    }
}

void vioblk_isr(int irqno, void *aux)
{
    //            FIXME your code here
    struct vioblk_device *dev = (struct vioblk_device *)aux;
    uint32_t isr_status = dev->regs->interrupt_status;
    dev->regs->interrupt_ack = isr_status;
    if (isr_status & 0x1)
    {
        // Signal condition variable to wake up waiting thread
        condition_broadcast(&dev->vq.used_updated);
    }
}

int vioblk_getlen(const struct vioblk_device *dev, uint64_t *lenptr)
{
    //            FIXME your code here
     if (!lenptr) {
        return -EINVAL;
    }
    *lenptr = dev->size;
    return 0;
}

int vioblk_getpos(const struct vioblk_device *dev, uint64_t *posptr)
{
    //            FIXME your code here
     if (!posptr) {
        return -EINVAL;
    }
    *posptr = dev->pos;
    return 0;
}

int vioblk_setpos(struct vioblk_device *dev, const uint64_t *posptr)
{
    //            FIXME your code here
    if (!posptr || *posptr > dev->size) {
        return -EINVAL;
    }
    dev->pos = *posptr;
    return 0;
}

int vioblk_getblksz(
    const struct vioblk_device *dev, uint32_t *blkszptr)
{
    //            FIXME your code here
    if (!blkszptr) {
        return -EINVAL;
    }
    *blkszptr = dev->blksz;
    return 0;
}
