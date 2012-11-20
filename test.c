#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(void)
{
	int fd;
    pid_t child;

    child = fork();

	fd = open("/dev/cdata", O_RDWR);

    if (child != 0) {   // Parent
        write(fd, "ABCDE", 5);
    } else {            // Child
        write(fd, "12345", 5);
    }

    ioctl(fd, IOCTL_SYNC, 0);

	close(fd);
}
