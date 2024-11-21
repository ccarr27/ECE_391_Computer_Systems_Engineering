// main.c - Main function: runs shell to load executable
//

#ifdef MAIN_TRACE
#define TRACE
#endif

#ifdef MAIN_DEBUG
#define DEBUG
#endif

#define INIT_PROC "init0" // name of init process executable

#include "console.h"
#include "thread.h"
#include "device.h"
#include "uart.h"
#include "timer.h"
#include "intr.h"
#include "memory.h"
#include "heap.h"
#include "virtio.h"
#include "halt.h"
#include "elf.h"
#include "fs.h"
#include "string.h"
#include "process.h"
#include "config.h"



void main(void) {

    memory_init();

    /*
    Trying to test virtual memory 
    */
   
    console_printf("testing alloc page \n");

    void * test = memory_alloc_page();

    console_printf("testing free page \n");

    console_printf("alloc page before free %d \n", test);

    memory_free_page(&test);

    console_printf("alloc page after free %d \n", test);  
}
