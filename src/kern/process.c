// process.c - user process
//

#include "process.h"
#include "memory.h"
#include "thread.h"
#include "elf.h"
#include "intr.h"
#include "csr.h"
#include "heap.h"
#include "console.h"
// #include "syscall.h"




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

/*
void procmgr_init(void)

Inputs: None

Outputs: None

Effects: Creates the global main_proc process retroactively

Description: This function creates the initial process. It will always have an ID of 0, because it will always be the first process.
We can then assign its thread ID, mtag, and set the process of it using the corresponding functions below. 

*/

void procmgr_init(void)
{
    main_proc.id = 0;       // init process always assigned process ID 0

    main_proc.tid = running_thread();       // Assign thread ID

    main_proc.mtag = active_memory_space();     // Set mtag from memory function

    thread_set_process(main_proc.tid, &main_proc);      // Creates process 0 around main thread and address space

}

/*
int process_exec(struct io_intf * exeio)

Inputs: exeio - The I/O interface that we want to create a process around

Outputs: Returns 0 (won't get here)

Effects: process_exec has three main requirements. It first unmaps all virtual memory mappings belonging to other user processes. It then
creates a 2nd level root page table (for CP3). Then, it loads the executable using elf_load, before finally jumping to user mode with the desired upc and usp. 

Description: process_exec allows us to run any program with an io interface. After reading the file using elf_load, we can run the executable by
jumping to user mode with the entry pointer as the upc, which allows the jump_to_user function to return to the beginning of the program we want to run.

*/

// Executes program referred to by I/O interface
int process_exec(struct io_intf * exeio)
{


    /*
    (a) First any virtual memory mappings belonging to other user processes should be unmapped.
    
    (b) Then a fresh 2nd level (root) page table should be created and initialized with the default map-
    pings for a user process. (This is not required for Checkpoint 2, as in Checkpoint 2 any user
    process will live in the ”main” memory space.)
    
    (c) Next the executable should be loaded from the I/O interface provided as an argument into the
    mapped pages. (Hint: elf load)
    
    (d) Finally, the thread associated with the process needs to be started in user-mode. (Hint: An as-
    sembly function in thrasm.s would be useful here)
    */
   
// 1. Any virtual memory mappings belonging to other user processes should be unmapped
    memory_unmap_and_free_user();        //NEED IT EVENTUALLY

// Not for CP2, but may later need to create new root table and initalize with default mapping for a user process

    // struct process * curr = current_process();
    // int s = intr_disable();
    // curr->mtag = memory_space_clone(0);
    // memory_space_switch(curr->mtag);
    // asm inline ("sfence.vma" ::: "memory");
    // intr_restore(s);


// 2. Executable loaded from I/O interface provided as argument into the mapped pages

    void (*exe_entry)(void);
    //void (*exe_entry)(struct io_intf*);
    int loaded = elf_load(exeio, &exe_entry);
    if(loaded < 0)
    {
        console_printf("Elf load failed return was: %d \n", loaded);
        // panic("elf_load fail");
    }
    //elf_load

// 3. Thread associated with process needs to be started in user-mode (assembly function in thrasm.s would be helpful)
    // usp = user stack pointer, upc = user program counter

    // Thread context stores stack pointer for thread
    //

    // interrupts must be disabled when setting up jump
    
    //console_printf("we get past elf load\n");

    console_printf("file: %s line:%d \n", __FILE__, __LINE__);

    int interrupt = intr_disable();
    
    thread_jump_to_user(USER_STACK_VMA, (uintptr_t) exe_entry); //USP, UPC will have to change -> Should now be okay
    intr_restore(interrupt);
    console_printf("file: %s line:%d \n", __FILE__, __LINE__);
    return 0;
}

/*
void process_exit(void)

Inputs: None

Outputs: None

Effects: Closes the current process and cleans up the resources used by it

Description: process_exit first gets the current process, reclaims the memory associated with it, and sets all of the iotab values to NULL
to ensure that all of the devices have been released.

*/

void process_exit(void)
{
    struct process * curr = current_process();
    // Anything associated with process at execution should be released

    memory_space_reclaim(); // Releases process memory space

    // console_printf("file: %s line:%d \n", __FILE__, __LINE__);

    for(int x = 0; x < 16; x++)
    {
        if(curr -> iotab[x] != NULL)
        {
        if(curr -> iotab[x] -> ops != NULL)
        {
        ioclose(curr -> iotab[x]);      // Close all open IO interfaces
        }
        }
    }
    // console_printf("file: %s line:%d \n", __FILE__, __LINE__);

    thread_exit();          // Releases associated kernel thread?
}

/*
int process_fork(const struct trap_frame * tfr)
Inputs:
tfr - the parent trap frame we are looking at

Outputs:
y - an int which represents the return value from thread_fork_to_user,
which is the child thread ID in the parent, 0 in the child process

Effects:
process_fork sets up the child process, making sure to copy the iotab and memory space of the parent thread

Description:
Process fork creates a process that is a copy of the current process. It allocates space
for a new process, then copies over the iotab and memory space. It then calls
thread_fork_to_user to continue the forking process.
*/


int process_fork(const struct trap_frame * tfr)
{
    // Copies memory space, file descriptor table, calls thread_fork_to_user()
    
    // Copy all io tab pointers
    
    //struct io_intf * iotab[PROCESS_IOMAX];


    //get the process id
    struct process * new_proc = kmalloc(sizeof(struct process));
    
    for(int x = 0; x < NPROC; x++)
    {
        if(!proctab[x])
        {
            proctab[x] = new_proc;
            new_proc->id = x;
            break;
        }
    }

    /*
    main_proc.tid = running_thread();       // Assign thread ID

    main_proc.mtag = active_memory_space();     // Set mtag from memory function

    thread_set_process(main_proc.tid, &main_proc);      // Creates process 0 around main thread and address space
    */

    // Will assign new_proc->tid in thread fork to user
    for(int x = 0; x < PROCESS_IOMAX; x++)
    {
        new_proc->iotab[x] = current_process() -> iotab[x];
        
        if(new_proc->iotab[x] != NULL)
        {
            new_proc->iotab[x] -> refcnt += 1;
        }
    }

    new_proc->mtag = memory_space_clone(0);
    int s = intr_disable();
    memory_space_switch(new_proc->mtag);
    asm inline ("sfence.vma" ::: "memory");
    intr_restore(s);

    // console_printf("parent mtag: %llx, child_proc mtag: %llx", csrr_satp(), new_proc->mtag);    
    
    int y = 0;
    //thread_fork_to_user(&new_proc, tfr);

    // Don't need to memcpy data inside global pages
    // Usually leaves only user page data to be copied
    // Alloc physical page from kernel range as backinh physical page for new clone
    return y;
}