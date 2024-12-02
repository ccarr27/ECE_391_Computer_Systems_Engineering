#include "excp.h"

struct trap_frame {
    uint64_t x[32];   
    uint64_t sstatus; 
    uint64_t sepc;    
};

trap_frame temp_frame = {
    .x = {0},          
    .sstatus = 0x0,    
    .sepc = 0x80000000 
};


umode_excp_handler(8, )


// Example testing function
void test_cat_function() {
    // Simulate invoking the `cat` function using the trap frame
    temp_frame.x[10] = (uint64_t)"testfile.txt"; // Set an argument register (e.g., x10/a0) to the filename

    // Call the `cat` function and handle the trap frame
    cat((char*)temp_frame.x[10]);

    // Simulate a return from the function
    temp_frame.sepc += 4; // Increment the program counter as if the instruction was executed
}