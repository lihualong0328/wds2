
//参考 m41t11.c
#include <string.h>
#include "i2c.h"

//void i2c_write(unsigned int slvAddr, unsigned char *buf, int len)
	//slvAddr: 8b地址，10100000，bit0=0写，所以是0xA0
	//8b地址: at24c08是8K=1024x8，1024需10b地址，8b不能表示所有地址，如图：0xA0表示只访问 at24c08(8k包含4页)的第0页
unsigned char at24cxx_read(unsigned char address)
{
	unsigned char val;
	printf("at24cxx_read address = %d\r\n", address);
    i2c_write(0xA0, &address, 1);	//把 address 写给从机
	printf("at24cxx_read send address ok\r\n");
    i2c_read(0xA0, (unsigned char *)&val, 1);	// 读
	printf("at24cxx_read get data ok\r\n");
	return val;
}

void at24cxx_write(unsigned char address, unsigned char data)
{
	unsigned char val[2];
	val[0] = address;
	val[1] = data;
    i2c_write(0xA0, val, 2);
}

	