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

// struct vioblk_device * save_pointer;

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
    .ctl = vioblk_ioctl};

/*
Description: this function initializes virt io block
Purpose: fills out descriptors and sets everything up
Input: takes in the registers
Output: void so does not return anything
*/
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

    // Initialize the neccesary feature for the virtio
    // (Some features were not used but I initialized them anyway)
    virtio_featset_init(needed_features);
    virtio_featset_add(needed_features, VIRTIO_F_RING_RESET);
    virtio_featset_add(needed_features, VIRTIO_F_INDIRECT_DESC);
    // virtio_featset_add(needed_features, VIRTIO_F_EVENT_IDX);
    // virtio_featset_add(needed_features, VIRTIO_F_ANY_LAYOUT);

    virtio_featset_init(wanted_features);
    virtio_featset_add(wanted_features, VIRTIO_BLK_F_BLK_SIZE);
    virtio_featset_add(wanted_features, VIRTIO_BLK_F_TOPOLOGY);

    result = virtio_negotiate_features(regs,
                                       enabled_features, wanted_features, needed_features);

    if (result != 0)
    {
        kprintf("%d: feature error\n", result);
        kprintf("%p: virtio feature negotiation failed\n", regs);
        // return;
    }

    //            If the device provides a block size, use it. Otherwise, use 512.

    if (virtio_featset_test(enabled_features, VIRTIO_BLK_F_BLK_SIZE))
        blksz = regs->config.blk.blk_size;
    else
        blksz = 512;

    debug("%p: virtio block device block size is %lu", regs, (long)blksz);

    // Allocate initialize device struct

    dev = kmalloc(sizeof(struct vioblk_device) + blksz);

    //set everything to 0 just in case

    memset(dev,0, sizeof(struct vioblk_device));

    // save_pointer = dev;

    //            FIXME Finish initialization of vioblk device here

    // Initilze the device and its variables
    uint64_t capacity = regs->config.blk.capacity; // capacity from config
    uint64_t size_in_bytes = capacity * blksz;    // 512 bytes per block
    // set the rest of the neccesary field
    dev->regs = regs;
    dev->irqno = irqno;
    dev->blksz = blksz;
    dev->size = size_in_bytes;
    dev->blkcnt = capacity;
    dev->opened = 0;
    dev->pos = 0;
    dev->bufblkno=0;
    // Allocate memory for block buffer
    // dev->blkbuf = kmalloc(blksz);
    dev->blkbuf = (char *)(dev +1); //assign the pointer to the buffer we allocated

    

    // Select the queue (queue 0)
    // regs->queue_sel = 0;
    // Memory barrier
    __sync_synchronize();

    // Get the maximum queue size
    // uint32_t queue_num_max = regs->queue_num_max;

    uint16_t queue_size = 1;
    // regs->queue_num = 1;
    // size_t vq_total_size = desc_size + avail_size + used_size;

    virtio_attach_virtq(
        regs,
        0, // queue id
        queue_size,
        (uint64_t)&dev->vq.desc, // desc_addr
        (uint64_t)&dev->vq.used, // used_addr
        (uint64_t)&dev->vq.avail // avail_addr
    );
    // kprintf("finished int VirtQ\n");

    // dev->vq.avail.flags = 0;
    // dev->vq.avail.idx = 0;

    // // Initialize used ring
    // dev->vq.used.flags = 0;
    // dev->vq.used.idx = 0;



    // Register the ISR
    intr_register_isr(dev->irqno, VIOBLK_IRQ_PRIO, vioblk_isr, dev);
    // Register device
    dev->instno = device_register("blk", &vioblk_open, dev);

    // FIXME END

    regs->status |= VIRTIO_STAT_DRIVER_OK;
    //            fence o,oi
    __sync_synchronize();
}

/*
Description: this opens the vioblk
Purpose: the purpose is to set both the avail and used queues
Input: takes in a pointer to an io_intf andio_intf extra data
Output: return 0 if success
*/
int vioblk_open(struct io_intf **ioptr, void *aux)
{
    //            FIXME your code here
    struct vioblk_device *dev = (struct vioblk_device *)aux;
    // struct vioblk_device *dev = save_pointer;

    if (dev->opened)
    {
        return -EBUSY; // Device is already opened
    }

    virtio_enable_virtq(dev->regs, 0); //id is 0
    intr_enable_irq(dev->irqno);

    // Initialize thread condition   kprintf("%d \n",num_queues);
    condition_init(&dev->vq.used_updated, "used_updated");

    // Initialize IO interface
    dev->io_intf.ops = &vioblk_ops;

    *ioptr = &dev->io_intf; // Set ioptr to the io interface
    dev->opened = 1;        // Set opened
    return 0;
}

