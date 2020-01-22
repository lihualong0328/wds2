#include <errno.h>
#include <unistd.h>
//#include <sysdep.h>

//#if defined(__thumb__) || defined(__ARM_EABI__)
//#define __NR_SYSCALL_BASE	0
//#else
#define __NR_SYSCALL_BASE	0x900000
//#endif

void hello(char *buf, int count)
{
	/* swi */	//需要通过执行一条swi指令进入到kernel态。void不需要输出: "=r"(newbrk)

	asm ("mov r0, %0\n"   /* save the argment in r0 */
	     "mov r1, %1\n"   /* save the argment in r0 */
		 "swi %2\n"   /* do the system call */
		 :	
		 : "r"(buf), "r"(count), "i" (__NR_SYSCALL_BASE + 352)	//新添加的CALL(sys_hello)在352处。
		 : "r0", "r1");
}

int main(int argc, char **argv)
{
	printf("in app, call hello\n");
	hello("www.100ask.net", 15);	//最终执行上面的swi指令并进入kernel态。调用Read_write.c/sys_hello()。
	return 0;
}

