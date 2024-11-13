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
// extern char _companion_f_start[];
// extern char _companion_f_end[];

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

// static void shell_main(struct io_intf * termio);

// void main(void) {
//     struct io_intf * termio;
//     struct io_intf * blkio;
//     void * mmio_base;
//     int result;
//     int i;

//     console_init();
//     intr_init();
//     devmgr_init();
//     thread_init();
//     timer_init();

//     heap_init(_kimg_end, (void*)USER_START);

//     //           Attach NS16550a serial devices

//     for (i = 0; i < 2; i++) {
//         mmio_base = (void*)UART0_IOBASE;
//         mmio_base += (UART1_IOBASE-UART0_IOBASE)*i;
//         uart_attach(mmio_base, UART0_IRQNO+i);
//     }
    
//     //           Attach virtio devices

//     for (i = 0; i < 8; i++) {
//         mmio_base = (void*)VIRT0_IOBASE;
//         mmio_base += (VIRT1_IOBASE-VIRT0_IOBASE)*i;
//         virtio_attach(mmio_base, VIRT0_IRQNO+i);
//     }

//     intr_enable();
//     timer_start();

//     struct io_lit * lit = kmalloc(sizeof(struct  io_lit));
//     console_printf("start addr %x \n", _companion_f_start);
//     console_printf("end addr %x \n", _companion_f_end);
//     blkio = iolit_init(lit, _companion_f_start, (size_t)(_companion_f_end- _companion_f_start));
//     void (*exe_entry)(struct io_intf*);
//     struct io_intf * exeio;


//     console_printf("we got right before elf_load \n");
//     result = elf_load(&(lit->io_intf) ,  &exe_entry);

//     console_printf("elf return result: %d \n", result);

//     // FS TESTING

//     kfree(lit);
//     kfree(blkio);
//     console_printf("hello \n");

//     struct io_lit * memory_io_fs = kmalloc(sizeof(struct  io_lit));

//     // Initialize the io_lit object with the buffer
//     //void * tempBug;
//     //tempBug = kmalloc(1);
//     struct io_intf * io_fs;
//     io_fs = iolit_init(memory_io_fs, _companion_f_start, (size_t)(_companion_f_end - _companion_f_start));
//     //io_fs -> ops -> read(io_fs, tempBug, 1);
//     int testMount;
//     //fs_mount mounts the provided io_intf as a filesystem (if valid)
//     testMount = fs_mount(io_fs);
//     console_printf("testing fs_mount: %d \n", testMount);
//     struct io_intf ** forOpen = kmalloc(sizeof(struct io_intf));
//     int open_result = fs_open("hello", forOpen);
//     //struct io_lit * tempIO = kmalloc(sizeof(struct io_lit));
//     //char * charTemp;
//     //charTemp = kmalloc(1);
//     //charTemp = "hello";
//     //int open_result = fs_open(charTemp, io_fs);
//     console_printf("testing fs_open: %d \n", open_result);

//     void* tempBuff = kmalloc(39040);
//     //demonstrate file being read in entirety with fs_read
//     //int testRead = io_fs -> ops -> read(io_fs, tempBuff, 39040);
//     int testRead = ioread(*forOpen, tempBuff, 39040);
//     console_printf("test fs_read: %d \n", testRead);
//     //console_printf("test buffer: %s \n", tempBuff);
    

// //////////////////////////////////////////////////////////////////////////////////////////
//     // result = device_open(&blkio, "blk", 0);

//     // if (result != 0)
//     //     panic("device_open failed");
    
//     // result = fs_mount(blkio);

//     // debug("Mounted blk0");

//     // if (result != 0)
//     //     panic("fs_mount failed");

//     // //           Open terminal for trek

//     // result = device_open(&termio, "ser", 1);

//     // if (result != 0)
//     //     panic("Could not open ser1");
    
//     // shell_main(termio);
// }

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

