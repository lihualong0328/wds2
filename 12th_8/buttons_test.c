#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

int fd;

int main(int argc, char **argv)
{
	unsigned char key_val;
	int ret;
	
	fd = open("/dev/buttons", O_RDWR);

	while (1)
	{
		ret = read(fd, &key_val, 1);
		printf("key_val: 0x%x, ret = %d\n", key_val, ret);
		//sleep(5);
	}
	
	return 0;
}

