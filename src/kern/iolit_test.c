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
#include "io.h"

//           end of kernel image (defined in kernel.ld)
extern char _kimg_end[];


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



// static void shell_main(struct io_intf * termio);

void main(void) {
    struct io_intf * io;
    void * mmio_base;
    // int result;
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

    for (i = 0; i < 8; i++) {
        mmio_base = (void*)VIRT0_IOBASE;
        mmio_base += (VIRT1_IOBASE-VIRT0_IOBASE)*i;
        virtio_attach(mmio_base, VIRT0_IRQNO+i);
    }

    intr_enable();
    timer_start();

    struct io_lit * lit = kmalloc(sizeof(struct  io_lit));

    int buffer[128];  // A memory buffer that will act like a "file"

    io = iolit_init(lit, buffer, sizeof(buffer));
    console_printf("io init done \n");
    int io_length;
    //get length test
    io->ops->ctl(io, IOCTL_GETLEN, &io_length);;
    console_printf("get length of buff through io_length: %d\n", io_length);
    int io_pos;
    //get the curr position
    io->ops->ctl(io, IOCTL_GETPOS, &io_pos);
    console_printf("curr pos: %d\n", io_pos);

    const int message = 5;
    //io write test
    console_printf("%d\n", message);
    int write_test = io->ops->write(io, &message, sizeof(int));
    console_printf("io write return: %d \n", write_test);
    console_printf("io buf check val: %d \n", buffer[0]);

    //get the curr position
    io->ops->ctl(io, IOCTL_GETPOS, &io_pos);
    console_printf("curr pos after calling write: %d\n", io_pos);

    //now set the curr postion
    size_t new_pos = 0;
    int setpos_ret = io->ops->ctl(io, IOCTL_SETPOS, &new_pos);
    console_printf("set positon return value: %d\n", setpos_ret);

    //get the curr position
    io->ops->ctl(io, IOCTL_GETPOS, &io_pos);
    console_printf("curr pos after setting it: %d\n", io_pos);


    //now in order to test write we can test read
    int recieve_buffer[128];    //make a buffer to return to
    io->ops->read(io, recieve_buffer, sizeof(int));
    console_printf("First character in buffer: %d\n", recieve_buffer[0]); // Print the first character in buffer to check if it was written correctly


    //get the curr position
    io->ops->ctl(io, IOCTL_GETPOS, &io_pos);
    console_printf("curr pos: %d\n", io_pos);


}
