// process.c - user process
//

#include "process.h"
#include "memory.h"
#include "thread.h"
#include "elf.h"
#include "intr.h"
#include "csr.h"



#ifdef PROCESS_TRACE
#define TRACE
#endif

#ifdef PROCESS_DEBUG
#define DEBUG
#endif


// COMPILE-TIME PARAMETERS
//

// NPROC is the maximum number of processes

#ifndef NPROC
#define NPROC 16
#endif

// INTERNAL FUNCTION DECLARATIONS
//

// INTERNAL GLOBAL VARIABLES
//

#define MAIN_PID 0

// The main user process struct

static struct process main_proc;

// A table of pointers to all user processes in the system

struct process * proctab[NPROC] = {
    [MAIN_PID] = &main_proc
};

// EXPORTED GLOBAL VARIABLES
//

char procmgr_initialized = 0;

// EXPORTED FUNCTION DEFINITIONS
//

void procmgr_init(void)
{
    main_proc.id = 0;       // init process always assigned process ID 0

    main_proc.tid = running_thread();       // Assign thread ID

    main_proc.mtag = active_memory_space();     // Set mtag from memory function

    thread_set_process(main_proc.tid, &main_proc);      // Creates process 0 around main thread and address space

    // Don't think we need to set iotab, syscalls will do that

}

// Executes program referred to by I/O interface
int process_exec(struct io_intf * exeio)
{
// 1. Any virtual memory mappings belonging to other user processes should be unmapped
    memory_unmap_and_free_user();

// Not for CP2, but may later need to create new root table and initalize with default mapping for a user process

// 2. Executable loaded from I/O interface provided as argument into the mapped pages

    void (*exe_entry)(struct io_intf*);
    int loaded = elf_load(exeio, &exe_entry);
    if(loaded != 0)
    {
        panic("Elf load failed");
    }
    //elf_load

// 3. Thread associated with process needs to be started in user-mode (assembly function in thrasm.s would be helpful)
    // usp = user stack pointer, upc = user program counter

    // Thread context stores stack pointer for thread
    //

    // interrupts must be disabled when setting up jump

    int interrupt = intr_disable();
    thread_jump_to_user(0 , 0); //USP, UPC
    intr_restore(interrupt);
    return 0;
}

void process_exit(void)
{
    struct process * curr = current_process();
    // Anything associated with process at execution should be released

    memory_space_reclaim(); // Releases process memory space

    for(int x = 0; x < 16; x++)
    {
        if(curr -> iotab[x] -> ops != NULL) // May need to check if iotab[x] != NULL (depends on io_intf default value)
        {
        ioclose(curr -> iotab[x]);      // Close open IO interfaces
        }
    }

    thread_exit();          // Releases associated kernel thread?
}
