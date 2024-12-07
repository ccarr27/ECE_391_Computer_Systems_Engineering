#include "config.h"

#include "memory.h"
#include "console.h"
#include "halt.h"
#include "heap.h"
#include "csr.h"

#include "string.h"
#include "error.h"
#include "thread.h"
#include "process.h"

// #include "syscall.h"
#include "scnum.h"

#include "device.h"
#include "fs.h"


// struct trap_frame {
//              uint64_t x[32]; // x[0] used to save tp when in U mode
//              uint64_t sstatus;
//              uint64_t sepc;
//          };


extern void syscall_handler(struct trap_frame *tfr);

// INTERNAL FUNCTION DECLARATIONS

static int sysexit(void);
static int sysmsgout(const char *msg);
static int sysdevopen(int fd, const char *name, int instno);
static int sysfsopen(int fd, const char *name);
static int sysclose(int fd);
static long sysread(int fd, void *buf, size_t bufsz);
static long syswrite(int fd, const void *buf, size_t len);
static int sysioctl(int fd, int cmd, void *arg);
static int sysexec(int fd);
static int sysfork(const struct trap_frame * tfr);
static int syswait(int tid);
static int sysusleep(unsigned long us);

/*
int64_t syscall(struct trap_frame * tfr)

Inputs: tfr - trap frame that we are taking parameters from

Outputs: integer that is the return value from the specific system call (normally 0 or error message)

Effects: It calls a specific system call and has all of the desired effects of said call

Description: We look at the a[7] value from the trap frame to determine which system call we want to make.
We then call the specific system call with the needed parameters for that function. Returns 0 or an error code.

*/

int64_t syscall(struct trap_frame * tfr)
{
    const uint64_t * const a = tfr -> x + TFR_A0;
    // const uint64_t * const a = tfr -> x;

    // switch (a[TFR_A7]) {
    switch (a[7]) {     // Runs specific system call with desired parameters

        case SYSCALL_MSGOUT:
            return sysmsgout((const char *)a[0]);
            break;
        
        case SYSCALL_EXIT:
            return sysexit();
            break;
        
        case SYSCALL_DEVOPEN:
            return sysdevopen((int)a[0], (const char *)a[1], (int)a[2]);
            break;
        
        case SYSCALL_FSOPEN:
            return sysfsopen((int)a[0], (const char *)a[1]);
            break;
        
        case SYSCALL_CLOSE:
            return sysclose((int)a[0]);
            break;
        
        case SYSCALL_READ:
            return sysread((int)a[0], (void *)a[1], (size_t)a[2]);
            break;

        case SYSCALL_WRITE:
            return syswrite((int)a[0], (const void *)a[1], (size_t)a[2]);
            break;

        case SYSCALL_IOCTL:
            return sysioctl((int)a[0], (int)a[1], (void *)a[2]);
            break;

        case SYSCALL_EXEC:
            return sysexec((int)a[0]);
            break;

        case SYSCALL_FORK:
            // return sysfork((const struct trap_frame *)a[0]); //we think we pass in trap frame instead
            return sysfork((const struct trap_frame *) tfr);
            break;

        case SYSCALL_WAIT:
            return syswait((int)a[0]);
            break;
        
        case SYSCALL_USLEEP:
            return sysusleep((unsigned long)a[0]);
            break;
    }
    return 0;
}

/*
void syscall_handler(struct trap_frame * tfr)

Inputs: tfr - trap frame that we are taking parameters from

Outputs: None

Effects: It calls the specific system call using the syscall function, and places its return value into a0

Description: syscall_handler is called by the umode exception handler when it requests a system call to be performed.
It then calls syscall, which executes a specific system call and places its return value in a0 of the trap frame.
*/

void syscall_handler(struct trap_frame * tfr)
{
    tfr -> sepc += 4;

    tfr ->x[TFR_A0] = syscall(tfr);
}

/*
int sysmsgout(const char * msg)

Inputs: msg - the message we want printed to the console

Outputs: returns 0 when completed

Effects: Prints the name of the thread and the message to the console

Description: This function calls kprintf to print out the name of the thread and the message for the thread
*/

