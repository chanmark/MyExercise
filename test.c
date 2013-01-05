#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(void)
{
	int fd;
	unsigned char *fb;
	int i;
	char p[4] = {0x33,0,0,0};


	fd = open("/dev/cdata",O_RDWR);

	if( fd == NULL )
		printf("open file error");

	for( i=0;i<4096;i++){
		write(fd, p, 1);
	}
	close(fd);
}
