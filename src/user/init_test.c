
#include "syscall.h"
#include "string.h"

void main(void) {

// ALL SYSTEM CALLS SHOULD BE WORKING (need to double check exit and exec)
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

    int thirdRes = _fsopen(-1, "hello");

        if (thirdRes < 0) {
        _msgout("_fsopen failed");
        return;
    }
    else{
        _msgout("_fsopen with next available fd works");
    }

    // Testing sysclose

    int close = _close(0);
      if (close < 0) {
        _msgout("_close failed");
        return;
    }
    else{
        _msgout("_close works");
    }

    // Testing that file was properly closed

    char tempBuff[5];

    int readRes = _read(0, tempBuff, sizeof(tempBuff));
    if (readRes < 0) {
        _msgout("_close properly closed file");
    }
    else{
        _msgout("_close did not properly close file");
    }

    // Testing reading from the filesystem

    readRes = _read(1, tempBuff, sizeof(tempBuff));
    if (readRes < 0) {
        _msgout("_read not working");
    }
    else{
        _msgout("_read worked");
    }

    _msgout(tempBuff);

    // Testing sysioctl
    int checkPos;
    int checking = _ioctl(1, 3, &checkPos); // Check pos

    if (checking == 5) {
        _msgout("ioctl works");
    }
    else{
        _msgout("ioctl does not work");
    }

    //_ioctl(1, 3, &checkPos);

    //_msgout((char *) checkPos);

    int setPos = 0;
    int otherCheck = _ioctl(1, 4, &setPos); // Set pos
        if (otherCheck == setPos) {
        _msgout("ioctl works");
    }
    else{
        _msgout("ioctl does not work");
    }
    //_ioctl(1, 4, &setPos);

    //_msgout((char *) checkPos);

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

    setPos = 0;
    otherCheck = _ioctl(1, 4, &setPos); // Set pos

    if (otherCheck == setPos) {
        _msgout("ioctl works");
    }
    else{
        _msgout("ioctl does not work");
    }

    char thirdBuff[slen];
        readRes = _read(1, thirdBuff, slen);
    if (readRes < 0) {
        _msgout("_read not working");
    }
    else{
        _msgout("_read worked");
    }

    _msgout(thirdBuff);



    // Testing writing to the filesysten
    
    /*
    char otherBuff[5] = {'H', 'e', 'l', 'l', 'o'};
    int writeRes = _write(1, otherBuff, sizeof(otherBuff));
        if (writeRes < 0) {
        _msgout("_write not working");
    }
    else{
        _msgout("_write worked");
    }

    char thirdBuff[5];
        readRes = _read(1, thirdBuff, sizeof(thirdBuff));
    if (readRes < 0) {
        _msgout("_read not working");
    }
    else{
        _msgout("_read worked");
    }

    _msgout(thirdBuff);
    */

    /*

    Need memory_space_reclaim to work for this function to work

    // Testing sysexit
    _exit();
    */
    
}

