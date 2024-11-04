#include "console.h"
#include "thread.h"
#include "device.h"
#include "uart.h"
#include "timer.h"
#include "intr.h"
#include "heap.h"
#include "virtio.h"
#include "halt.h"
#include "elf.h"
#include "fs.h"
#include "string.h"
#include "io.h"


// End of kernel image (defined in kernel.ld)
extern char _kimg_end[];

#define RAM_SIZE (8*1024*1024)
#define RAM_START 0x80000000UL
#define KERN_START RAM_START
#define USER_START 0x80100000UL

#define UART0_IOBASE 0x10000000
#define UART1_IOBASE 0x10000100
#define UART0_IRQNO 10

#define VIRT0_IOBASE 0x10001000
#define VIRT1_IOBASE 0x10002000
#define VIRT0_IRQNO 1

// Global variables



volatile struct virtio_mmio_regs *virtio_regs;
struct vioblk_device *vioblk_dev;

// Function prototypes
void test_vioblk_attach(void);
void test_vioblk_open(void);
void test_vioblk_read(void);
void test_vioblk_write(void);
void test_vioblk_ioctl(void);
void test_vioblk_close(void);
void test_vioblk_isr(void);



// Main function
void main(void) {
    console_init();
    intr_init();
    heap_init(_kimg_end, (void*)USER_START);
    devmgr_init();
    thread_init();
    timer_init();

    // Initialize the VirtIO block device
    test_vioblk_attach();

    // Open the device
    test_vioblk_open();

    // Perform read operation
    test_vioblk_read();

    // Perform write operation
    test_vioblk_write();

    // Test ioctl functions
    test_vioblk_ioctl();

    // Close the device
    test_vioblk_close();

    // Test interrupt handling
    test_vioblk_isr();

    // Halt the system
}

// Test functions

void test_vioblk_attach(void) {
    // Simulate the virtio_mmio_regs structure
    virtio_regs = (volatile struct virtio_mmio_regs *)VIRT0_IOBASE;

    // Initialize the VirtIO block device
    vioblk_attach(virtio_regs, VIRT0_IRQNO);

    // Retrieve the vioblk_device instance
    // This assumes that vioblk_attach registers the device and you can retrieve it via device manager
    struct io_intf *io;
    int result = device_open(&io, "blk", 0);
    if (result != 0) {
        kprintf("Failed to open device after attach: error %d\n", result);
        return;
    }
    vioblk_dev = (struct vioblk_device *)((char *)io - offsetof(struct vioblk_device, io_intf));
    io->ops->close(io); // Close immediately, will reopen in test_vioblk_open

    kprintf("vioblk_attach completed.\n");
}

void test_vioblk_open(void) {
    struct io_intf *io;
    int result;

    // Open the device
    result = device_open(&io, "blk", 0);
    if (result != 0) {
        kprintf("device_open failed with error %d\n", result);
        return;
    }

    kprintf("vioblk_open succeeded.\n");

    // Store the io interface globally for use in other tests
    vioblk_dev = (struct vioblk_device *)((char *)io - offsetof(struct vioblk_device, io_intf));
}

void test_vioblk_read(void) {
    struct io_intf *io = &vioblk_dev->io_intf;
    uint8_t buffer[512];
    long bytes_read;

    // Read 512 bytes from the device
    bytes_read = io->ops->read(io, buffer, sizeof(buffer));
    if (bytes_read < 0) {
        kprintf("vioblk_read failed with error %ld\n", bytes_read);
    } else {
        kprintf("vioblk_read succeeded, read %ld bytes.\n", bytes_read);
    }
}

void test_vioblk_write(void) {
    struct io_intf *io = &vioblk_dev->io_intf;
    uint8_t buffer[512];
    long bytes_written;

    // Fill buffer with test data
    for (int i = 0; i < sizeof(buffer); i++) {
        buffer[i] = (uint8_t)i;
    }

    // Write 512 bytes to the device
    bytes_written = io->ops->write(io, buffer, sizeof(buffer));
    if (bytes_written < 0) {
        kprintf("vioblk_write failed with error %ld\n", bytes_written);
    } else {
        kprintf("vioblk_write succeeded, wrote %ld bytes.\n", bytes_written);
    }
}

void test_vioblk_ioctl(void) {
    uint64_t len;
    uint64_t pos;
    uint64_t new_pos;
    uint32_t blksz;
    int result;
    struct io_intf *io = &vioblk_dev->io_intf;

    // Get device length
    result = io->ops->ctl(io, IOCTL_GETLEN, &len);
    if (result != 0) {
        kprintf("vioblk_getlen failed with error %d\n", result);
    } else {
        kprintf("Device length: %llu bytes.\n", len);
    }

    // Get current position
    result = io->ops->ctl(io, IOCTL_GETPOS, &pos);
    if (result != 0) {
        kprintf("vioblk_getpos failed with error %d\n", result);
    } else {
        kprintf("Current position: %llu bytes.\n", pos);
    }

    // Set new position
    new_pos = 1024; // Move to byte offset 1024
    result = io->ops->ctl(io, IOCTL_SETPOS, &new_pos);
    if (result != 0) {
        kprintf("vioblk_setpos failed with error %d\n", result);
    } else {
        kprintf("Set new position to %llu bytes.\n", new_pos);
    }

    // Get block size
    result = io->ops->ctl(io, IOCTL_GETBLKSZ, &blksz);
    if (result != 0) {
        kprintf("vioblk_getblksz failed with error %d\n", result);
    } else {
        kprintf("Block size: %u bytes.\n", blksz);
    }
}

void test_vioblk_close(void) {
    struct io_intf *io = &vioblk_dev->io_intf;

    // Close the device
    io->ops->close(io);

    kprintf("vioblk_close completed.\n");
}

void test_vioblk_isr(void) {
    // Simulate an interrupt
    vioblk_isr(VIRT0_IRQNO, vioblk_dev);

    kprintf("vioblk_isr executed.\n");
}