#include "string.h"
#include "io.h"
#include "virtio.h"
#include "fs.h" // NOT SURE

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
#define FS_BLKSZ      4096
#define FS_NAMELEN    32

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

//int fs_mount(struct io_intf* io)
int fs_mount(struct io_intf * blkio)
{

    globalIO = blkio;
    int err = blkio -> ops -> read(blkio, boot, 4096);
    // boot = io -> ops -> read(io, ) # Read something to get boot_block from virtio

    // Look at virtio to see where to get setup info, then actually set it up
    return err;
} 

// When traveling to specific inode, use io -> seek

//int fs_open(const char* name, struct io_intf** io)
int fs_open(const char * name, struct io_intf ** ioptr)
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
    int testVal = ioseek(globalIO, 4096 + (tempIndex * 4096));

    if(testVal != 0)
    {
        return -2;
    }

    void * readSize[4];
    uint64_t size;

    int otherVal = globalIO -> ops -> read(globalIO, readSize, 4);
    if(otherVal != 0)
    {
        return -3;
    }

    size = (uint64_t)(readSize);

    fileArray[spot] -> inode = tempIndex;
    fileArray[spot] -> file_pos = 0;
    
    fileArray[spot] -> file_size = size; // Should be equal to length given in inode block for particular inode
    fileArray[spot] -> flags = 1;
    static const struct io_ops newOps = {
        .close = fs_close,
        .read = fs_read,
        .write = fs_write,
        .ctl = fs_ioctl
    };
    struct io_intf * newIO;
    //newIO =
    newIO -> ops = &newOps;
    fileArray[spot] -> io_intf = newIO;


    //struct io_intf * tempIO = &newIO;
    //*ioptr = &tempIO;    // HERE!
    *ioptr = newIO;  

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

void fs_close(struct io_intf* io)
{
    io -> ops = NULL;
}

long fs_read(struct io_intf* io, void * buf, unsigned long n)
{
    // Read from data blocks into buf
    int seek_one = ioseek(io, 16);

    ioseek(globalIO, 4);
    void * read_numInodes;
    uint64_t numInodes;
    ioread(globalIO, read_numInodes, 4);

    numInodes = (uint64_t) (read_numInodes);
    //void * read_numData;
    //uint64_t numData;
    ioseek(globalIO, 8);
    //ioread(globalIO, read_numData, 4);

    //numData = (uint64_t) read_numData;
    if(seek_one != 0)
    {
        return -1;
    }
    void * read_inode_num[8];
    uint64_t inode_num;
    io -> ops -> read(io, read_inode_num, 8);
    inode_num = (uint64_t) (read_inode_num);

    int seek_two = ioseek(globalIO, 4096 + (inode_num * 4096));

    if(seek_two != 0)
    {
        return -2;
    }

    void * read_length_b[4];
    uint64_t length_b;
    globalIO -> ops -> read(globalIO, read_length_b, 4);
    length_b = (uint64_t) read_length_b;
    ioseek(io, 8);

    void * read_filePos[8];
    uint64_t filePos;
    io -> ops -> read(io, read_filePos, 8);
    filePos = (uint64_t) (read_filePos);

    // If filePos + n is bigger than fileSize, do we return an error code, or read up until that spot
    unsigned long tempN;
    if(filePos + n > length_b)
    {
        tempN = length_b - filePos;
    }
    else
    {
        tempN = n;
    }

    int blockNum = 0;

    while(tempN != 0)
    {
        ioseek(globalIO, 4096 + (numInodes * 4096) + (blockNum * 4096) + (filePos % 4096));
        // Move to correct block #
        int leftInBlock = 4096 - filePos % 4096; //Tells us how many bytes left to read in the block before finishing the block
        if(leftInBlock >= tempN)
        {
        memcpy(globalIO, buf, leftInBlock);
        tempN -= leftInBlock;
        }
        else
        {
            memcpy(globalIO, buf, tempN);
            tempN = 0;
        }
        filePos += leftInBlock;
        // After finish read, update filePos
        blockNum += 1;
    }

    return 0;

    // Use address of io_intf + however many bytes to get to inode #
    // Go to address of boot block + however many bytes to get to specific inode #
    // keep track of length of B
    // Go to 0th data block # (or wherever based on current position)
    // Keep reading data into buf (using memcopy?) until have done it n times
    // If successful, return 1
}

long fs_write(struct io_intf* io, const void* buf, unsigned long n)
{
    // Same as fs_read, but read buf into data blocks

    int seek_one = ioseek(io, 16);

    ioseek(globalIO, 4);
    void * read_numInodes[4];
    uint64_t numInodes;
    ioread(globalIO, read_numInodes, 4);
    numInodes = (uint64_t) read_numInodes;

    //void * read_numData;
    //uint64_t numData;
    ioseek(globalIO, 8);
    //ioread(globalIO, read_numData, 4);
    //numData = (uint64_t) read_numData;

    if(seek_one != 0)
    {
        return -1;
    }
    void * read_inode_num[8];
    uint64_t inode_num;
    io -> ops -> read(io, read_inode_num, 8);
    inode_num = (uint64_t) read_inode_num;

    int seek_two = ioseek(globalIO, 4096 + (inode_num * 4096));

    if(seek_two != 0)
    {
        return -2;
    }

    void * read_length_b[4];
    uint64_t length_b;
    globalIO -> ops -> read(globalIO, read_length_b, 4);
    length_b = (uint64_t) read_length_b;
    ioseek(io, 8);

    void * read_filePos[8];
    uint64_t filePos;
    io -> ops -> read(io, read_filePos, 8);

    filePos = (uint64_t) read_filePos;

    // If filePos + n is bigger than fileSize, do we return an error code, or read up until that spot
    unsigned long tempN;
    if(filePos + n > length_b)
    {
        tempN = length_b - filePos;
    }
    else
    {
        tempN = n;
    }

    int blockNum = 0;

    while(tempN != 0)
    {
        ioseek(globalIO, 4096 + (numInodes * 4096) + (blockNum * 4096) + (filePos % 4096));
        // Move to correct block #
        int leftInBlock = 4096 - filePos % 4096; //Tells us how many bytes left to read in the block before finishing the block
        if(leftInBlock >= tempN)
        {
        memcpy(globalIO, buf, leftInBlock);
        tempN -= leftInBlock;
        }
        else
        {
            memcpy(globalIO, buf, tempN);
            tempN = 0;
        }
        filePos += leftInBlock;
        // After finish read, update filePos
        blockNum += 1;
    }

    return 0;
}

int fs_ioctl(struct io_intf* io, int cmd, void * arg)
{
    // Get address of current fd
    struct file_desc_t * fd = (void*)io - offsetof(struct file_desc_t, io_intf); // Should be current fd, maybe address of io?

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
    fd -> file_pos = (uint64_t) arg;
    return (int) (fd -> file_pos);
    //Should this just return 0 or error code?
}

int fs_getblksz(file_desc_t* fd, void * arg)
{
    // Is block size just total number of blocks (N + D + 1)?
    ioseek(globalIO, 4);
    void * read_n[4];
    uint64_t n;
    ioread(globalIO, read_n, 4);
    n = (uint64_t) (read_n);
    ioseek(globalIO, 8);
    void * read_d[4];
    uint64_t d;
    ioread(globalIO, read_d, 4);
    d = (uint64_t)(read_d);

    return n + d + 1; //Gets block size of file?
}

