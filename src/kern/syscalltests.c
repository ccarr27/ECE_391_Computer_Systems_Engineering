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
#include "scnum.h"


// struct trap_frame {
//     uint64_t x[32];   
//     uint64_t sstatus; 
//     uint64_t sepc;    
// };

// struct trap_frame temp_frame = {
//     .x = {0},          
//     .sstatus = 0x0,    
//     .sepc = 0x80000000 
// };


// int64_t sysmsgout(const char *message) {
//     console_printf("%s\n", message);
//     return 0;
// }

int main() {
    thread_init();
    struct trap_frame temp_frame = {0};

    char * testname = "msgout";

    // msgout

    switch (test):
        case("msgout"):
            const char *message = "Hello, World!";

            temp_frame.x[TFR_A7] = SYSCALL_MSGOUT;
            temp_frame.x[TFR_A0] = (uint64_t)message;

        case("exit"):
            



    

    syscall(&temp_frame);

    return 0;
}