//            Must be called with interrupts enabled to ensure there are no pending
//            interrupts (ISR will not execute after closing).

/*
Description: this closes the vioblk device
Purpose: the purpose if to close the device
Input: the io_intf struct
Output:void so return nothing
*/
void vioblk_close(struct io_intf *io)
{
    //            FIXME your code here
    struct vioblk_device *dev = (struct vioblk_device *)((char *)io - offsetof(struct vioblk_device, io_intf));
    dev->io_intf.ops = NULL;
    dev->opened = 0;
    virtio_reset_virtq(dev->regs, 0);//id is 0
    intr_disable_irq(dev->irqno); //disable interrupts
}

/*
Description: this reads from the buffers
Purpose:it reads an amount of bufsz from the disk into buf
Input: we pass in the io_inf and buffer and the size we want to read
Output: return the amount read
*/

long vioblk_read(
    struct io_intf *restrict io,
    void *restrict buf,
    unsigned long bufsz)
{
    //            FIXME your code here
    //check inputs:
    if(bufsz == 0){
        return 0;  //bufsz was zero so we read 0
    }

    // geting the dev
    struct vioblk_device *dev = (struct vioblk_device *)((char *)io - offsetof(struct vioblk_device, io_intf));

    //make sure the dev is good
    if(dev->opened == 0){
        return -1;      //device was closed
    }

    // chnage the buf pointer to a char buf pointer
    char *char_buf = (char *)buf;

    // total amount read
    uint64_t total_read = 0;

    // the 512 block sectore we are in and the offset of the sector we are in
    uint64_t sector, sector_offset;

    // how much has to be read in this sector
    uint64_t to_read;

    // type cast block size to 64bits
    uint64_t blksz = dev->blksz;

    while (total_read < (uint64_t)bufsz)
    {
        if(dev->pos >= dev->size){
            break;
        }
        // Calculate current sector and offset
        sector = dev->pos / blksz;
        sector_offset = dev->pos % blksz;

        // Determine how much to read in this iteration
        to_read = (blksz - sector_offset);

        if (to_read < ((uint64_t)bufsz - total_read))
        {
            to_read = (blksz - sector_offset);
        }
        else{
            to_read = ((uint64_t)bufsz - total_read);
        }

        // Prepare request header
        // struct vioblk_request_header *req_hdr = &dev ->vq.req_header;
        dev->vq.req_header.type = VIRTIO_BLK_T_IN;
        dev->vq.req_header.sector = sector;
        dev->vq.req_header.reserved = 0;

        // prepare descriptor table
        // inderect
        dev->vq.desc[0].addr = (uint64_t)&dev->vq.desc[1];
        dev->vq.desc[0].len = sizeof(struct virtq_desc) * 3;
        dev->vq.desc[0].flags = VIRTQ_DESC_F_INDIRECT;
        dev->vq.desc[0].next = -1; //technically its own thing

        // header
        dev->vq.desc[1].addr = (uint64_t)&dev->vq.req_header;
        dev->vq.desc[1].len = sizeof(struct vioblk_request_header); // not sure about this
        dev->vq.desc[1].flags = VIRTQ_DESC_F_NEXT;
        dev->vq.desc[1].next = 1;

        // data
        //  dev->vq.desc[2].addr = &dev->blkbuf;
        dev->vq.desc[2].addr = (uint64_t)dev->blkbuf;
        dev->vq.desc[2].len = dev->blksz;
        dev->vq.desc[2].flags = VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE;
        dev->vq.desc[2].next = 2;

        // status
        dev->vq.desc[3].addr = (uint64_t)&dev->vq.req_status; // status
        dev->vq.desc[3].len = sizeof(dev->vq.req_status);
        dev->vq.desc[3].flags = VIRTQ_DESC_F_WRITE;
        dev->vq.desc[3].next = -1; //nothing next

        // prepare flags
        // dev->vq.avail.flags = 0;
        // dev->vq.avail.idx = 1;

        //only one queue so one descriptor
        dev->vq.avail.ring[0] = 0;

        __sync_synchronize();
        dev->vq.avail.idx++;
        __sync_synchronize();

        //notify that a descriptor is waiting
        dev->regs->queue_notify = 0;
        __sync_synchronize();



        int intr_num = intr_disable();
        while (dev->vq.avail.idx != dev->vq.used.idx) // not sure if correct
        {
            condition_wait(&dev->vq.used_updated);
        }
        intr_restore(intr_num);

        // Check status byte
        if (dev->vq.req_status != VIRTIO_BLK_S_OK)
        {
            return -EIO;
        }
        // console_printf("file: %s line: %d \n",__FILE__, __LINE__);

        uint64_t x = char_buf + (uint64_t)total_read;

        // console_printf("%x \n", x);
        // Copy data from block buffer to user buffer
        // memcpy(char_buf + (uint64_t)total_read, dev->blkbuf + sector_offset, to_read);
        memcpy(char_buf + (uint64_t)total_read, dev->blkbuf + sector_offset, to_read);

        // console_printf("file: %s line: %d \n",__FILE__, __LINE__);

        // Update position and counters
        dev->pos += to_read;
        total_read += to_read;
    }
    // kprintf("read op num %d \n", total_read);

    return (unsigned long)total_read;
}

