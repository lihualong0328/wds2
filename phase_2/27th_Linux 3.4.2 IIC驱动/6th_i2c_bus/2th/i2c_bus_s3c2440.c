#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of_i2c.h>
#include <linux/of_gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <asm/irq.h>
#include <plat/regs-iic.h>
#include <plat/iic.h>

//#define PRINTK printk
#define PRINTK(...) 

enum s3c24xx_i2c_state {
	STATE_IDLE,
	STATE_START,
	STATE_READ,
	STATE_WRITE,
	STATE_STOP
};

struct s3c2440_i2c_regs {
	unsigned int iiccon;
	unsigned int iicstat;
	unsigned int iicadd;
	unsigned int iicds;
	unsigned int iiclc;
};

struct s3c2440_i2c_xfer_data {
	struct i2c_msg *msgs;
	int msn_num;
	int cur_msg;	// 当前msg是第几个msg
	int cur_ptr;	// 每次发8bit, cur_ptr标记cur_msg已发多少B
	int state;		// 参考i2c-s3c2410.c, 表示时序图中各个阶段数据的意义
	int err;
	wait_queue_head_t wait;
};

static struct s3c2440_i2c_xfer_data s3c2440_i2c_xfer_data;
static struct s3c2440_i2c_regs *s3c2440_i2c_regs;

static void s3c2440_i2c_start(void)
{
	s3c2440_i2c_xfer_data.state = STATE_START;
	
	if (s3c2440_i2c_xfer_data.msgs->flags & I2C_M_RD) /* 读 */
	{
		s3c2440_i2c_regs->iicds		 = s3c2440_i2c_xfer_data.msgs->addr << 1;										// addr[0]=W/R
		s3c2440_i2c_regs->iicstat 	 = 0xb0;	// 主机接收，启动						//	发出地址后产生中断
	}
	else /* 写 */
	{
		s3c2440_i2c_regs->iicds		 = s3c2440_i2c_xfer_data.msgs->addr << 1;
		s3c2440_i2c_regs->iicstat    = 0xf0; 		// 主机发送，启动					//	发出地址后产生中断
	}
}

static void s3c2440_i2c_stop(int err)
{
	s3c2440_i2c_xfer_data.state = STATE_STOP;
	s3c2440_i2c_xfer_data.err   = err;

	PRINTK("STATE_STOP, err = %d\n", err);

	if (s3c2440_i2c_xfer_data.msgs->flags & I2C_M_RD) /* 读 */
	{
		// 下面两行恢复I2C操作，发出P信号
		s3c2440_i2c_regs->iicstat = 0x90;
		s3c2440_i2c_regs->iiccon  = 0xaf;
		ndelay(50);  // 等待一段时间以便P信号已经发出
	}
	else /* 写 */
	{
		// 下面两行用来恢复I2C操作，发出P信号
		s3c2440_i2c_regs->iicstat = 0xd0;
		s3c2440_i2c_regs->iiccon  = 0xaf;
		ndelay(50);  // 等待一段时间以便P信号已经发出
	}

	/* 唤醒app */
	wake_up(&s3c2440_i2c_xfer_data.wait);
}

static int s3c2440_i2c_xfer(struct i2c_adapter *adap,
			struct i2c_msg *msgs, int num)									// 参考i2c裸板中对i2c控制器的操作
{
	unsigned long timeout;
	
	/* 把num个msg的I2C数据发送出去/读进来 */
	s3c2440_i2c_xfer_data.msgs    = msgs;
	s3c2440_i2c_xfer_data.msn_num = num;
	s3c2440_i2c_xfer_data.cur_msg = 0;
	s3c2440_i2c_xfer_data.cur_ptr = 0;
	s3c2440_i2c_xfer_data.err     = -ENODEV;	// 结束时设置为0

	s3c2440_i2c_start();
	/* 休眠 */	// app调用adapter层的s3c2440_i2c_xfer(), 传输时休眠, 传输完唤醒并返回
	// timeout: 休眠一段时间, 因为i2c可能有问题, 被唤醒则继续向下执行
	timeout = wait_event_timeout(s3c2440_i2c_xfer_data.wait, (s3c2440_i2c_xfer_data.state == STATE_STOP), HZ * 5);	//(s3c2440_i2c_xfer_data.state == STATE_STOP)不成立时休眠 
	if (0 == timeout)
	{
		printk("s3c2440_i2c_xfer time out\n");
		return -ETIMEDOUT;
	}
	else
	{
		return s3c2440_i2c_xfer_data.err;
	}
}

