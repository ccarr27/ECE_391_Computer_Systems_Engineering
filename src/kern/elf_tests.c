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
extern char _companion_f_start[];
extern char _companion_f_end[];

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

    struct io_lit * lit = kmalloc(sizeof(struct  io_lit));
    console_printf("start addr %x \n", _companion_f_start);
    console_printf("end addr %x \n", _companion_f_end);
    blkio = iolit_init(lit, _companion_f_start, (size_t)(_companion_f_end- _companion_f_start));
    void (*exe_entry)(struct io_intf*);
    struct io_intf * exeio;


    console_printf("we got right before elf_load \n");
    result = elf_load(&(lit->io_intf) ,  &exe_entry);

    console_printf("elf return result: %d \n", result);

    // FS TESTING

    kfree(lit);
    kfree(blkio);
    console_printf("hello \n");

    struct io_lit * memory_io_fs = kmalloc(sizeof(struct  io_lit));

    // Initialize the io_lit object with the buffer
    //void * tempBug;
    //tempBug = kmalloc(1);
    struct io_intf * io_fs;
    io_fs = iolit_init(memory_io_fs, _companion_f_start, (size_t)(_companion_f_end - _companion_f_start));
    //io_fs -> ops -> read(io_fs, tempBug, 1);
    int testMount;
    //fs_mount mounts the provided io_intf as a filesystem (if valid)
    testMount = fs_mount(io_fs);
    console_printf("testing fs_mount: %d \n", testMount);
    struct io_intf ** forOpen = kmalloc(sizeof(struct io_intf));
    int open_result = fs_open("hello", forOpen);
    //struct io_lit * tempIO = kmalloc(sizeof(struct io_lit));
    //char * charTemp;
    //charTemp = kmalloc(1);
    //charTemp = "hello";
    //int open_result = fs_open(charTemp, io_fs);
    console_printf("testing fs_open: %d \n", open_result);

    void* tempBuff = kmalloc(39040);
    //demonstrate file being read in entirety with fs_read
    //int testRead = io_fs -> ops -> read(io_fs, tempBuff, 39040);
    int testRead = ioread(*forOpen, tempBuff, 39040);
    console_printf("test fs_read: %d \n", testRead);
    //console_printf("test buffer: %s \n", tempBuff);
    

//////////////////////////////////////////////////////////////////////////////////////////
    // result = device_open(&blkio, "blk", 0);

    // if (result != 0)
    //     panic("device_open failed");
    
    // result = fs_mount(blkio);

    // debug("Mounted blk0");

    // if (result != 0)
    //     panic("fs_mount failed");

    // //           Open terminal for trek

    // result = device_open(&termio, "ser", 1);

    // if (result != 0)
    //     panic("Could not open ser1");
    
    // shell_main(termio);
}

// void shell_main(struct io_intf * termio_raw) {
//     struct io_term ioterm;
//     struct io_intf * termio;
//     void (*exe_entry)(struct io_intf*);
//     struct io_intf * exeio;
//     char cmdbuf[9];
//     int tid;
//     int result;

//     termio = ioterm_init(&ioterm, termio_raw);

//     ioputs(termio, "Enter executable name or \"exit\" to exit");
    

//     for (;;) {
//         ioprintf(termio, "CMD> ");
//         ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));

//         if (cmdbuf[0] == '\0')
//             continue;

//         if (strcmp("exit", cmdbuf) == 0)
//             return;
        
//         result = fs_open(cmdbuf, &exeio);

//         if (result < 0) {
//             if (result == -ENOENT)
//                 ioprintf(termio, "%s: File not found\n", cmdbuf);
//             else
//                 ioprintf(termio, "%s: Error %d\n", cmdbuf, -result);
//             continue;
//         }

//         debug("Calling elf_load(\"%s\")", cmdbuf);

//         result = elf_load(exeio, &exe_entry);

//         debug("elf_load(\"%s\") returned %d", cmdbuf, result);

//         if (result < 0) {
//             ioprintf(termio, "%s: Error %d\n", -result);
        
//         } else {
//             tid = thread_spawn(cmdbuf, (void*)exe_entry, termio_raw);

//             if (tid < 0)
//                 ioprintf(termio, "%s: Error %d\n", -result);
//             else
//                 thread_join(tid);
//         }

//         ioclose(exeio);
//     }
// }
