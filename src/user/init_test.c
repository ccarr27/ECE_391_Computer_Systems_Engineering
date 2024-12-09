
#include "syscall.h"
#include "string.h"

void main(void) {

// ALL SYSTEM CALLS SHOULD BE WORKING (need to double check exit and exec)
    
    // Testing sysmsgout
    _msgout("Testing system calls: \n");

    // Testing sysdevopen
    // Testing sysfsopen

    int otherResult = _fsopen(0, "hello");

    if (otherResult < 0) {
        _msgout("_fsopen failed");
        return;
    }
    else{
        _msgout("_fsopen works");
    }

    const char * const greeting = "WORK!";
    size_t slen;

    slen = strlen(greeting);
    int writeRes = _write(0, greeting, slen);
    if (writeRes < 0) {
        _msgout("_write not working");
    }
    else{
        _msgout("_write worked");
    }

    int checkPos;
    int checking = _ioctl(0, 3, &checkPos); // Check pos

    if (checking == 5) {
        _msgout("get position IOCTL works");
    }
    else{
        _msgout("ioctl does not work");
    }

    int setPos = 0;
    int otherCheck = _ioctl(0, 4, &setPos); // Set pos
        if (otherCheck == setPos) {
        _msgout("set/get position IOCTL works");
    }
    else{
        _msgout("ioctl does not work");
    }

    char tempBuff[5];

    int readRes = _read(0, tempBuff, sizeof(tempBuff));
    if (readRes < 0) {
        _msgout("_read not working");
    }
    else{
        _msgout("_read worked");
    }

    _msgout(tempBuff);

    int close = _close(0);
      if (close < 0) {
        _msgout("_close failed");
        return;
    }
    else{
        _msgout("_close works");
    }

    readRes = _read(0, tempBuff, sizeof(tempBuff));
    if (readRes != 0) {
        _msgout("_close properly closed file");
    }
    else{
        _msgout("_close did not properly close file");
    }

    /*
    int result = _fsopen(1, "init0");
        if (result < 0) {
        _msgout("_fsopen failed");
        _exit();
    }
    else
    {
        _msgout("_fsopen works");
    }

    _exec(1);
    

    result = _devopen(0, "doesn't exist", 1);

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
*/
    
}

/*

#include "syscall.h"
#include "string.h"

void main(void) {
    int result;

    result = _fsopen(0, "inint0");

    if (result < 0) {
        _msgout("_fsopen failed");
        _exit();
    }

    _exec(1);
}

*/