#include "vioblk.c"
#include "fs.h"
#include "io.h"
#include "virtio.h"
#include "mkfs.c"

// Create file_desc_t * struct
/*
uint64_t io_intf
uint64_t file_pos
uint64_t file_size
uint64_t inode
uint64_t flags
*/

boot_block_t boot;

typedef struct file_desc_t{
    struct io_intf * io_intf;
    uint64_t file_pos;
    uint64_t file_size;
    uint64_t inode;
    uint64_t flags;
} file_desc_t;
//typdef struct fileDescript file_desc_t;

// Defined already in fs.h
/*
int fs_mount(struct io_intf* io);
int fs_open(const char* name, struct io_intf** io);
*/

void fs_close(struct io_intf* io);
long fs_write(struct io_intf* io, const void* buf, unsigned long n);
long fs_read(struct io_intf* io, void * buf, unsigned long n);
int fs_ioct1(struct io_intf*, int cmd, void * arg);
int fs_getlen(file_desc_t* fd, void * arg);
int fs_getpos(file_desc_t* fd, void * arg);
int fs_setpos(file_desc_t* fd, void * arg);
int fs_getblksz(file_desc_t* fd, void * arg);

int fs_mount(struct io_intf* io)
{
    // boot = io -> ops -> read(io, ) # Read something to get boot_block from virtio

    // Look at virtio to see where to get setup info, then actually set it up
    return 0;
}

int fs_open(const char* name, struct io_intf** io)
{
    // If file name exists, checl that it isn't already open
    // If both of these are true, set the file to 'in-use' and instantiate the rest of the file members
    // Change io to io_intf that can be used with fs_read, fs_write, to read/write the specific file that was opened

    // Kernel should find next file struct and set up metadata for the file
    return 0;
}


int fs_getlen(file_desc_t* fd, void * arg)
{
    return fd -> file_size;
}

int fs_getpos(file_desc_t* fd, void * arg)
{
    return fd -> file_pos;
}

int fs_setpos(file_desc_t* fd, void * arg)
{
    fd -> file_pos = arg;
    return arg;
}

int fs_getblksz(file_desc_t* fd, void * arg)
{
    return fd -> io_intf -> ops -> ctl(fd -> io_intf, IOCTL_GETBLKSZ, &arg); //Gets block size of file?
}