//////////////////////////////////////////////////////////////////////////////////////////

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
// extern char _companion_f_start[];
// extern char _companion_f_end[];

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

// // static void shell_main(struct io_intf * termio);

// void main(void) {
//     // struct io_intf * termio;
//     struct io_intf * blkio;
//     void * mmio_base;
//     int result;
//     int i;

//     console_init();
//     intr_init();
//     devmgr_init();
//     thread_init();
//     timer_init();

//     heap_init(_kimg_end, (void*)USER_START);

//     //           Attach NS16550a serial devices

//     for (i = 0; i < 2; i++) {
//         mmio_base = (void*)UART0_IOBASE;
//         mmio_base += (UART1_IOBASE-UART0_IOBASE)*i;
//         uart_attach(mmio_base, UART0_IRQNO+i);
//     }
    
//     //           Attach virtio devices

//     for (i = 0; i < 8; i++) {
//         mmio_base = (void*)VIRT0_IOBASE;
//         mmio_base += (VIRT1_IOBASE-VIRT0_IOBASE)*i;
//         virtio_attach(mmio_base, VIRT0_IRQNO+i);
//     }

//     intr_enable();
//     timer_start();

//     struct io_lit * lit = kmalloc(sizeof(struct  io_lit));
//     console_printf("start addr %x \n", _companion_f_start);
//     console_printf("end addr %x \n", _companion_f_end);
//     blkio = iolit_init(lit, _companion_f_start, (size_t)(_companion_f_end- _companion_f_start));
//     void (*exe_entry)(struct io_intf*);
//     // struct io_intf * exeio;


//     console_printf("we got right before elf_load \n");
//     result = elf_load(blkio,  &exe_entry);

//     console_printf("elf return result: %d \n", result);
//     console_printf("elf exe_entry: %lx \n", exe_entry);

//     // FS TESTING

//      kfree(lit);
//      kfree(blkio);
//      console_printf("hello \n");

//      struct io_lit * memory_io_fs = kmalloc(sizeof(struct  io_lit));

// //     // Initialize the io_lit object with the buffer

//      struct io_intf * io_fs;
//      io_fs = iolit_init(memory_io_fs, _companion_f_start, (size_t)(_companion_f_end - _companion_f_start));
// //     //io_fs -> ops -> read(io_fs, tempBug, 1);
//      int testMount;
// //     //fs_mount mounts the provided io_intf as a filesystem (if valid)
//      testMount = fs_mount(io_fs);
//      console_printf("testing fs_mount: %d \n", testMount);
//      struct io_intf ** forOpen = kmalloc(sizeof(struct io_intf )); // add **?
//      int open_result = fs_open("hello", forOpen);
//      console_printf("testing fs_open: %d \n", open_result);

//     void * otherBuff = kmalloc(3);
//     otherBuff = "bob";
//     iowrite(*forOpen, otherBuff, 3);
//     console_printf("testing io result %d \n", ioctl(*forOpen, 3, NULL));

//     console_printf("testing get length: %d \n", ioctl(*forOpen, 1, NULL));
//     console_printf("testing get position: %d \n", ioctl(*forOpen, 3, NULL));
//     console_printf("testing set position: %d \n", ioctl(*forOpen, 4, 0));
//     console_printf("testing get position: %d \n", ioctl(*forOpen, 3, NULL));
//      console_printf("testing get block size: %d \n", ioctl(*forOpen, 6. NULL));
// /*

// Testing fs_read
//      void* tempBuff = kmalloc(39040);s
// //     //demonstrate file being read in entirety with fs_read
//     int testRead = ioread(*forOpen, tempBuff, 39040);
//     console_printf("test fs_read: %d \n", testRead);
//     console_printf("test buffer: %s \n", tempBuff);

//     console_printf("testing reading full file by position changing %d \n", ioctl(*forOpen, 3, NULL));

//     ioctl(*forOpen, 4, 0);

//     console_printf("testing%d \n", ioctl(*forOpen, 3, NULL));
// */


