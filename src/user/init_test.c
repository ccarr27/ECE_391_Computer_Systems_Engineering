
#include "syscall.h"
#include "string.h"

void main(void) {

    // Testing sysmsgout
    _msgout("Testing system calls: \n");

    // Testing sysdevopen

    int result = _devopen(0, "ser", 1);

    if (result < 0) {
        _msgout("_devopen failed");
    }
    else
    {
        _msgout("_devopen works");
    }

    // Testing sysfsopen

    int otherResult = _fsopen(1, "trek");

    if (otherResult < 0) {
        _msgout("_fsopen failed");
        return;
    }
    else{
        _msgout("_fsopen works");
    }

    // Testing sysfsopen on next fd

    int thirdRes = _fsopen(-1, "init0");

        if (thirdRes < 0) {
        _msgout("_fsopen failed");
        return;
    }
    else{
        _msgout("_fsopen with next available fd works");
    }

    // Testing sysclose

    int close = _close(1);
      if (close < 0) {
        _msgout("_close failed");
        return;
    }
    else{
        _msgout("_close works");
    }

    // Testing that file was properly closed

    char tempBuff[5];

    int readRes = _read(1, tempBuff, sizeof(tempBuff));
    if (readRes < 0) {
        _msgout("_close properly closed file");
        return;
    }
    else{
        _msgout("_close did not properly close file");
    }


    /*

    Need memory_space_reclaim to work for this function to work

    // Testing sysexit
    _exit();
    */
    
}

