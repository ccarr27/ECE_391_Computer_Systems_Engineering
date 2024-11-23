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

#include "syscall.h"
#include "scnum.h"



struct trap_frame {
             uint64_t x[32]; // x[0] used to save tp when in U mode
             uint64_t sstatus;
             uint64_t sepc;
         };


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

int64_t syscall(struct trap_frame * tfr)
{
    const uint64_t * const a = tfr -> x + TFR_A0;

    switch (a[TFR_A7]) {

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
            return sysread((int)a[0], (const void *)a[1], (size_t)a[2]);
            break;

        case SYSCALL_IOCTL:
            return sysioctl((int)a[0], (int)a[1], (void *)a[2]);
            break;

        case SYSCALL_EXEC:
            return sysexec((int)a[0]);
            break;
    }
}

void syscall_handler(struct trap_frame * tfr)
{
    tfr -> sepc += 4;

    tfr ->x[TFR_A0] = syscall(tfr);
}

int sysmsgout(const char * msg)
{
    int result;
    trace("%s(msg=%p)", __func__, msg);

    //result = memory_validate_str(msg, PTE_U);
    //if(result != 0)
    //  return result;
    kprintf("Thread <%s:%d> says: %s\n",
    thread_name(running_thread()), running_thread(), msg);

    return 0;
}

int sysexit(void)
{
    process_exit();
}

int sysexec(int fd)
{
    if(current_process() -> iotab[fd] == NULL)
    {
        return -EINVAL;
    }
    process_exec(current_process() -> iotab[fd]);
    
    // Validate that fd is a valid descripter

}

int sysdevopen(int fd, const char * name, int instno)
{
    if(fd >= 0)
    {
        // Request specific file descriptor number
    }
    else{
        // Request next availble descriptor number
    }
    // Returns file descriptor on success, negative on error
    return fd;
}

int sysfsopen(int fd, const char * name)
{
    if(fd >= 0)
    {
        // Requests specific file descriptor number
    
    }
    else{
        // Requests next file descriptor number
    }
    // Returns file descriptor on success, negative on error
    return fd;
}

int sysclose(int fd)
{
    ioclose(current_process() -> iotab[fd]);
    // fd no longer valid
    current_process() -> iotab[fd] = NULL;
    // returns 0 on success code, error code on error
    return 0;
}