int sysmsgout(const char * msg)
{

    trace("%s(msg=%p)", __func__, msg);


    kprintf("Thread <%s:%d> says: %s\n",
    thread_name(running_thread()), running_thread(), msg);

    return 0;
}

/*
int sysexit(void)

Inputs: None

Outputs: Returns 0 (won't return this value)

Effects: Calls process_exit to exit the system calls

Description: Exits the currently running process
*/

int sysexit(void)
{
    process_exit();
    return 0;
}

/*
int sysexec(int fd)

Inputs: fd - file descriptor to be opened

Outputs: Returns 0 or error code

Effects: It halts the currently running program and starts a new program

Description: Calls process_exec on the process given by the file descriptor input
*/

int sysexec(int fd)
{
    if(current_process() -> iotab[fd] == NULL)
    {
        return -EINVAL;
    }
    process_exec(current_process() -> iotab[fd]);
    
    // Validate that fd is a valid descripter
    return 0;
}

/*
int sysdevopen(int fd, const char * name, int instno)

Inputs: 
fd - file descriptor to be opened
name - name of file descriptor to be opened
instno - instance number to of file descriptor to be opened

Outputs: Returns 0 or an error code

Effects: Calls device_open to open a specific device given by the input parameters

Description: sysdevopen allocates memory for the io_intf device, which is passed through device_open
and stored in the iotab for the current process to be used for future operations. If fd is negative, find the next available iotab, 
otherwise open the specified io tab from fd.
*/

int sysdevopen(int fd, const char * name, int instno)
{
    if(fd >= 0)
    {
        struct io_intf * device;
        int retVal = device_open(&device, name, instno);
        if(retVal != 0)
        {
            return -EINVAL;
        }
        current_process() -> iotab[fd] = device;
        return fd;
        // Request specific file descriptor number
    }
    else{
        int x = 0;      // Else find next available iotab
        while(current_process() -> iotab[x] != NULL)
        {
            if(x <= PROCESS_IOMAX)
            {
                return -EINVAL;
            }
            x += 1;
        }
        struct io_intf * device;
        int retVal = device_open(&device, name, instno);
        if(retVal != 0)
        {
            return -EINVAL;
        }
        current_process() -> iotab[x] = device;
        return x;
        // Request next availble descriptor number
    }
    // Returns file descriptor on success, negative on error
    return 0;
}

/*
int sysfsopen(int fd, const char * name)

Inputs: fd - the file descriptor we want to open
name - name of file we want to open

Outputs: Returns 0 or error code

Effects: Calls fs_open to open the given file name, sets the necessary iotab to the opened file

Description: sysfsopen allocates memory for the io_intf device, which is passed through fs_open
and stored in the iotab for the current process to be used for future operations. If fd is negative, find the next available iotab, 
otherwise open the specified io tab from fd.

*/

int sysfsopen(int fd, const char * name)
{
    if(fd >= 0)
    {
        struct io_intf ** file = kmalloc(sizeof(struct io_intf));
        int retVal = fs_open(name, file);
        if(retVal != 0)
        {
            return -EINVAL;
        }
        current_process() -> iotab[fd] = *file;
        return fd;
        // Requests specific file descriptor number
    
    }
    else{
        int x = 0;
        while(current_process() -> iotab[x] != NULL)
        {
            if(x >= PROCESS_IOMAX)
            {
                return -EINVAL;
            }
            x += 1;
        }
        
        struct io_intf ** file = kmalloc(sizeof(struct io_intf));
        int retVal = fs_open(name, file);
        if(retVal != 0)
        {
            return -EINVAL;
        }
        current_process() -> iotab[x] = *file;
        return x;
        // Requests next file descriptor number
    }
    // Returns file descriptor on success, negative on error
    return fd;
}

/*
int sysread(int fd, void *buf, size_t bufsz)

Inputs: fd - the file descriptor we want to read from
buf - the buffer we want to write into
bufsz - the number of bytes we want to read

Outputs: Returns 0 or error code

Effects: Calls ioread on the given device or file to read from the io device.

Description: sysread reads bufsz bytes from the io specified by fd into buf. It also checks to make sure that the amount of bytes
read is not greater than the size of of the io device.

*/

