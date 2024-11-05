// #include "console.h"
// #include "thread.h"
// #include "device.h"
// #include "uart.h"
// #include "timer.h"
// #include "intr.h"
// #include "heap.h"
// #include "virtio.h"
// #include "halt.h"
// #include "elf.h"
// #include "fs.h"
// #include "string.h"
// #include "io.h"


// // End of kernel image (defined in kernel.ld)
// extern char _kimg_end[];

// #define RAM_SIZE (8*1024*1024)
// #define RAM_START 0x80000000UL
// #define KERN_START RAM_START
// #define USER_START 0x80100000UL

// #define UART0_IOBASE 0x10000000
// #define UART1_IOBASE 0x10000100
// #define UART0_IRQNO 10

// #define VIRT0_IOBASE 0x10001000
// #define VIRT1_IOBASE 0x10002000
// #define VIRT0_IRQNO 1

// // Global variables


// #define VIOBLK_IRQ_PRIO 1

// //            INTERNAL CONSTANT DEFINITIONS
// //           

// //            VirtIO block device feature bits (number, *not* mask)

// #define VIRTIO_BLK_F_SIZE_MAX 1
// #define VIRTIO_BLK_F_SEG_MAX 2
// #define VIRTIO_BLK_F_GEOMETRY 4
// #define VIRTIO_BLK_F_RO 5
// #define VIRTIO_BLK_F_BLK_SIZE 6
// #define VIRTIO_BLK_F_FLUSH 9
// #define VIRTIO_BLK_F_TOPOLOGY 10
// #define VIRTIO_BLK_F_CONFIG_WCE 11
// #define VIRTIO_BLK_F_MQ 12
// #define VIRTIO_BLK_F_DISCARD 13
// #define VIRTIO_BLK_F_WRITE_ZEROES 14

// //            INTERNAL TYPE DEFINITIONS
// //           

// //            All VirtIO block device requests consist of a request header, defined below,
// //            followed by data, followed by a status byte. The header is device-read-only,
// //            the data may be device-read-only or device-written (depending on request
// //            type), and the status byte is device-written.

// struct vioblk_request_header
// {
//     uint32_t type;
//     uint32_t reserved;
//     uint64_t sector;
// };

// //            Request type (for vioblk_request_header)

// #define VIRTIO_BLK_T_IN 0
// #define VIRTIO_BLK_T_OUT 1

// //            Status byte values

// #define VIRTIO_BLK_S_OK 0
// #define VIRTIO_BLK_S_IOERR 1
// #define VIRTIO_BLK_S_UNSUPP 2

// //            Main device structure.
// //           
// //            FIXME You may modify this structure in any way you want. It is given as a
// //            hint to help you, but you may have your own (better!) way of doing things.

// struct vioblk_device
// {
//     volatile struct virtio_mmio_regs *regs;
//     struct io_intf io_intf;
//     uint16_t instno;
//     uint16_t irqno;
//     int8_t opened;
//     int8_t readonly;

//     //            optimal block size
//     uint32_t blksz;
//     //            current position
//     uint64_t pos;
//     //            sizeo of device in bytes
//     uint64_t size;
//     //            size of device in blksz blocks
//     uint64_t blkcnt;

//     struct
//     {
//         //            signaled from ISR
//         struct condition used_updated;

//         //            We use a simple scheme of one transaction at a time.

//         union
//         {
//             struct virtq_avail avail;
//             char _avail_filler[VIRTQ_AVAIL_SIZE(1)];
//         };

//         union
//         {
//             volatile struct virtq_used used;
//             char _used_filler[VIRTQ_USED_SIZE(1)];
//         };

//         //            The first descriptor is an indirect descriptor and is the one used in
//         //            the avail and used rings. The second descriptor points to the header,
//         //            the third points to the data, and the fourth to the status byte.

//         struct virtq_desc desc[4];
//         struct vioblk_request_header req_header;
//         uint8_t req_status;
//     } vq;

//     //            Block currently in block buffer
//     uint64_t bufblkno;
//     //            Block buffer
//     char *blkbuf;
// };






// volatile struct virtio_mmio_regs *virtio_regs;
// struct vioblk_device *vioblk_dev;

// // Function prototypes
// void test_vioblk_attach(void);
// void test_vioblk_open(void);
// void test_vioblk_read(void);
// void test_vioblk_write(void);
// void test_vioblk_ioctl(void);
// void test_vioblk_close(void);
// void test_vioblk_isr(void);





// // Main function
// void main(void) {
//     console_init();
//     intr_init();
//     heap_init(_kimg_end, (void*)USER_START);
//     devmgr_init();
//     thread_init();
//     timer_init();

//     // Initialize the VirtIO block device
//     test_vioblk_attach();

//     // Open the device
//     test_vioblk_open();

//     // Perform read operation
//     test_vioblk_read();

