#include <io/stddev.h>

ocdev_t io_std_ocdev;

void io_set_std_ocdev(const ocdev_t ocdev) {
    io_std_ocdev = ocdev;
}

const ocdev_t io_get_std_ocdev(void) {
    return io_std_ocdev;
}