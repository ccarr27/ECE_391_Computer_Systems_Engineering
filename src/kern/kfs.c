#include "vioblk.c"
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

// Disk layout:
// [ boot block | inodes | data blocks ]

typedef struct dentry_t{
    char file_name[FS_NAMELEN];
    uint32_t inode;
    uint8_t reserved[28];
}__attribute((packed)) dentry_t; 

typedef struct boot_block_t{
    uint32_t num_dentry;
    uint32_t num_inodes;
    uint32_t num_data;
    uint8_t reserved[52];
    dentry_t dir_entries[63];
}__attribute((packed)) boot_block_t;

typedef struct inode_t{
    uint32_t byte_len;
    uint32_t data_block_num[1023];
}__attribute((packed)) inode_t;

typedef struct data_block_t{
    uint8_t data[FS_BLKSZ];
}__attribute((packed)) data_block_t;

typedef struct file_desc_t{
    struct io_intf * io_intf;
    uint64_t file_pos;
    uint64_t file_size;
    uint64_t inode;
    uint64_t flags;
} file_desc_t;

// Global variables we need to use

struct boot_block_t * boot;

file_desc_t * fileArray[32];

struct io_intf * globalIO;
//typdef struct fileDescript file_desc_t;

// Defined already in fs.h
/*
int fs_mount(struct io_intf* io);
int fs_open(const char* name, struct io_intf** io);
*/

void fs_close(struct io_intf* io);
long fs_write(struct io_intf* io, const void* buf, unsigned long n);
long fs_read(struct io_intf* io, void * buf, unsigned long n);
int fs_ioctl(struct io_intf*, int cmd, void * arg);
int fs_getlen(file_desc_t* fd, void * arg);
int fs_getpos(file_desc_t* fd, void * arg);
int fs_setpos(file_desc_t* fd, void * arg);
int fs_getblksz(file_desc_t* fd, void * arg);

int fs_mount(struct io_intf* io)
{

    globalIO = io;
    long err = io -> ops -> read(io, boot, 4096);
    // boot = io -> ops -> read(io, ) # Read something to get boot_block from virtio

    // Look at virtio to see where to get setup info, then actually set it up
    return err;
} 

int fs_open(const char* name, struct io_intf** io)
{
    int spot = -1;
    int tempIndex = -1;
    int tempPos = -1;

    for(int x = 0; x < 64; x++)
    {
        if(boot -> dir_entries[x].file_name == name)
        {
            tempIndex = boot -> dir_entries[x].inode;
            tempPos = x;
        }
    }
    if(tempPos == -1)
    {
        return -1;
    }

    int cont = 0;
    for (int y = 0; y < 32; y++)
    {
    if(cont == 0)
    {
        if(fileArray[y] == NULL)
        {
            cont = 1;
            spot = y;
        }
        else if(fileArray[y] -> flags == 0)
            {
                cont = 1;
                spot = y;
            }
        }
    else
    {
        break;
    }
    }

    fileArray[spot] -> inode = tempIndex;
    fileArray[spot] -> file_pos = 0;
    fileArray[spot] -> file_size = 0; // Should be equal to length given in inode block for particular inode
    fileArray[spot] -> flags = 1;
    struct io_intf * newIO;
    static const struct io_ops newOps = {
        .close = fs_close,
        .read = fs_read,
        .write = fs_write,
        .ctl = fs_ioctl
    };
    *io = newIO;
    newIO -> ops = &newOps;
    fileArray[spot] -> io_intf = newIO;

   // If file name exists, checl that it isn't already open
    // If both of these are true, set the file to 'in-use' and instantiate the rest of the file members
    // Change io to io_intf that can be used with fs_read, fs_write, to read/write the specific file that was opened

    // Kernel should find next file struct and set up metadata for the file

    //struct file_desct_t * file();
    //assign file metadata
    //*io = io from file
    // change boot block, add inode, directory entry, datablock for new file
    // change the io within the file to let it read, write, etc.

    return 0;
}

long fs_read(struct io_intf* io, void * buf, unsigned long n)
{
    // Read from data blocks into buf

    // Use address of io_intf + however many bytes to get to inode #
    // Go to address of boot block + however many bytes to get to specific inode #
    // keep track of length of B
    // Go to 0th data block # (or wherever based on current position)
    // Keep reading data into buf (using memcopy?) until have done it n times
    // If successful, return 1

    return 0;
}

long fs_write(struct io_intf* io, const void* buf, unsigned long n)
{
    // Same as fs_read, but read buf into data blocks

    return 0;
}

int fs_ioctl(struct io_intf*, int cmd, void * arg)
{
    // Get address of current fd
    file_desc_t * fd; // Should be current fd, maybe address of io?
    if(cmd == 1)
    {
        return fs_getlen(fd, arg);
    }
    if(cmd == 3)
    {
        return fs_getpos(fd, arg);
    }
    if(cmd == 4)
    {
        return fs_setpos(fd, arg);
    }
    if(cmd == 6)
    {
        return fs_getblksz(fd, arg);
    }
}

int fs_getlen(file_desc_t* fd, void * arg)
{
    return 0;
    //return fd -> file_size;
}

int fs_getpos(file_desc_t* fd, void * arg)
{
    return 0;

    //return fd -> file_pos;
}

int fs_setpos(file_desc_t* fd, void * arg)
{
    return 0;
    //fd -> file_pos = arg;
    //return arg;
}

int fs_getblksz(file_desc_t* fd, void * arg)
{
    return 0;
    //return fd -> io_intf -> ops -> ctl(fd -> io_intf, IOCTL_GETBLKSZ, &arg); //Gets block size of file?
}