static u32 s3c2440_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL | I2C_FUNC_PROTOCOL_MANGLING;
}

static const struct i2c_algorithm s3c2440_i2c_algo = {
//	.smbus_xfer     = ,
	.master_xfer	= s3c2440_i2c_xfer,
	.functionality	= s3c2440_i2c_func,
};

/* 1. 分配/设置i2c_adapter
 */
static struct i2c_adapter s3c2440_i2c_adapter = {
 .name			 = "s3c2440_100ask",
 .algo			 = &s3c2440_i2c_algo,
 .owner 		 = THIS_MODULE,
};

static int isLastMsg(void)
{
	return (s3c2440_i2c_xfer_data.cur_msg == s3c2440_i2c_xfer_data.msn_num - 1);
}

static int isEndData(void)
{
	return (s3c2440_i2c_xfer_data.cur_ptr >= s3c2440_i2c_xfer_data.msgs->len);	// 1>=2: cur_ptr=1表示正在处理，还没处理完
}

static int isLastData(void)
{
	return (s3c2440_i2c_xfer_data.cur_ptr == s3c2440_i2c_xfer_data.msgs->len - 1);	// 1==1：即将处理最后一个
}

static irqreturn_t s3c2440_i2c_xfer_irq(int irq, void *dev_id)
{
	unsigned int iicSt;
  iicSt = s3c2440_i2c_regs->iicstat; 					// 读取中断状态

	if(iicSt & 0x8){ printk("Bus arbitration failed\n\r"); }

	switch (s3c2440_i2c_xfer_data.state)
	{
		case STATE_START : /* 发出S和设备地址后, 产生中断 */
		{
			PRINTK("Start\n");
			/* 如果没有ACK, 返回错误 */
			if (iicSt & S3C2410_IICSTAT_LASTBIT)	// 参考i2c-s3c2410.c
			{
				s3c2440_i2c_stop(-ENODEV);
				break;
			}

			if (isLastMsg() && isEndData())				// 枚举设备时，只发start和设备地址, 并等待ack, 此时msg.len=0, 这里判断最后一个消息 && 最后一个数据
			{
				s3c2440_i2c_stop(0);								// 成功停止
				break;
			}

			/* 进入下一个状态 */
			if (s3c2440_i2c_xfer_data.msgs->flags & I2C_M_RD) /* 读 */
			{
				s3c2440_i2c_xfer_data.state = STATE_READ;
				goto next_read;
			}
			else
			{
				s3c2440_i2c_xfer_data.state = STATE_WRITE;
			}	
		}
		// break;	// 直接进入case STATE_WRITE:
		case STATE_WRITE:
		{
			PRINTK("STATE_WRITE\n");
			/* 如果没有ACK, 返回错误 */	// 2440每写一个数据, 必须收到ack才确定是否写成功
			if (iicSt & S3C2410_IICSTAT_LASTBIT)
			{
				s3c2440_i2c_stop(-ENODEV);
				break;
			}

			if (!isEndData())  /* 如果当前msg还有数据要发送 */	// 写完之后会收到ack, 再次进入中断处理函数
			{
				s3c2440_i2c_regs->iicds = s3c2440_i2c_xfer_data.msgs->buf[s3c2440_i2c_xfer_data.cur_ptr];
				s3c2440_i2c_xfer_data.cur_ptr++;
				
				ndelay(50);	// 将数据写入IICDS后，需要一段时间才能出现在SDA线上
				
				s3c2440_i2c_regs->iiccon = 0xaf;		// 恢复I2C传输
				break;				
			}
			else if (!isLastMsg())
			{
				/* 开始处理下一个消息 */
				s3c2440_i2c_xfer_data.msgs++;
				s3c2440_i2c_xfer_data.cur_msg++;
				s3c2440_i2c_xfer_data.cur_ptr = 0;
				s3c2440_i2c_xfer_data.state = STATE_START;	// 处理下一个消息前，肯定也先发start和设备地址
				/* 发出START信号和发出设备地址 */
				s3c2440_i2c_start();
				break;
			}
			else
			{
				/* 是最后一个消息的最后一个数据 */
				s3c2440_i2c_stop(0);
				break;				
			}

			break;
		}

		case STATE_READ:
		{
			// 一旦STATE_READ后产生的ack到这，肯定有数据
			PRINTK("STATE_READ\n");
			/* 读出数据 */
			s3c2440_i2c_xfer_data.msgs->buf[s3c2440_i2c_xfer_data.cur_ptr] = s3c2440_i2c_regs->iicds;			
			s3c2440_i2c_xfer_data.cur_ptr++;
next_read:	// 从STATE_START的ack过来的, 这里还没读到
			if (!isEndData()) /* 如果数据没读写, 继续发起 读操作 */
			{
				if (isLastData())  /* 如果即将读的数据是最后一个, 不发ack给i2c设备 */
				{
					s3c2440_i2c_regs->iiccon = 0x2f;   // 恢复I2C传输，接收到下一数据时无ACK给i2c设备, 直接stop
				}
				else
				{
					s3c2440_i2c_regs->iiccon = 0xaf;   // 恢复I2C传输，接收到下一数据时发出ACK给i2c设备
				}				
				break;
			}
			else if (!isLastMsg())
			{
				/* 开始处理下一个消息 */
				s3c2440_i2c_xfer_data.msgs++;
				s3c2440_i2c_xfer_data.cur_msg++;
				s3c2440_i2c_xfer_data.cur_ptr = 0;
				s3c2440_i2c_xfer_data.state = STATE_START;
				/* 发出START信号和发出设备地址 */
				s3c2440_i2c_start();
				break;
			}
			else
			{
				/* 是最后一个消息的最后一个数据 */
				s3c2440_i2c_stop(0);
				break;								
			}
			break;
		}

		default: break;
	}

	/* 清中断 */	// 如图：i2c控制器发中断信号给cpu, kernel自带的处理函数会把中断控制器里的中断清除，但没清除i2c控制器中的中断
	s3c2440_i2c_regs->iiccon &= ~(S3C2410_IICCON_IRQPEND);	// s3c2440.pdf

	return IRQ_HANDLED;	
}

