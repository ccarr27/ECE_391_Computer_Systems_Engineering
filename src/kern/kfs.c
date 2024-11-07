#include "string.h"
#include "heap.h"
#include "io.h"
#include "virtio.h"
#include "console.h"
#include "fs.h"

// Disk layout:
// [ boot block | inodes | data blocks ]
#define FS_BLKSZ      4096
#define FS_NAMELEN    32
#define numFiles      64
#define blockNumSize  4
#define fileStructSize 8

#define IOCTL_GETLEN        1
//           arg is pointer to uint64_t
#define IOCTL_SETLEN        2
//           arg is pointer to uint64_t
#define IOCTL_GETPOS        3
//           arg is pointer to uint64_t
#define IOCTL_SETPOS        4
//           arg is ignored
#define IOCTL_FLUSH         5
//           arg is pointer to uint32_t
#define IOCTL_GETBLKSZ      6

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

struct boot_block_t boot;

file_desc_t fileArray[32];

uint64_t intSpot;

struct io_intf * globalIO;

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
    int err = blkio -> ops -> read(blkio, &boot, FS_BLKSZ);
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

    int contTwo = 0;

    for(int x = 0; x < numFiles; x++)
    {
        if(contTwo == 0)
        {
        if(strcmp(name, boot.dir_entries[x].file_name) == 0)
        {
            tempIndex = boot.dir_entries[x].inode;
            tempPos = x;
            contTwo = 1;
        }
        }
        else
        {
            break;
        }
    }
    if(tempPos == -1)
    {
        return -1;
    }

    int cont = 0;
    for (int y = 0; y < FS_NAMELEN; y++)
    {
    if(cont == 0)
    {
        if(fileArray[y].flags == 0)
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
    int testVal = ioseek(globalIO, FS_BLKSZ + (tempIndex * FS_BLKSZ));

    
    if(testVal != 0)
    {
        return -2;
    }

    void * readSize = kmalloc(blockNumSize);
    uint64_t size;

    int otherVal = globalIO -> ops -> read(globalIO, &readSize, blockNumSize);
    if(otherVal != 0)
    {
        return -3;
    }

    size = (uint64_t)(readSize);
    kfree(readSize);

    fileArray[spot].inode = tempIndex;
    fileArray[spot].file_pos = 0;
    
    fileArray[spot].file_size = size; // Should be equal to length given in inode block for particular inode
    fileArray[spot].flags = 1;
    static const struct io_ops newOps = {
        .close = fs_close,
        .read = fs_read,
        .write = fs_write,
        .ctl = fs_ioctl
    };
    struct io_intf * newIO;
    //newIO =
    newIO = kmalloc(sizeof(struct io_intf));
    newIO -> ops = &newOps;
    fileArray[spot].io_intf = newIO;
    intSpot = spot;

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
    kfree(io);
}

long fs_read(struct io_intf* io, void * buf, unsigned long n)
{
    // Read from data blocks into buf

    ioseek(globalIO, blockNumSize);
    void * read_numInodes = kmalloc(blockNumSize);
    uint64_t numInodes;
    globalIO -> ops -> read(globalIO, &read_numInodes, blockNumSize);

    numInodes = (uint64_t) (read_numInodes);

    kfree(read_numInodes);
    //void * read_numData;
    //uint64_t numData;
    //ioseek(globalIO, 8);
    //ioread(globalIO, read_numData, 4);

    //numData = (uint64_t) read_numData;
    //ioseek(io, 24);
    //console_printf("pos %d \n", io -> ops -> ctl(io, 3, NULL));
 
    uint64_t inode_num = fileArray[intSpot].inode;

    //console_printf("inode %d \n", inode_num);

    int seek_two = ioseek(globalIO, FS_BLKSZ + (inode_num * FS_BLKSZ));

    if(seek_two != 0)
    {
        return -2;
    }

    void * read_length_b = kmalloc(blockNumSize);
    uint64_t length_b;
    globalIO -> ops -> read(globalIO, &read_length_b, blockNumSize);
    length_b = (uint64_t) read_length_b;
    ioseek(io, fileStructSize);
    kfree(read_length_b);

    uint64_t filePos = fileArray[intSpot].file_pos;
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

    while(tempN != 0)
    {
        int blockNum = filePos / FS_BLKSZ;
        ioseek(globalIO, (FS_BLKSZ + (inode_num * FS_BLKSZ)) + (blockNumSize + (blockNum * blockNumSize)));
        void * read_blockSpot = kmalloc(blockNumSize);
        ioread(globalIO, &read_blockSpot, blockNumSize);
        uint64_t blockSpot = (uint64_t) read_blockSpot;

        ioseek(globalIO, (FS_BLKSZ + FS_BLKSZ * (numInodes) + FS_BLKSZ * blockSpot + filePos % FS_BLKSZ));


        // Move to correct block #
        int leftInBlock = FS_BLKSZ - (filePos % FS_BLKSZ); //Tells us how many bytes left to read in the block before finishing the block

        if(leftInBlock <= tempN)
        {
        memcpy(buf, &globalIO, leftInBlock);
        tempN -= leftInBlock;
        filePos += leftInBlock;
        }
        else
        {
            memcpy(buf, &globalIO, tempN);
            filePos += tempN;
            tempN = 0;
        }
        // After finish read, update filePos
    }
    fileArray[intSpot].file_pos = filePos;

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
    // Read from data blocks into buf

    ioseek(globalIO, blockNumSize);
    void * read_numInodes = kmalloc(blockNumSize);
    uint64_t numInodes;
    globalIO -> ops -> read(globalIO, &read_numInodes, blockNumSize);

    numInodes = (uint64_t) (read_numInodes);

    kfree(read_numInodes);
    //void * read_numData;
    //uint64_t numData;
    //ioseek(globalIO, 8);
    //ioread(globalIO, read_numData, 4);

    //numData = (uint64_t) read_numData;
    //ioseek(io, 24);
    //console_printf("pos %d \n", io -> ops -> ctl(io, 3, NULL));
 
    uint64_t inode_num = fileArray[intSpot].inode;

    //console_printf("inode %d \n", inode_num);

    int seek_two = ioseek(globalIO, FS_BLKSZ + (inode_num * FS_BLKSZ));

    if(seek_two != 0)
    {
        return -2;
    }

    void * read_length_b = kmalloc(blockNumSize);
    uint64_t length_b;
    globalIO -> ops -> read(globalIO, &read_length_b, blockNumSize);
    length_b = (uint64_t) read_length_b;
    ioseek(io, fileStructSize);
    kfree(read_length_b);

    uint64_t filePos = fileArray[intSpot].file_pos;
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

    while(tempN != 0)
    {
        int blockNum = filePos / FS_BLKSZ;
        ioseek(globalIO, (FS_BLKSZ + (inode_num * FS_BLKSZ)) + (blockNumSize + (blockNum * blockNumSize)));
        void * read_blockSpot = kmalloc(blockNumSize);
        ioread(globalIO, &read_blockSpot, blockNumSize);
        uint64_t blockSpot = (uint64_t) read_blockSpot;

        ioseek(globalIO, (FS_BLKSZ + FS_BLKSZ * (numInodes) + FS_BLKSZ * blockSpot + filePos % FS_BLKSZ));


        // Move to correct block #
        int leftInBlock = FS_BLKSZ - (filePos % FS_BLKSZ); //Tells us how many bytes left to read in the block before finishing the block

        if(leftInBlock <= tempN)
        {
        memcpy(globalIO, &buf, leftInBlock);
        tempN -= leftInBlock;
        filePos += leftInBlock;
        }
        else
        {
            memcpy(globalIO, &buf, tempN);
            filePos += tempN;
            tempN = 0;
        }
        // After finish read, update filePos
    }
    fileArray[intSpot].file_pos = filePos;

    return 0;
    
}

int fs_ioctl(struct io_intf* io, int cmd, void * arg)
{
    // Get address of current fd
    struct file_desc_t * const fd = (void*)io - offsetof(struct file_desc_t, io_intf); // Should be current fd, maybe address of io?

    file_desc_t fdOther = fileArray[intSpot];
    if(cmd == IOCTL_GETLEN)
    {
        return fs_getlen(&fdOther, arg);
    }
    if(cmd == IOCTL_GETPOS)
    {
        return fs_getpos(&fdOther, arg);
    }
    if(cmd == IOCTL_SETPOS)
    {
        return fs_setpos(&fdOther, arg);
    }
    if(cmd == IOCTL_GETBLKSZ)
    {
        return fs_getblksz(fd, arg);
    }
    return 0;
}

int fs_getlen(file_desc_t* fd, void * arg)
{
    //return fileArray[intSpot].file_size;
    return fd -> file_size;
}

int fs_getpos(file_desc_t* fd, void * arg)
{
    return fileArray[intSpot].file_pos;
    return fd -> file_pos;
}

int fs_setpos(file_desc_t* fd, void * arg)
{
    fd -> file_pos = (uint64_t) arg;
    return 0;
    //fd -> file_pos = (uint64_t) arg;
    return (int) (fd -> file_pos);
    //Should this just return 0 or error code?
}

int fs_getblksz(file_desc_t* fd, void * arg)
{
    ioseek(globalIO, blockNumSize);
    void * read_n = kmalloc(blockNumSize);
    uint64_t n;
    ioread(globalIO, read_n, blockNumSize);
    n = (uint64_t) (read_n);
    kfree(read_n);
    ioseek(globalIO, fileStructSize);
    void * read_d = kmalloc(blockNumSize);
    uint64_t d;
    ioread(globalIO, read_d, blockNumSize);
    d = (uint64_t)(read_d);
    kfree(read_d);

    return n + d + 1; //Gets block size of file?
}

