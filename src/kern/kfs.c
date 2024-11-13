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
    struct io_intf io_intf;
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

/*
Description: mounts an io_intf to the filesystem provider so it is ready for fs_open
Purpose: To prepare for io for fs_open
Input: blkio - io_intf to be mounted
Output: int that usually if success equals 0
*/

int fs_mount(struct io_intf * blkio)
{
    for(int x = 0; x < 32; x++)
    {
        memset(&fileArray[x], 0, sizeof(file_desc_t));
    }
    globalIO = blkio;                                           // Store the given io_intf
    long err = ioread(blkio, &boot, FS_BLKSZ);                  // Read the boot block from the io_intf and store it in memory
    if(err == FS_BLKSZ)
    {
        return 0;
    }
    else
    {
        return -EIO;
    }
} 


/*
Description: looks for a file with a given name, and if it finds it it sets up a file struct with the file and marks it in-use
Purpose: To prepare a file struct and to initialize operations for it (fs_read, fs_write, etc.)
Input: name - name of file, ioptr - double pointer to io_intf to be used for later fs operations
Output: int that usually if success equals 0
*/
int fs_open(const char * name, struct io_intf ** ioptr)
{
    int spot;
    int tempIndex;
    int tempPos = -EINVAL;  // Initialize tempPos to negative value for case of not finding file

    // Iterate through the file struct to find a file with the given name

    for(int x = 0; x < numFiles; x++)
    {
        if(strcmp(name, boot.dir_entries[x].file_name) == 0)    // If we find a file with the given name, set the inode # and position
        {   
            tempIndex = boot.dir_entries[x].inode;  
            tempPos = x;
            x = numFiles;       // Stop loop after reaching first file with given name
        }
    }

    if(tempPos < 0)
    {
        return -EINVAL;         // Return -1 if no file with name found
    }

    for (int y = 0; y < FS_NAMELEN; y++)        // Find the first unused file struct and mark it as used
    {
    if(fileArray[y].flags == 0)
        {
            spot = y;
            y = FS_NAMELEN;
        }
    }

    int testVal = ioseek(globalIO, FS_BLKSZ + (tempIndex * FS_BLKSZ));  // Get the length of the file
    
    if(testVal != 0)
    {
        return -EBUSY;
    }

    void * readSize = kmalloc(blockNumSize);
    uint64_t size;

    long otherVal = globalIO -> ops -> read(globalIO, &readSize, blockNumSize); // Read the length of the file into the buffer

    if(otherVal != blockNumSize)
    {
        return -EBADFMT;
    }

    size = (uint64_t)(readSize);
    kfree(readSize);

    // Set members of the specific file struct we are working with

    fileArray[spot].inode = tempIndex;
    fileArray[spot].file_pos = 0;
    
    fileArray[spot].file_size = size; // Should be equal to length given in inode block for particular inode
    fileArray[spot].flags = 1;
    // Set operations for the io_intf (want to use the fs operations)
    static const struct io_ops newOps = {
        .close = &fs_close,
        .read = &fs_read,
        .write = &fs_write,
        .ctl = &fs_ioctl
    };

    fileArray[spot].io_intf.ops = &newOps;
    intSpot = spot;

    //Set the value of the double pointer to point to the io_intf for the new file struct entry
    *ioptr = &fileArray[spot].io_intf;  

    return 0;
}

/*
Description: closes the file
Purpose: sets the file struct to "not in use" and frees memory associated with the object
Input: io - the io_intf for the file struct to close
Output: None
*/

void fs_close(struct io_intf* io)
{
    io -> ops = NULL;
    fileArray[intSpot].flags = 0;
    kfree(io);  // Free the memory associated with the io_intf for the file struct and mark the file as closed
}

/*
Description: Reads from the file into a buffer
Purpose: Allows us to read a file, and changes the position to where we finished reading from
Input: io - io_intf for file struct, buf - place we are reading to, n - number of bytes we are reading
Output: long that usually if success equals 0
*/


