#include "excp.c"

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


int64_t sysmsgout(const char *message) {
    printf("%s\n", message);
    return 0;
}

int main() {
    struct trap_frame temp_frame = {0};
    const char *message = "Hello, World!";

    temp_frame.x[TFR_A7] = SYSCALL_MSGOUT;
    temp_frame.x[TFR_A0] = (uint64_t)message;

    syscall(&temp_frame);

    return 0;
}