/*
 * I2C初始化
 */
static void s3c2440_i2c_init(void)	// 参考i2c裸板
{
	struct clk *clk;
	
	// 参考i2c-s3c2410.o/probe/, linux为了省电, 上电时, 用不到的模块都关闭, 使用时打开
	clk = clk_get(NULL, "i2c");
	clk_enable(clk);
	
  // GPEUP  |= 0xc000;       // 禁止内部上拉															// 内部上拉无所谓, mini2440电路图已有外部上拉电阻
  // GPECON |= 0xa0000000;   // 选择引脚功能：GPE15:IICSDA, GPE14:IICSCL  // 可ioremap()再设置
  s3c_gpio_cfgpin(S3C2410_GPE(14), S3C2410_GPE14_IICSCL);
	s3c_gpio_cfgpin(S3C2410_GPE(15), S3C2410_GPE15_IICSDA);

    // INTMSK &= ~(BIT_IIC);	// 不管中断


    /* bit[7] = 1, 使能ACK
     * bit[6] = 0, IICCLK = PCLK/16
     * bit[5] = 1, 使能中断
     * bit[3:0] = 0xf, Tx clock = IICCLK/16
     * PCLK = 50MHz, IICCLK = 3.125MHz, Tx Clock = 0.195MHz
     */
    s3c2440_i2c_regs->iiccon = (1<<7) | (0<<6) | (1<<5) | (0xf);  // 0xaf
    s3c2440_i2c_regs->iicadd  = 0x10;     // S3C24xx slave address = [7:1]
    s3c2440_i2c_regs->iicstat = 0x10;     // I2C串行输出使能(Rx/Tx)
}

static int i2c_bus_s3c2440_init(void)
{
	/* 2. 硬件相关的设置 */
	s3c2440_i2c_regs = ioremap(0x54000000, sizeof(struct s3c2440_i2c_regs));
	
	s3c2440_i2c_init();

	request_irq(IRQ_IIC, s3c2440_i2c_xfer_irq, 0, "s3c2440-i2c", NULL);	// 共享中断时，arg5 dev_id不可省略，IRQ_IIC非共享

	init_waitqueue_head(&s3c2440_i2c_xfer_data.wait);
	
	/* 3. 注册i2c_adapter */	// 注册之后, 马上可以用
	i2c_add_adapter(&s3c2440_i2c_adapter);
	
	return 0;
}

static void i2c_bus_s3c2440_exit(void)
{
	i2c_del_adapter(&s3c2440_i2c_adapter);
	free_irq(IRQ_IIC, NULL);
	iounmap(s3c2440_i2c_regs);
}

module_init(i2c_bus_s3c2440_init);
module_exit(i2c_bus_s3c2440_exit);
MODULE_LICENSE("GPL");