long fs_read(struct io_intf* io, void * buf, unsigned long n)
{
    struct file_desc_t * fd = (void*)io - offsetof(struct file_desc_t, io_intf); // Should be current fd, maybe address of io?
    // Get the total number of innodes from the boot block
   
    ioseek(globalIO, blockNumSize);
    uint32_t numInodes;
    ioread(globalIO, &numInodes, blockNumSize);
    
    //get the specific innode number

 
    uint64_t inode_num = fd -> inode;

    int seek_two = ioseek(globalIO, FS_BLKSZ + (inode_num * FS_BLKSZ));

    if(seek_two != 0)
    {
        return -EBADFMT;
    }
    // Get the length of the file
    uint32_t length_b;
    ioread(globalIO, &length_b, blockNumSize);
    
    //Get the current position of the file
    
    uint64_t filePos;
    ioctl(io, IOCTL_GETPOS, &filePos);
    long count = 0;

    console_printf("filepos: %d \n", filePos);
 
    // If filePos + n is bigger than fileSize, we only read up to the end of the file

    if(filePos + n > length_b)
    {
        n = length_b - filePos;
    }
    while(n > 0)
    {
        int blockNum = filePos / FS_BLKSZ;
        // Get the current data block by looking at the current position and innode #
        ioseek(globalIO, (uint64_t)((FS_BLKSZ + (inode_num * FS_BLKSZ)) + (blockNumSize + (blockNum * blockNumSize))));
        uint64_t blockSpot;
        ioread(globalIO, &blockSpot, blockNumSize);
        
        // Move to the correct data block specified by the innode block

        ioseek(globalIO, (FS_BLKSZ + FS_BLKSZ * (numInodes) + FS_BLKSZ * blockSpot + filePos % FS_BLKSZ));
            
        // Move to correct block #
        unsigned long leftInBlock = FS_BLKSZ - (filePos % FS_BLKSZ); //Tells us how many bytes left to read in the block before finishing the block
        
        if(leftInBlock <= n) // If we should write the entire data block..
        {
        uint64_t leftAfter;   


        leftAfter = ioread(globalIO, buf, leftInBlock);    // Do the actual reading into buffer
        n = n - leftAfter;                                  // Decrement n based on # bytes read 
        count += leftAfter;
        filePos += leftAfter;
        }
        else
        {
            filePos += n;
            count += n;
            uint64_t leftAfter;
            leftAfter = ioread(globalIO, buf, n);
            n = n - leftAfter;
        }
        // After finish read, update filePos

    }
    //read directly from buffer
    ioctl(io, IOCTL_SETPOS, &filePos);

    return count;
}

/*
Description: Writes from the buffer into a file
Purpose: Allows us to write to a file, and changes the position to where we finished writing to
Input: io - io_intf for file struct, buf - place we are writing from, n - number of bytes we are writing
Output: long that usually if success equals 0
*/