//     // Perform write operation
//     test_vioblk_write();

//     // Test ioctl functions
//     test_vioblk_ioctl();

//     // Close the device
//     test_vioblk_close();

//     // Test interrupt handling
//     //test_vioblk_isr();

//     // Halt the system
// }

// // Test functions

// void test_vioblk_attach(void) {
//     // Simulate the virtio_mmio_regs structure
//     virtio_regs = (volatile struct virtio_mmio_regs *)VIRT0_IOBASE;

//     // Initialize the VirtIO block device
//     vioblk_attach(virtio_regs, VIRT0_IRQNO);

//     // Retrieve the vioblk_device instance
//     // This assumes that vioblk_attach registers the device and you can retrieve it via device manager
//     struct io_intf *io;
//     int result = device_open(&io, "blk", 0);
//     if (result != 0) {
//         kprintf("Failed to open device after attach: error %d\n", result);
//         return;
//     }
//     vioblk_dev = (struct vioblk_device *)((char *)io - offsetof(struct vioblk_device, io_intf));
//     io->ops->close(io); // Close immediately, will reopen in test_vioblk_open

//     kprintf("vioblk_attach completed.\n");
// }

// void test_vioblk_open(void) {
//     struct io_intf *io;
//     int result;

//     // Open the device
//     result = device_open(&io, "blk", 0);
//     if (result != 0) {
//         kprintf("device_open failed with error %d\n", result);
//         return;
//     }

//     kprintf("vioblk_open succeeded.\n");

//     // Store the io interface globally for use in other tests
//     vioblk_dev = (struct vioblk_device *)((char *)io - offsetof(struct vioblk_device, io_intf));
// }

// void test_vioblk_read(void) {
//     struct io_intf *io = &vioblk_dev->io_intf;
//     uint8_t buffer[512];
//     long bytes_read;

//     // Read 512 bytes from the device
//     bytes_read = io->ops->read(io, buffer, sizeof(buffer));
//     if (bytes_read < 0) {
//         kprintf("vioblk_read failed with error %ld\n", bytes_read);
//     } else {
//         kprintf("vioblk_read succeeded, read %ld bytes.\n", bytes_read);
//     }
// }

// void test_vioblk_write(void) {
//     struct io_intf *io = &vioblk_dev->io_intf;
//     uint8_t buffer[512];
//     long bytes_written;

//     // Fill buffer with test data
//     for (int i = 0; i < sizeof(buffer); i++) {
//         buffer[i] = (uint8_t)i;
//     }

//     // Write 512 bytes to the device
//     bytes_written = io->ops->write(io, buffer, sizeof(buffer));
//     if (bytes_written < 0) {
//         kprintf("vioblk_write failed with error %ld\n", bytes_written);
//     } else {
//         kprintf("vioblk_write succeeded, wrote %ld bytes.\n", bytes_written);
//     }
// }

// void test_vioblk_ioctl(void) {
//     uint64_t len;
//     uint64_t pos;
//     uint64_t new_pos;
//     uint32_t blksz;
//     int result;
//     struct io_intf *io = &vioblk_dev->io_intf;

//     // Get device length
//     result = io->ops->ctl(io, IOCTL_GETLEN, &len);
//     if (result != 0) {
//         kprintf("vioblk_getlen failed with error %d\n", result);
//     } else {
//         kprintf("Device length: %llu bytes.\n", len);
//     }

//     // Get current position
//     result = io->ops->ctl(io, IOCTL_GETPOS, &pos);
//     if (result != 0) {
//         kprintf("vioblk_getpos failed with error %d\n", result);
//     } else {
//         kprintf("Current position: %llu bytes.\n", pos);
//     }

//     // Set new position
//     new_pos = 1024; // Move to byte offset 1024
//     result = io->ops->ctl(io, IOCTL_SETPOS, &new_pos);
//     if (result != 0) {
//         kprintf("vioblk_setpos failed with error %d\n", result);
//     } else {
//         kprintf("Set new position to %llu bytes.\n", new_pos);
//     }

//     // Get block size
//     result = io->ops->ctl(io, IOCTL_GETBLKSZ, &blksz);
//     if (result != 0) {
//         kprintf("vioblk_getblksz failed with error %d\n", result);
//     } else {
//         kprintf("Block size: %u bytes.\n", blksz);
//     }
// }

// void test_vioblk_close(void) {
//     struct io_intf *io = &vioblk_dev->io_intf;

//     // Close the device
//     io->ops->close(io);

//     kprintf("vioblk_close completed.\n");
// }


// /////////////////////////////////////////////////////////
// // void test_vioblk_isr(void) {
// //     // Simulate an interrupt
// //     vioblk_isr(VIRT0_IRQNO, vioblk_dev);

// //     kprintf("vioblk_isr executed.\n");
// // }
// ////////////////////////////////////////////////////////

//           main.c - Main function: runs shell to load executable
//          

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

