
// //           main.c - Main function: runs shell to load executable
// //          

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

// //           end of kernel image (defined in kernel.ld)
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

// //static void shell_main(struct io_intf * termio);

// extern char _companion_f_start[];
// extern char _companion_f_end[];


// void main(void) {

//     console_printf("hello");
//     /*
//     struct io_lit memory_io;

//     // Initialize the io_lit object with the buffer
//     struct io_intf * io = iolit_init(&memory_io, _companion_f_start, _companion_f_end - _companion_f_start);

//     //fs_mount mounts the provided io_intf as a filesystem (if valid)
//     fs_mount(io);

//     char * temp = "hello";

//     // demonstrates file being opened with fs_open, populating file_t struct array
//     fs_open(temp, &io);

//     void* tempBuff = kmalloc(39040);
//     //demonstrate file being read in entirety with fs_read
//     io -> ops -> read(io, tempBuff, 39040);

//     // somehow print tempBuff
//     char * display = (char *) tempBuff;
//     console_printf(display);

//     io -> ops -> ctl(io, 4, 0);

//     //void * otherBuff = kmalloc(8);
//     //otherBuff = "NEW INFO";
    
//     otherBuff[0] = "N";
//     otherBuff[1] = "E";
//     otherBuff[2] = "W";
//     otherBuff[3] = " ";
//     otherBuff[4] = "I";
//     otherBuff[5] = "N";
//     otherBuff[6] = "F";
//     otherBuff[7] = "O";
    

//     // demonstrate use of fs_write to overwrite the contents of the file

    

//     io -> ops -> write(io, otherBuff, 1);

//     // Need to show that contents were overwritten, maybe read entire file in again and print in again?
    
//     io -> ops -> read(io, tempBuff, 39040);

//     char * newDisplay = (char *) tempBuff;
//     console_printf(newDisplay);

//     // demonstrates control functions

//     //get length
//     io -> ops -> ctl(io, 1, 0);

//     //get position
//     io -> ops -> ctl(io, 3, 0);

//     //set position
    
//     io -> ops -> ctl(io, 4, (void *) 2);
//     io -> ops -> ctl(io, 3, 0);

//     // get block size
//     io -> ops -> ctl(io, 6, 0);
    

//     //kfree(tempBuff);

//     //ioclose(io);
//     */
// }

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

static void shell_main(struct io_intf * termio);

void main(void) {
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
    char buffer [512];

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

    uint64_t length;
    uint64_t pos;

    (blkio->ops)->ctl(blkio, IOCTL_GETLEN, &length );

    kprintf("length: %d\n", length);


    for(int j=0; j<50; j++){

    kprintf("j: %d\n",j);

    blkio->ops->read(blkio, buffer, 512);

    for (int i =0; i <512;i++){
        kprintf("%d",buffer[i]);
    }
    kprintf("\n");
    (blkio->ops)->ctl(blkio, IOCTL_GETPOS, &pos );
    kprintf("pos: %d\n", pos);


    kprintf("\n\n");
    }

}
