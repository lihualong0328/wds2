
//�ο� m41t11.c
#include <string.h>
#include "i2c.h"

//void i2c_write(unsigned int slvAddr, unsigned char *buf, int len)
	//slvAddr: 8b��ַ��10100000��bit0=0д��������0xA0
	//8b��ַ: at24c08��8K=1024x8��1024��10b��ַ��8b���ܱ�ʾ���е�ַ����ͼ��0xA0��ʾֻ���� at24c08(8k����4ҳ)�ĵ�0ҳ
unsigned char at24cxx_read(unsigned char address)
{
	unsigned char val;
	printf("at24cxx_read address = %d\r\n", address);
    i2c_write(0xA0, &address, 1);	//�� address д���ӻ�
	printf("at24cxx_read send address ok\r\n");
    i2c_read(0xA0, (unsigned char *)&val, 1);	// ��
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

	