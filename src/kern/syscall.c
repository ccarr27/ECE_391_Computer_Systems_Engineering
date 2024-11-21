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
// Maybe - #include "syscall.h"


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
