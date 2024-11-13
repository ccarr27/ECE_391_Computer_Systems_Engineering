#include "console.h"
#include "thread.h"
#include "device.h"
#include "uart.h"
#include "timer.h"
#include "intr.h"
#include "heap.h"
#include "virtio.h"
#include "halt.h"
#include "elf.h"
#include "fs.h"
#include "string.h"

//           end of kernel image (defined in kernel.ld)
extern char _kimg_end[];
extern char _companion_f_start[];
extern char _companion_f_end[];

#define RAM_SIZE (8*1024*1024)
#define RAM_START 0x80000000UL
#define KERN_START RAM_START
#define USER_START 0x80100000UL

#define UART0_IOBASE 0x10000000
#define UART1_IOBASE 0x10000100
#define UART0_IRQNO 10

#define VIRT0_IOBASE 0x10001000
#define VIRT1_IOBASE 0x10002000
#define VIRT0_IRQNO 1

void main(void) {
     struct io_intf * termio;
    // struct io_intf * blkio;
    void * mmio_base;
    int res;
    int i;

    console_init();
    intr_init();
    devmgr_init();
    thread_init();
    timer_init();

    heap_init(_kimg_end, (void*)USER_START);

    //           Attach NS16550a serial devices

    for (i = 0; i < 2; i++) {
        mmio_base = (void*)UART0_IOBASE;
        mmio_base += (UART1_IOBASE-UART0_IOBASE)*i;
        uart_attach(mmio_base, UART0_IRQNO+i);
    }
    
    //           Attach virtio devices

    void(*entry_func)(struct io_intf*);

    struct io_intf *exeio;





    // timer_start();

    size_t total= (size_t)(_companion_f_end- _companion_f_start);

    struct io_lit lit;
    exeio = iolit_init(&lit, _companion_f_start, total);

    intr_enable();

    res = device_open(&termio, "ser", 1);

    if (res != 0)
        panic("device_open failed");
    
    res = elf_load(exeio, &entry_func);

    int tid = thread_spawn("trek", (void*)entry_func, termio);

    if(res ==0){
        console_puts("elf is good\n");
        entry_func(termio);
    }
    else{
        console_printf("elf return result: %d \n", res);
    }

    console_printf("done with elf test\n");
    ioclose(exeio);

}