static long sysread(int fd, void *buf, size_t bufsz)
{
    if(current_process() -> iotab[fd] == NULL)
    {
        return -EACCESS;
    }
    int val = ioread(current_process() -> iotab[fd], buf, bufsz);
    // end of file return 0
    if(val < 0)
    {
        return -EIO;
    }
    /*
    void * pos = kmalloc(sizeof(uint64_t));
    void * len = kmalloc(sizeof(uint64_t));
    // Checks that we have not gone past the length of the file
    ioctl(current_process() -> iotab[fd], IOCTL_GETPOS, pos);
    ioctl(current_process() -> iotab[fd], IOCTL_GETLEN, len);
    if(pos >= len)
    {
        return 0;
    }
    kfree(pos);
    kfree(len);
    */
    return val;
}

/*
int syswrite(int fd, void *buf, size_t len)

Inputs: fd - the file descriptor we want to write into
buf - the buffer we want to read from
len - the number of bytes we want to write

Outputs: Returns 0 or error code

Effects: Calls iowrite on the given device or file to read from the io device.

Description: syswrite writes bufsz bytes from the buf into the io specified by fd into. It also checks to make sure that the amount of bytes
written is not greater than the size of of the io device.

*/

static long syswrite(int fd, const void *buf, size_t len)
{
    if(current_process() -> iotab[fd] == NULL)
    {
        return -EACCESS;
    }
    int val = iowrite(current_process() -> iotab[fd], buf, len);
    // end of file return 0
    if(val < 0)
    {
        return -EIO;
    }
    /*
     // Checks that we have not gone past the length of the file
    void * pos = kmalloc(sizeof(uint64_t));
    void * size = kmalloc(sizeof(uint64_t));
    ioctl(current_process() -> iotab[fd], IOCTL_GETPOS, pos);
    ioctl(current_process() -> iotab[fd], IOCTL_GETLEN, size);
    if(pos >= size)
    {
        return 0;
    }
    kfree(pos);
    kfree(size);
    */
    return val;
}

/*
int sysioctl(int fd, void *buf, size_t len)

Inputs: fd - the file descriptor we want to perform an ioctl with
cmd - the specific ioctl command we want to perform
arg - the argument where we put the return of the ioctl

Outputs: Returns output from ioctl (0 or error code)

Effects: Calls ioctl on the given device or file.

Description: sysioctl performs an ioctl with the given iotab. The return value of the function is stored in temmp, which is then returned

*/

static int sysioctl(int fd, int cmd, void * arg)
{
    if(cmd == IOCTL_SETLEN)
    {
    uint64_t * tempVal = (uint64_t *) arg;
    int temp = ioseek(current_process() -> iotab[fd], *tempVal);
    return temp;
    }
    else
    {
    int temp = ioctl(current_process() -> iotab[fd], cmd, &arg);
    return temp;
    }
}

/*
int sysclose(int f)

Inputs: fd - the file descriptor we want to cos

Outputs: Returns 0

Effects: Calls ioclose on the given device or file to close itt.

Description: sysclose goes to the io device in the iotab specified by the fd and closes it. We then set the iotab[fd] to NULL so that
it can be used for another device or file later on. 

*/

int sysclose(int fd)
{
    ioclose(current_process() -> iotab[fd]);
    // fd no longer valid
    current_process() -> iotab[fd] = NULL;
    return 0;
}


static int sysfork(const struct trap_frame * tfr)
{
    // Creates copy of invoking process, creating child that is identical to parent except:
    // _fork returns child thread ID in parent, _fork returns 0 in child process
    // Calls process_fork()
    process_fork(tfr);

    // Allocate new process and copy all iotab pointers from parent to child process
    // Initialize all reference counts
    // Calls helper functions

    // thread_fork_to_user
    //_thread_finish_fork
    // memory space clone

    //this return should returns child thread ID in parent or returns 0 in child process
    return 0;
}

static int syswait(int tid)
{
    // Wait for certain child to exit before returning. If tid is main thread, wait for any child to exit

    // Given code from lecture slides

    trace("%s(%d)", __func__, tid);

    if(tid == 0)
    {
        return thread_join_any();
    }
    else
        return thread_join(tid);
}

static int sysusleep(unsigned long us)
{
    // Sleeps for us number of microseconds (read timer.c)

    return 0;
}