//           end of kernel image (defined in kernel.ld)
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

//static void shell_main(struct io_intf * termio);

extern char _companion_f_start[];
extern char _companion_f_end[];


void main(void) {

    console_printf("Hello");
    struct io_lit memory_io;

    // Initialize the io_lit object with the buffer
    struct io_intf * io = iolit_init(&memory_io, _companion_f_start, _companion_f_end - _companion_f_start);

    //fs_mount mounts the provided io_intf as a filesystem (if valid)
    fs_mount(io);

    char * temp = "hello";

    // demonstrates file being opened with fs_open, populating file_t struct array
    fs_open(temp, &io);

    void* tempBuff = kmalloc(39040);
    //demonstrate file being read in entirety with fs_read
    io -> ops -> read(io, tempBuff, 39040);

    // somehow print tempBuff
    char * display = (char *) tempBuff;
    console_printf(display);

    io -> ops -> ctl(io, 4, 0);

    //void * otherBuff = kmalloc(8);
    //otherBuff = "NEW INFO";
    /*
    otherBuff[0] = "N";
    otherBuff[1] = "E";
    otherBuff[2] = "W";
    otherBuff[3] = " ";
    otherBuff[4] = "I";
    otherBuff[5] = "N";
    otherBuff[6] = "F";
    otherBuff[7] = "O";
    */

    // demonstrate use of fs_write to overwrite the contents of the file

    /*

    io -> ops -> write(io, otherBuff, 1);

    // Need to show that contents were overwritten, maybe read entire file in again and print in again?
    
    io -> ops -> read(io, tempBuff, 39040);

    char * newDisplay = (char *) tempBuff;
    console_printf(newDisplay);

    // demonstrates control functions

    //get length
    io -> ops -> ctl(io, 1, 0);

    //get position
    io -> ops -> ctl(io, 3, 0);

    //set position
    
    io -> ops -> ctl(io, 4, (void *) 2);
    io -> ops -> ctl(io, 3, 0);

    // get block size
    io -> ops -> ctl(io, 6, 0);
    */

    kfree(tempBuff);

    ioclose(io);

    //-----------------------------------------------------------

    /*
    struct io_intf * termio;
    struct io_intf * blkio;
    void * mmio_base;
    int result;
    int i;

    console_init();
    intr_init();
    devmgr_init();
    thread_init();
    timer_init();

    heap_init(_kimg_end, (void*)USER_START);

    //           Attach NS16550a serial devices

    for (i = 0; i < 2; i++) {
        mmio_base = (void*)UART0_IOBASE;
        mmio_base += (UART1_IOBASE-UART0_IOBASE)*i;
        uart_attach(mmio_base, UART0_IRQNO+i);
    }
    
    //           Attach virtio devices

    for (i = 0; i < 8; i++) {
        mmio_base = (void*)VIRT0_IOBASE;
        mmio_base += (VIRT1_IOBASE-VIRT0_IOBASE)*i;
        virtio_attach(mmio_base, VIRT0_IRQNO+i);
    }

    intr_enable();
    timer_start();

   result = device_open(&blkio, "blk", 0);

    if (result != 0)
        panic("device_open failed");
    
    result = fs_mount(blkio);

    debug("Mounted blk0");

    if (result != 0)
        panic("fs_mount failed");

    //           Open terminal for trek

    result = device_open(&termio, "ser", 1);

    if (result != 0)
        panic("Could not open ser1");
    
    shell_main(termio);
    */
}
/*
void shell_main(struct io_intf * termio_raw) {
    struct io_term ioterm;
    struct io_intf * termio;
    void (*exe_entry)(struct io_intf*);
    struct io_intf * exeio;
    char cmdbuf[9];
    int tid;
    int result;

    termio = ioterm_init(&ioterm, termio_raw);

    ioputs(termio, "Enter executable name or \"exit\" to exit");
    

    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));

        if (cmdbuf[0] == '\0')
            continue;

        if (strcmp("exit", cmdbuf) == 0)
            return;
        
        result = fs_open(cmdbuf, &exeio);

        if (result < 0) {
            if (result == -ENOENT)
                ioprintf(termio, "%s: File not found\n", cmdbuf);
            else
                ioprintf(termio, "%s: Error %d\n", cmdbuf, -result);
            continue;
        }

        debug("Calling elf_load(\"%s\")", cmdbuf);

        result = elf_load(exeio, &exe_entry);

        debug("elf_load(\"%s\") returned %d", cmdbuf, result);

        if (result < 0) {
            ioprintf(termio, "%s: Error %d\n", -result);
        
        } else {
            tid = thread_spawn(cmdbuf, (void*)exe_entry, termio_raw);

            if (tid < 0)
                ioprintf(termio, "%s: Error %d\n", -result);
            else
                thread_join(tid);
        }

        ioclose(exeio);
    }
}
*/

