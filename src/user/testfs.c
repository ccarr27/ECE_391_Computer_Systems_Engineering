#include "io.h"
#include "string.h"
#include "fs.h"

void main() {
    char buffer[128];  // A memory buffer that will act like a "file"
    struct io_lit memory_io;

    // Initialize the io_lit object with the buffer
    struct io_intf * io = iolit_init(&memory_io, buffer, sizeof(buffer));

    // Set up file system with proper name, so we can open the file with the specific name

    //fs_mount mounts the provided io_intf as a filesystem (if valid)
    fs_mount(io);

    char * temp = "file";

    // demonstrates file being opened with fs_open, populating file_t struct array
    fs_open(temp, &io);
    // Need to show file_t struct array (in kfs.raw?)


    void* tempBuff[39040];
    //demonstrate file being read in entirety with fs_read
    io -> ops -> read(io, tempBuff, 128);

    // somehow print tempBuff

    io -> ops -> ctl(io, 4, 0);

    void * otherBuff = 'NEW INFO';

    // demonstrate use of fs_write to overwrite the contents of the file

    io -> ops -> write(io, otherBuff, 8);

    // Need to show that contents were overwritten, maybe read entire file in again and print in again?
    io -> ops -> read(io, tempBuff, 128);

    // demonstrates control functions

    //get length
    io -> ops -> ctl(io, 1, 0);

    //get position
    io -> ops -> ctl(io, 3, 0);

    //set position
    io -> ops -> ctl(io, 4, 2);
    io -> ops -> ctl(io, 3, 0);

    // get block size
    io -> ops -> ctl(io, 6, 0);

    ioclose(io);
}