/*
Description: this writes from the buf to the disk
Purpose: the purpose is to write an amount from the buffer to disk
Input: should take in the io, buf to read from and n for number of bites to read
Output: return the amount read
*/
long vioblk_write(
    struct io_intf *restrict io,
    const void *restrict buf,
    unsigned long n)
{
    
    //            FIXME your code here
    struct vioblk_device *dev = (struct vioblk_device *)((char *)io - offsetof(struct vioblk_device, io_intf));
    //make sure the dev is good
    if(dev->opened == 0){
        return -1;      //device was closed
    }

    if (dev->readonly)
    {
        return -ENOTSUP; // Read-only file system
    }

    //check inputs:
    if(n == 0){
        return 0;  //bufsz was zero so we read 0
    }

    //variables used
    uint64_t total_written = 0;
    uint64_t sector, sector_offset;
    uint64_t to_write;
    uint64_t blksz = dev->blksz;

    char * buffer = (char *)buf;




    while (total_written < (uint64_t)n)
    {

        if(dev->pos >= dev->size){
            break;
        }

        // Calculate current sector and offset
        sector = dev->pos / blksz;
        sector_offset = dev->pos % blksz;

        // Determine how much to write in this iteration
        to_write = blksz - sector_offset;
        if (to_write >= (n - total_written))
        {
            to_write = n - total_written;
        }

        // // If not a full block, read existing data first
        // if (sector_offset != 0 || to_write != blksz) // Donno what to do here
        // {
        //     // Prepare read request to fill blkbuf
        // }

        // Copy data from user buffer to blkbuf
        memcpy(dev->blkbuf + sector_offset, buffer + total_written, to_write);

        // Prepare write request
        dev->vq.req_header.type = VIRTIO_BLK_T_OUT;
        dev->vq.req_header.sector = sector;
        dev->vq.req_header.reserved = 0;

        dev->vq.desc[0].addr = (uint64_t)&dev->vq.desc[1];
        dev->vq.desc[0].len = sizeof(struct virtq_desc) * 3;
        dev->vq.desc[0].flags = VIRTQ_DESC_F_INDIRECT;
        dev->vq.desc[0].next = 0;

        // header
        dev->vq.desc[1].addr = (uint64_t)&dev->vq.req_header;
        dev->vq.desc[1].len = sizeof(struct vioblk_request_header); // not sure about this
        dev->vq.desc[1].flags = VIRTQ_DESC_F_NEXT;
        dev->vq.desc[1].next = 1;

        // data
        dev->vq.desc[2].addr = (uint64_t)dev->blkbuf;
        dev->vq.desc[2].len = dev->blksz;
        dev->vq.desc[2].flags = VIRTQ_DESC_F_NEXT;  //device is auto set to read
        dev->vq.desc[2].next = 2;

        dev->vq.desc[3].addr = (uint64_t)&dev->vq.req_status; // status
        dev->vq.desc[3].len = 1;
        dev->vq.desc[3].flags = VIRTQ_DESC_F_WRITE;

        // dev->vq.avail.idx = 1;

        //add descriptor to available to make this happen
        dev->vq.avail.ring[0] = 0;

        // Check status byte

        __sync_synchronize();
        dev->vq.avail.idx++;
        __sync_synchronize();

        //notify that a descriptor is waiting
        dev->regs->queue_notify = 0;
        __sync_synchronize();


        int intr_num = intr_disable();
        while (dev->vq.avail.idx != dev->vq.used.idx) // not sure if correct
        {
            condition_wait(&dev->vq.used_updated);
        }
        intr_restore(intr_num);

        // Check status byte
        if (dev->vq.req_status != VIRTIO_BLK_S_OK)
        {
            return -EIO;
        }

        // Update position and counters
        dev->pos += to_write;
        total_written += to_write;
    }
    return (long)total_written;
}

