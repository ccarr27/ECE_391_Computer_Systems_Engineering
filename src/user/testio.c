#include "io.h"
#include "string.h"

void main() {
    char buffer[128];  // A memory buffer that will act like a "file"
    struct io_lit memory_io;

    // Initialize the io_lit object with the buffer
    struct io_intf * io = iolit_init(&memory_io, buffer, sizeof(buffer));

    // Write data to the buffer as if it's a file
    const char * message = "This is a test message.";
    iowrite(io, message, strlen(message));

    // Set the position to the beginning of the buffer
    ioctl(io, IOCTL_SETPOS, (void *)0);

    // Read the data back from the buffer
    char read_buf[128];
    long bytes_read = ioread(io, read_buf, sizeof(read_buf) - 1);
    if (bytes_read >= 0) {
        read_buf[bytes_read] = '\0';  // Null-terminate the string

        // Output the contents of the buffer using ioputs
        ioputs(io, read_buf);
    }

    // Close the io_lit object
    ioclose(io);
}