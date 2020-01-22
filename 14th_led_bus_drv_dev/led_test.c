#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

/* led_test on
 * led_test off
 */
int main(int argc, char **argv)
{
	int fd;
	int val = 1;
	fd = open("/dev/led", O_RDWR);

	if (strcmp(argv[1], "on") == 0)
	{
		val  = 1;
	}
	else
	{
		val = 0;
	}
	
	write(fd, &val, 4);
	return 0;
}
