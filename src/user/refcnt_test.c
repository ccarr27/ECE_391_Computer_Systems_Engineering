#include "syscall.h"
#include "string.h"

void main(void) {
    int result;

    result = _fsopen(1, "hello");

    if (result < 0) {
            _msgout("_fsopen failed");
            _exit();
        }

    result = _fork();

    if(result != 0)
    {
        // if this is parent...
        _close(1);
    }
    else
    {
       char tempBuff[5];
        readRes = _read(1, tempBuff, sizeof(tempBuff));
    if (readRes < 0) {
        _msgout("_read not working");
    }
    else{
        _msgout("_read worked");
    }

    _msgout(tempBuff);

    int setPos = 0;
    int otherCheck = _ioctl(1, 4, &setPos); // Set pos
        if (otherCheck == setPos) {
        _msgout("ioctl works");
    }
    else{
        _msgout("ioctl does not work");
    }

    const char * const greeting = "Hello, world!\r\n";
    size_t slen;

    slen = strlen(greeting);
    int writeRes = _write(1, greeting, slen);
    if (writeRes < 0) {
        _msgout("_write not working");
    }
    else{
        _msgout("_write worked");
    }

    _close();
    _exit();

    }

    _exit();

    }
