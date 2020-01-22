#include "s3c24xx.h"

void putc(unsigned char c);
void puts(char *s);
void puthex(unsigned long val);


/* WOTCH DOG register */
#define 	WTCON				(*(volatile unsigned long *)0x53000000)

/* SDRAM regisers */
#define 	MEM_CTL_BASE		0x48000000
 
void disable_watch_dog();
void memsetup();

/*上电后，WATCH DOG默认是开着的，要把它关掉 */
void disable_watch_dog()
{
	WTCON	= 0;
}

/* 设置控制SDRAM的13个寄存器 */
void memsetup()
{
	int 	i = 0;
	unsigned long *p = (unsigned long *)MEM_CTL_BASE;
	volatile unsigned long *mem = (volatile unsigned long *)0x30000000;

#if 0
    /* SDRAM 13个寄存器的值 */
    unsigned long  const    mem_cfg_val[]={ 0x22011110,     //BWSCON
                                            0x00000700,     //BANKCON0
                                            0x00000700,     //BANKCON1
                                            0x00000700,     //BANKCON2
                                            0x00000700,     //BANKCON3  
                                            0x00000700,     //BANKCON4
                                            0x00000700,     //BANKCON5
                                            0x00018005,     //BANKCON6
                                            0x00018005,     //BANKCON7
                                            0x008C07A3,     //REFRESH
                                            0x000000B1,     //BANKSIZE
                                            0x00000030,     //MRSRB6
                                            0x00000030,     //MRSRB7
                                    };

	//puts("start init mem\n\r");
	putc('s');
	putc('t');
	putc('a');
	putc('r');
	putc('t');

	puthex(0x1234abcd);	//如果是puthex("0x1234abcd");


	for(; i < 13; i++)
	{
		p[i] = mem_cfg_val[i];
		putc('1'+i);
		putc(':');
		puthex(&mem_cfg_val[i]);
		putc(' ');
		puthex(mem_cfg_val[i]);
		putc('\n');
		putc('\r');
	}

	//puts("end init mem\n\r");
	putc('e');
	putc('n');
	putc('d');
#endif
	p[0] = 0x22011110;     //BWSCON	
	p[1] = 0x00000700;     //BANKCON0	
	p[2] = 0x00000700;     //BANKCON1	
	p[3] = 0x00000700;     //BANKCON2	
	p[4] = 0x00000700;     //BANKCON3  	
	p[5] = 0x00000700;     //BANKCON4	
	p[6] = 0x00000700;     //BANKCON5	
	p[7] = 0x00018005;     //BANKCON6	
	p[8] = 0x00018005;     //BANKCON7	
	p[9] = 0x008C07A3;     //REFRESH	
	p[10] = 0x000000B1;     //BANKSIZE	
	p[11] = 0x00000030;     //MRSRB6	
	p[12] = 0x00000030;     //MRSRB7

	*mem = 0x5555aaaa;
	putc('v');
	putc('a');
	putc('l');
	putc(':');
	puthex(*mem);	//mem没初始化，能打印出来？
}

#define TXD0READY   (1<<2)
#define RXD0READY   (1)

#define PCLK            12000000	//因为在调用uart0_init()时，没设置时钟(系统时钟没被提起来，所以就只能用晶振的12M)，所以PCLK != 50000000。波特率就不能到115200。
#define UART_CLK        PCLK        //  UART0的时钟源设为PCLK
#define UART_BAUD_RATE  57600      // 波特率
#define UART_BRD        ((UART_CLK  / (UART_BAUD_RATE * 16)) - 1)

/*
 * 初始化UART0
 * 57600,8N1,无流控
 */
void uart0_init(void)
{
    GPHCON  |= 0xa0;    // GPH2,GPH3用作TXD0,RXD0	//一般gpio管脚用来输出输入，也可用来做特殊功能，如串口。
    GPHUP   = 0x0c;     // GPH2,GPH3内部上拉

    ULCON0  = 0x03;     // 8N1(8个数据位，无较验，1个停止位)
    UCON0   = 0x05;     // 查询方式，UART时钟源为PCLK
    UFCON0  = 0x00;     // 不使用FIFO
    UMCON0  = 0x00;     // 不使用流控
    UBRDIV0 = UART_BRD; // 波特率为115200
}

/*
 * 发送一个字符
 */
void putc(unsigned char c)
{
    /* 等待，直到发送缓冲区中的数据已经全部发送出去 */
    while (!(UTRSTAT0 & TXD0READY));
    
    /* 向UTXH0寄存器中写入数据，UART即自动将它发送出去 */
    UTXH0 = c;
}

void puts(char *s)
{
	int i = 0;
	while (s[i])
	{
		putc(s[i]);
		i++;
	}
}

void puthex(unsigned long val)
{
	/* val = 0x1234ABCD */
	unsigned char c;
	int i = 0;
	
	putc('0');
	putc('x');

	for (i = 0; i < 8; i++)
	{
		c = (val >> ((7-i)*4)) & 0xf;
		if ((c >= 0) && (c <= 9))
		{
			c = '0' + c;
		}
		else if ((c >= 0xA) && (c <= 0xF))
		{
			c = 'A' + (c -  0xA);
		}
		putc(c);
	}
}