// //     //struct io_lit * tempIO = kmalloc(sizeof(struct io_lit));
// //     //char * charTemp;
// //     //charTemp = kmalloc(1);
// //     //charTemp = "hello";
// //     //int open_result = fs_open(charTemp, io_fs);


// //     void* tempBuff = kmalloc(39040);
// //     //demonstrate file being read in entirety with fs_read
// //     //int testRead = io_fs -> ops -> read(io_fs, tempBuff, 39040);
// //     int testRead = ioread(*forOpen, tempBuff, 39040);
// //     console_printf("test fs_read: %d \n", testRead);
// //     //console_printf("test buffer: %s \n", tempBuff);
// }



////////////////////////////////////////////////////////////////////////////////////////////////

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
    termio = iolit_init(lit, _companion_f_start, (size_t)(_companion_f_end- _companion_f_start));

     intr_enable();
    timer_start();
    /*

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


    
    // shell_main(termio);

    console_printf("start addr %x \n", _companion_f_start);
    console_printf("end addr %x \n", _companion_f_end);
    blkio = iolit_init(lit, _companion_f_start, (size_t)(_companion_f_end- _companion_f_start));
    void (*exe_entry)(struct io_intf*);
    // struct io_intf * exeio;


    console_printf("we got right before elf_load \n");
    result = elf_load(blkio,  &exe_entry);

    console_printf("elf return result: %d \n", result);
    console_printf("elf exe_entry: %lx \n", exe_entry);
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


    /*
    console_printf("start addr %x \n", _companion_f_start);
    console_printf("end addr %x \n", _companion_f_end);
    blkio = iolit_init(lit, _companion_f_start, (size_t)(_companion_f_end- _companion_f_start));
    void (*exe_entry)(struct io_intf*);
    // struct io_intf * exeio;


    console_printf("we got right before elf_load \n");
    result = elf_load(blkio,  &exe_entry);

    console_printf("elf return result: %d \n", result);
    console_printf("elf exe_entry: %lx \n", exe_entry);
    */


    

    // Testing kfs.c

    console_printf("\n");
    console_printf("Testing kfs.c");
    console_printf("\n");

    // Initialize io_lit object to use for testing the file system

    struct io_lit * memory_io_fs = kmalloc(sizeof(struct  io_lit));                 
    struct io_intf * io_fs;
    io_fs = iolit_init(memory_io_fs, _companion_f_start, (size_t)(_companion_f_end - _companion_f_start));

    // Test fs_mount on valid io_intf
    console_printf("\n");
    console_printf("Testing fs_mount \n");
    console_printf("\n");

    //Test fs_mount on invalid io_intf
    

    struct io_lit * null_io_fs = kmalloc(sizeof(struct  io_lit));                 
    struct io_intf * io_null;
    void * nullBuf = kmalloc(1);
    nullBuf = " ";
    io_null = iolit_init(null_io_fs, nullBuf, 1);

    int testMount2;
    testMount2 = fs_mount(io_null);

    // Error code for invalid io_intf

    console_printf("testing fs_mount on invalid io: %d \n", testMount2);

    kfree(nullBuf);
    kfree(io_null);
    kfree(null_io_fs);
    

    int testMount;
    testMount = fs_mount(io_fs);
 
    // fs_mount returns 0 if the mount is successful

    console_printf("testing fs_mount on valid io: %d \n", testMount);



    console_printf("\n");
    console_printf("Testing fs_open \n");
    console_printf("\n");

    // Test fs_open with invalid file (name that doesn't exist in filesystem)

    struct io_intf ** invalidOpen = kmalloc(sizeof(struct io_intf ));
    int invalidResult = fs_open("random", invalidOpen);
    console_printf("testing fs_open with invalid file: %d \n", invalidResult);

    kfree(invalidOpen);

    // Test fs_open with valid file

    struct io_intf ** forOpen = kmalloc(sizeof(struct io_intf ));
    int open_result = fs_open("hello", forOpen);
    console_printf("testing fs_open with valid file ('hello'): %d \n", open_result);

    console_printf("\n");
    console_printf("Testing fs_ioctl \n");
    console_printf("\n");

    // Test get_length  
    int getLength;
    ioctl(*forOpen, IOCTL_GETLEN, &getLength);

    console_printf("Length of file: %d \n", getLength);

    // Test get_position
    int getPosition;
    ioctl(*forOpen, IOCTL_GETPOS, &getPosition);

    console_printf("Position of file: %d \n", getPosition);

    // Test set_position
    int setPosition = 20;
    ioseek(*forOpen, setPosition);
    ioctl(*forOpen, IOCTL_GETPOS, &getPosition);

    console_printf("Position of file after setting position: %d \n", getPosition);

    // Set position back to 0 to read/write
    setPosition = 0;
    ioseek(*forOpen, setPosition);
    ioctl(*forOpen, IOCTL_GETPOS, &getPosition);
    console_printf("Position of file after resetting position back to 0: %d \n", getPosition);

    // Test get_blksz

    int blockSize;
    ioctl(*forOpen, IOCTL_GETBLKSZ, &blockSize);
    console_printf("Block size of filesystem: %d \n", blockSize);

    console_printf("\n");
    console_printf("Testing fs_read \n");
    console_printf("\n");

    // Testing file read in entirety using fs_read
    ioctl(*forOpen, IOCTL_GETLEN, &getLength);

    void * tempBuff;
    tempBuff = kmalloc(39040);
    long validRead = ioread_full(*forOpen, tempBuff, getLength);

    console_printf("Number of bytes read, should be equal to length: %d \n", validRead);

    char * p = tempBuff;

    console_printf("Original first value in file %d \n", *p);
    p += 1;

    console_printf("Original second value in file %d \n", *p);
    p += 1;

    console_printf("Original third value in file %d \n", *p);
    p += 1;

    console_printf("Original fourth value in file %d \n", *p);

    p += 1;

    console_printf("Original fifth value in file %d \n", *p);
    p += 1;

    console_printf("Original sixth value in file %d \n", *p);

    kfree(tempBuff);
    kfree(p);

    console_printf("\n");
    console_printf("Testing fs_write \n");
    console_printf("\n");
    

    // Testing fs_write to overwrite contents of file

    setPosition = 0;
    ioseek(*forOpen, setPosition);
    ioctl(*forOpen, IOCTL_GETPOS, &getPosition);
    console_printf("Position of file after resetting position back to 0 to prepare for fs_write: %d \n", getPosition);
    
    int otherBuff[30] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

    long validWrite = iowrite(*forOpen, otherBuff, sizeof(otherBuff));

    console_printf("Number of bytes written: %d \n", validWrite);

    setPosition = 0;
    ioseek(*forOpen, setPosition);

    ioctl(*forOpen, IOCTL_GETLEN, &getLength);

    

    void * thirdBuff;
    thirdBuff = kmalloc(getLength);

    long otherValidRead = ioread(*forOpen, thirdBuff, 120);

    console_printf("Return value from reading 120 bytes %d \n", otherValidRead);

    char * a = thirdBuff;

    console_printf("New first value in file %d \n", *a);
    a += 1;

    console_printf("New second value in file %d \n", *a);
    a += 1;

    console_printf("New third value in file %d \n", *a);
    a += 1;

    console_printf("New fourth value in file %d \n", *a);

    a += 1;

    console_printf("New fifth value in file %d \n", *a);
    a += 1;

    console_printf("New sixth value in file %d \n", *a);

    kfree(otherBuff);
    kfree(p);
    kfree(thirdBuff);

    setPosition = 0;
    ioseek(*forOpen, setPosition);
    ioctl(*forOpen, IOCTL_GETPOS, &getPosition);
    console_printf("Resetting position for future operations: %d \n", getPosition);


}