// Function: vioblk_ioctl
// Inputs: struct io_intf *restrict io, int cmd, void *restrict arg
// ouput: int
// Description: Maps all the ioctl commands to the appropriate vioblk functions.

int vioblk_ioctl(struct io_intf *restrict io, int cmd, void *restrict arg)
{
    struct vioblk_device *const dev = (void *)io -
                                      offsetof(struct vioblk_device, io_intf);

    trace("%s(cmd=%d,arg=%p)", __func__, cmd, arg);

    // All cases of cmd
    switch (cmd)
    {
        // cmd GETLEN
    case IOCTL_GETLEN:
        return vioblk_getlen(dev, arg);
        // cmd GETPOS
    case IOCTL_GETPOS:
        return vioblk_getpos(dev, arg);
        // cmd SETPOS
    case IOCTL_SETPOS:
        return vioblk_setpos(dev, arg);
        // cmd GETBLKSZ
    case IOCTL_GETBLKSZ:
        return vioblk_getblksz(dev, arg);
    default:
        return -ENOTSUP;
    }
}

// Function: vioblk_isr
// Input: int irqno, void *aux
// Output: void
// Description: vioblk interrupt service routine. will broadcast condition at the end.
void vioblk_isr(int irqno, void *aux)
{
    //            FIXME your code here
    struct vioblk_device *dev = (struct vioblk_device *)aux;
    // isr status is interrupt status
    uint32_t isr_status = dev->regs->interrupt_status;
    // set interrupt acknoledge to isr_status
    dev->regs->interrupt_ack = isr_status;
    if (isr_status & 0x1)
    {
        // Signal condition variable to wake up waiting thread
        condition_broadcast(&dev->vq.used_updated);
    }

}
/*
Description: gets the length of the vioblk
Purpose: to get length
Input: the device
Output: 0 for success
*/

// Function: vioblk_getlen
// Input: const struct vioblk_device *dev, uint64_t *lenptr
// Output: int
// Description: Returns the device size in byte

int vioblk_getlen(const struct vioblk_device *dev, uint64_t *lenptr)
{
    //            FIXME your code here
    if (!lenptr)
    {
        return -EINVAL;
    }
    *lenptr = dev->size;
    return 0;
}

// Function: vioblk_getpos
// Input: const struct vioblk_device *dev, uint64_t *posptr
// Ouput: int
// Description: Retrieve the current position in the disk which is being written or read from

int vioblk_getpos(const struct vioblk_device *dev, uint64_t *posptr)
{
    //            FIXME your code here
    if (!posptr)
    {
        return -EINVAL;
    }
    // retrieve current position
    *posptr = dev->pos;
    return 0;
}

// Function: vioblk_setpos
// Input: struct vioblk_device *dev, const uint64_t *posptr
// Ouput: int
// Description: Sets the current position in the disk which is currently being written to or read from

int vioblk_setpos(struct vioblk_device *dev, const uint64_t *posptr)
{
    //            FIXME your code here
    if (!posptr || *posptr > dev->size)
    {
        return -EINVAL;
    }
    // Sets position
    dev->pos = *posptr;
    return 0;
}
/*
Description:to get the blk size
Purpose:to get the blk size
Input: takes in the deivce
Output: 0 for success
*/

// Function: vioblk_getblksz
// Input: const struct vioblk_device *dev, uint32_t *blkszptr
// Output: int
// Description: Provides the device block size

int vioblk_getblksz(
    const struct vioblk_device *dev, uint32_t *blkszptr)
{
    //            FIXME your code here
    if (!blkszptr)
    {
        return -EINVAL;
    }
    // Set device block size.
    *blkszptr = dev->blksz;
    return 0;
}
