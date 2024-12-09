
#include "syscall.h"
#include "string.h"

void main(void) {

// ALL SYSTEM CALLS SHOULD BE WORKING (need to double check exit and exec)
    
    // Testing sysmsgout
    _msgout("Testing system calls pt 2: \n");
    

    int result = _devopen(0, "doesn't exist", 1);

    if (result < 0) {
        _msgout("_devopen failed");
        _exit();
    }

    result = _fsopen(1, "init1");
        if (result < 0) {
        _msgout("_fsopen failed");
        _exit();
    }
    else
    {
        _msgout("_fsopen works");
    }

    _msgout("Here if exit doesn't work");
    
}