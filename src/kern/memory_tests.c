// // main.c - Main function: runs shell to load executable
// //

// #ifdef MAIN_TRACE
// #define TRACE
// #endif

// #ifdef MAIN_DEBUG
// #define DEBUG
// #endif

// #define INIT_PROC "init0" // name of init process executable

// #include "console.h"
// #include "thread.h"
// #include "device.h"
// #include "uart.h"
// #include "timer.h"
// #include "intr.h"
// #include "memory.h"
// #include "heap.h"
// #include "virtio.h"
// #include "halt.h"
// #include "elf.h"
// #include "fs.h"
// #include "string.h"
// #include "process.h"
// #include "config.h"


// void main(void) {
//     struct io_intf * initio;
//     struct io_intf * blkio;
//     void * mmio_base;
//     int result;
//     int i;

//     console_init();
//     memory_init();

//     intr_init();
//     devmgr_init();
//     thread_init();
//     procmgr_init();


//     // Attach NS16550a serial devices

//     for (i = 0; i < 2; i++) {
//         mmio_base = (void*)UART0_IOBASE;
//         mmio_base += (UART1_IOBASE-UART0_IOBASE)*i;
//         uart_attach(mmio_base, UART0_IRQNO+i);
//     }
    
//     // Attach virtio devices

//     for (i = 0; i < 8; i++) {
//         mmio_base = (void*)VIRT0_IOBASE;
//         mmio_base += (VIRT1_IOBASE-VIRT0_IOBASE)*i;
//         virtio_attach(mmio_base, VIRT0_IRQNO+i);
//     }

//     intr_enable();    


//     result = device_open(&blkio, "blk", 0);

//     if (result != 0)
//         panic("device_open failed");
    
//     result = fs_mount(blkio);

//     debug("Mounted blk0");


//     if (result != 0)
//         panic("fs_mount failed");

//     result = fs_open(INIT_PROC, &initio);

//     if (result < 0)
//         panic(INIT_PROC ": process image not found");

//     //at this point the _kimg_start and kimg_end are mapped


//     // 2. Executable loaded from I/O interface provided as argument into the mapped pages

//     void (*exe_entry)(void);
//     int loaded = elf_load(initio, &exe_entry);
//     if(loaded < 0)
//     {
//         console_printf("Elf load failed return was: %d", loaded);
//     }

//     //at this point we have successful mapping of user pages within USER_START and USER_END

    


//     console_printf("\n============ Test cases ============\n");

//     // TEST CASE: demonstrates successful paging implementation with repeated pointer 
//     //            arithmetic operations

//     console_printf("\nDemonstrating paging: \n");

//     void * allocatedPage1 = memory_alloc_and_map_page(USER_START_VMA+(2000 * PAGE_SIZE), PTE_R|PTE_W|PTE_U);
//     void * allocatedPage2 = memory_alloc_and_map_page(USER_START_VMA+(2002 * PAGE_SIZE), PTE_R|PTE_W|PTE_U);
//     void * allocatedPage3 = memory_alloc_and_map_page(USER_START_VMA+(2004 * PAGE_SIZE), PTE_R|PTE_W|PTE_U);

//     int * firstPage = (int *)allocatedPage1;
//     int * secondPage = (int *)allocatedPage2;
//     int * thirdPage = (int *)allocatedPage3;

//     *firstPage = 0x1234;
//     *secondPage = 0x5678;
//     *thirdPage = 0xfffe;

//     console_printf("First Page Value: 0x%x\n", *firstPage);
//     console_printf("Second Page Value: 0x%x\n", *secondPage);
//     console_printf("Third Page Initial Value: 0x%x\n", *thirdPage);

//     int * thirdPageModified = allocatedPage2 + PAGE_SIZE*2;
//     *thirdPageModified += 0x1;  

//     console_printf("Third Page Final Value: 0x%x\n", *thirdPage);

    

    
//     // TEST CASE: triggering various faults 

//     console_printf("\n\nTriggering faults:\n");
    
//     console_printf("\nWriting kernel page from user mode: \n"); // Test 1
//     long * testKernelMem1 = 0x8000b000;
//     long * testKernelMem2 = 0x8000c000;
//     *testKernelMem2 = 0xabcd;

//     console_printf("\nReading kernel page from user mode: \n"); // Test 2
//     long * testKernelMem = 0x8000c000;
//     int tmp = *testKernelMem;
//     console_printf("Memory read from %p: %d\n", testKernelMem, tmp);

//     console_printf("\nReading from a page without read permissions:\n"); // Test 3
//     void * noReadPage = memory_alloc_and_map_page(USER_START_VMA+(128 * PAGE_SIZE), PTE_W);
//     int * readPage = (int *)noReadPage;
//     int * testRead = *readPage;
//     console_printf("Read Processed\n");

//     console_printf("\nWriting to a page without write permissions:\n"); // Test 4
//     void * noWritePage = memory_alloc_and_map_page(USER_START_VMA+(256 * PAGE_SIZE), PTE_R);
//     int * writePage = (int *)noWritePage;
//     *writePage = 0xbbbb;
//     console_printf("Write Processed\n");

//     console_printf("\nExecuting instructions from a page without execute permissions:s\n"); // Test 5
//     void * noExecutePage = memory_alloc_and_map_page(USER_START_VMA+(290 * PAGE_SIZE), 0);
//     char * executePage = (char *)noExecutePage;
//     //write code to put and execute instructions from that page




    // memory_alloc_and_map_page(vma+ (x * PAGE_SIZE) , rwxug_flags);


// }