long fs_write(struct io_intf* io, const void* buf, unsigned long n)
{
        struct file_desc_t * fd = (void*)io - offsetof(struct file_desc_t, io_intf); // Should be current fd, maybe address of io?
    // Get the total number of innodes from the boot block
    ioseek(globalIO, blockNumSize);
    uint32_t numInodes;
    ioread(globalIO, &numInodes, blockNumSize);
    
    //get the specific innode number
 
    uint64_t inode_num = fd -> inode;

    int seek_two = ioseek(globalIO, FS_BLKSZ + (inode_num * FS_BLKSZ));

    if(seek_two != 0)
    {
        return -EBADFMT;
    }
    // Get the length of the file
    uint32_t length_b;
    ioread(globalIO, &length_b, blockNumSize);
    
    //Get the current position of the file
    
    uint64_t filePos;
    ioctl(io, IOCTL_GETPOS, &filePos);
    long count = 0;
 
    // If filePos + n is bigger than fileSize, we only read up to the end of the file

    if(filePos + n > length_b)
    {
        n = length_b - filePos;
    }
    while(n > 0)
    {
        int blockNum = filePos / FS_BLKSZ;
        // Get the current data block by looking at the current position and innode #
        ioseek(globalIO, (uint64_t)((FS_BLKSZ + (inode_num * FS_BLKSZ)) + (blockNumSize + (blockNum * blockNumSize))));
        uint64_t blockSpot;
        ioread(globalIO, &blockSpot, blockNumSize);
        
        // Move to the correct data block specified by the innode block

        ioseek(globalIO, (FS_BLKSZ + FS_BLKSZ * (numInodes) + FS_BLKSZ * blockSpot + filePos % FS_BLKSZ));
        // Move to correct block #
        unsigned long leftInBlock = FS_BLKSZ - (filePos % FS_BLKSZ); //Tells us how many bytes left to read in the block before finishing the block
        
        if(leftInBlock <= n) // If we should write the entire data block..
        {
        uint64_t leftAfter;

        leftAfter = iowrite(globalIO, buf, leftInBlock);    // Do the actual reading into buffer
        n = n - leftAfter;                                  // Decrement n based on # bytes read 
        count += leftAfter;
        filePos += leftAfter;
        }
        else
        {
            filePos += n;
            count += n;
            uint64_t leftAfter;
            leftAfter = iowrite(globalIO, buf, n);
            n = n - leftAfter;
        }
        // After finish read, update filePos

    }

    ioctl(io, IOCTL_SETPOS, &filePos);

    return count;
    
}

/*
Description: Uses specific commands to call helper functions
Purpose: Allows us to modify values within the file struct
Input: io - io_intf for file struct, cmd - the command number, arg - the argument (if needed)
Output: int that is the output of the helper function
*/

int fs_ioctl(struct io_intf* io, int cmd, void * arg)
{
    // Get address of current fd
    struct file_desc_t * const fd = (void*)io - offsetof(struct file_desc_t, io_intf); // Should be current fd, maybe address of io?

    if(cmd == IOCTL_GETLEN) // Call get length
    {
        return fs_getlen(fd, arg);
    }
    if(cmd == IOCTL_GETPOS) // Call get position
    {
        return fs_getpos(fd, arg);
    }
    if(cmd == IOCTL_SETPOS) // Call set position
    {
        return fs_setpos(fd, arg);
    }
    if(cmd == IOCTL_GETBLKSZ)   // Call get block size
    {
        return fs_getblksz(fd, arg);
    }
    return 0;
}

/*
Description: Gets length of file struct
Purpose: Allows us to access the value from the file struct
Input: fd - the file struct, arg - argument (not used)
Output: length of file
*/

int fs_getlen(file_desc_t* fd, void * arg)
{
    size_t * tempSize = (size_t *) arg;
    *tempSize = fd -> file_size;
    return fd -> file_size;
}

/*
Description: Gets position of file struct
Purpose: Allows us to access the value from the file struct
Input: fd - the file struct, arg - argument (not used)
Output: position of file
*/

int fs_getpos(file_desc_t* fd, void * arg)
{
    size_t * tempSize = (size_t *) arg;
    *tempSize = fd -> file_pos;
    return fd -> file_pos;
}

/*
Description: Sets position of file struct
Purpose: Allows us to change the value from the file struct
Input: fd - the file struct, arg - new file position
Output: 0 if successful
*/

int fs_setpos(file_desc_t* fd, void * arg)
{
    size_t new_pos = *(size_t *)arg;
        if (new_pos >= fd->file_size) {
            return -EIO; // Out of bounds, return error
        }
    fd -> file_pos = new_pos;
    return 0;

}

/*
Description: Gets block size of file struct
Purpose: Allows us to access the value from the file struct
Input: fd - the file struct, arg - argument (not used)
Output: block size of file
*/

int fs_getblksz(file_desc_t* fd, void * arg)
{
    //Should just return 4096, constant
    // NEED TO CHANGE
    size_t * tempSize = (size_t *) arg;
    *tempSize = FS_BLKSZ;

    return FS_BLKSZ; //Gets block size of file?
}

