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
	int cur_msg;	// ��ǰmsg�ǵڼ���msg
	int cur_ptr;	// ÿ�η�8bit, cur_ptr���cur_msg�ѷ�����B
	int state;		// �ο�i2c-s3c2410.c, ��ʾʱ��ͼ�и����׶����ݵ�����
	int err;
	wait_queue_head_t wait;
};

static struct s3c2440_i2c_xfer_data s3c2440_i2c_xfer_data;
static struct s3c2440_i2c_regs *s3c2440_i2c_regs;

static void s3c2440_i2c_start(void)
{
	s3c2440_i2c_xfer_data.state = STATE_START;
	
	if (s3c2440_i2c_xfer_data.msgs->flags & I2C_M_RD) /* �� */
	{
		s3c2440_i2c_regs->iicds		 = s3c2440_i2c_xfer_data.msgs->addr << 1;										// addr[0]=W/R
		s3c2440_i2c_regs->iicstat 	 = 0xb0;	// �������գ�����						//	������ַ������ж�
	}
	else /* д */
	{
		s3c2440_i2c_regs->iicds		 = s3c2440_i2c_xfer_data.msgs->addr << 1;
		s3c2440_i2c_regs->iicstat    = 0xf0; 		// �������ͣ�����					//	������ַ������ж�
	}
}

static void s3c2440_i2c_stop(int err)
{
	s3c2440_i2c_xfer_data.state = STATE_STOP;
	s3c2440_i2c_xfer_data.err   = err;

	PRINTK("STATE_STOP, err = %d\n", err);

	if (s3c2440_i2c_xfer_data.msgs->flags & I2C_M_RD) /* �� */
	{
		// �������лָ�I2C����������P�ź�
		s3c2440_i2c_regs->iicstat = 0x90;
		s3c2440_i2c_regs->iiccon  = 0xaf;
		ndelay(50);  // �ȴ�һ��ʱ���Ա�P�ź��Ѿ�����
	}
	else /* д */
	{
		// �������������ָ�I2C����������P�ź�
		s3c2440_i2c_regs->iicstat = 0xd0;
		s3c2440_i2c_regs->iiccon  = 0xaf;
		ndelay(50);  // �ȴ�һ��ʱ���Ա�P�ź��Ѿ�����
	}

	/* ����app */
	wake_up(&s3c2440_i2c_xfer_data.wait);
}

static int s3c2440_i2c_xfer(struct i2c_adapter *adap,
			struct i2c_msg *msgs, int num)									// �ο�i2c����ж�i2c�������Ĳ���
{
	unsigned long timeout;
	
	/* ��num��msg��I2C���ݷ��ͳ�ȥ/������ */
	s3c2440_i2c_xfer_data.msgs    = msgs;
	s3c2440_i2c_xfer_data.msn_num = num;
	s3c2440_i2c_xfer_data.cur_msg = 0;
	s3c2440_i2c_xfer_data.cur_ptr = 0;
	s3c2440_i2c_xfer_data.err     = -ENODEV;	// ����ʱ����Ϊ0

	s3c2440_i2c_start();
	/* ���� */	// app����adapter���s3c2440_i2c_xfer(), ����ʱ����, �����껽�Ѳ�����
	// timeout: ����һ��ʱ��, ��Ϊi2c����������, ���������������ִ��
	timeout = wait_event_timeout(s3c2440_i2c_xfer_data.wait, (s3c2440_i2c_xfer_data.state == STATE_STOP), HZ * 5);	//(s3c2440_i2c_xfer_data.state == STATE_STOP)������ʱ���� 
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

/* 1. ����/����i2c_adapter
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
	return (s3c2440_i2c_xfer_data.cur_ptr >= s3c2440_i2c_xfer_data.msgs->len);	// 1>=2: cur_ptr=1��ʾ���ڴ�����û������
}

static int isLastData(void)
{
	return (s3c2440_i2c_xfer_data.cur_ptr == s3c2440_i2c_xfer_data.msgs->len - 1);	// 1==1�������������һ��
}

static irqreturn_t s3c2440_i2c_xfer_irq(int irq, void *dev_id)
{
	unsigned int iicSt;
  iicSt = s3c2440_i2c_regs->iicstat; 					// ��ȡ�ж�״̬

	if(iicSt & 0x8){ printk("Bus arbitration failed\n\r"); }

	switch (s3c2440_i2c_xfer_data.state)
	{
		case STATE_START : /* ����S���豸��ַ��, �����ж� */
		{
			PRINTK("Start\n");
			/* ���û��ACK, ���ش��� */
			if (iicSt & S3C2410_IICSTAT_LASTBIT)	// �ο�i2c-s3c2410.c
			{
				s3c2440_i2c_stop(-ENODEV);
				break;
			}

			if (isLastMsg() && isEndData())				// ö���豸ʱ��ֻ��start���豸��ַ, ���ȴ�ack, ��ʱmsg.len=0, �����ж����һ����Ϣ && ���һ������
			{
				s3c2440_i2c_stop(0);								// �ɹ�ֹͣ
				break;
			}

			/* ������һ��״̬ */
			if (s3c2440_i2c_xfer_data.msgs->flags & I2C_M_RD) /* �� */
			{
				s3c2440_i2c_xfer_data.state = STATE_READ;
				goto next_read;
			}
			else
			{
				s3c2440_i2c_xfer_data.state = STATE_WRITE;
			}	
		}
		// break;	// ֱ�ӽ���case STATE_WRITE:
		case STATE_WRITE:
		{
			PRINTK("STATE_WRITE\n");
			/* ���û��ACK, ���ش��� */	// 2440ÿдһ������, �����յ�ack��ȷ���Ƿ�д�ɹ�
			if (iicSt & S3C2410_IICSTAT_LASTBIT)
			{
				s3c2440_i2c_stop(-ENODEV);
				break;
			}

			if (!isEndData())  /* �����ǰmsg��������Ҫ���� */	// д��֮����յ�ack, �ٴν����жϴ�����
			{
				s3c2440_i2c_regs->iicds = s3c2440_i2c_xfer_data.msgs->buf[s3c2440_i2c_xfer_data.cur_ptr];
				s3c2440_i2c_xfer_data.cur_ptr++;
				
				ndelay(50);	// ������д��IICDS����Ҫһ��ʱ����ܳ�����SDA����
				
				s3c2440_i2c_regs->iiccon = 0xaf;		// �ָ�I2C����
				break;				
			}
			else if (!isLastMsg())
			{
				/* ��ʼ������һ����Ϣ */
				s3c2440_i2c_xfer_data.msgs++;
				s3c2440_i2c_xfer_data.cur_msg++;
				s3c2440_i2c_xfer_data.cur_ptr = 0;
				s3c2440_i2c_xfer_data.state = STATE_START;	// ������һ����Ϣǰ���϶�Ҳ�ȷ�start���豸��ַ
				/* ����START�źźͷ����豸��ַ */
				s3c2440_i2c_start();
				break;
			}
			else
			{
				/* �����һ����Ϣ�����һ������ */
				s3c2440_i2c_stop(0);
				break;				
			}

			break;
		}

		case STATE_READ:
		{
			// һ��STATE_READ�������ack���⣬�϶�������
			PRINTK("STATE_READ\n");
			/* �������� */
			s3c2440_i2c_xfer_data.msgs->buf[s3c2440_i2c_xfer_data.cur_ptr] = s3c2440_i2c_regs->iicds;			
			s3c2440_i2c_xfer_data.cur_ptr++;
next_read:	// ��STATE_START��ack������, ���ﻹû����
			if (!isEndData()) /* �������û��д, �������� ������ */
			{
				if (isLastData())  /* ��������������������һ��, ����ack��i2c�豸 */
				{
					s3c2440_i2c_regs->iiccon = 0x2f;   // �ָ�I2C���䣬���յ���һ����ʱ��ACK��i2c�豸, ֱ��stop
				}
				else
				{
					s3c2440_i2c_regs->iiccon = 0xaf;   // �ָ�I2C���䣬���յ���һ����ʱ����ACK��i2c�豸
				}				
				break;
			}
			else if (!isLastMsg())
			{
				/* ��ʼ������һ����Ϣ */
				s3c2440_i2c_xfer_data.msgs++;
				s3c2440_i2c_xfer_data.cur_msg++;
				s3c2440_i2c_xfer_data.cur_ptr = 0;
				s3c2440_i2c_xfer_data.state = STATE_START;
				/* ����START�źźͷ����豸��ַ */
				s3c2440_i2c_start();
				break;
			}
			else
			{
				/* �����һ����Ϣ�����һ������ */
				s3c2440_i2c_stop(0);
				break;								
			}
			break;
		}

		default: break;
	}

	/* ���ж� */	// ��ͼ��i2c���������ж��źŸ�cpu, kernel�Դ��Ĵ���������жϿ���������ж��������û���i2c�������е��ж�
	s3c2440_i2c_regs->iiccon &= ~(S3C2410_IICCON_IRQPEND);	// s3c2440.pdf

	return IRQ_HANDLED;	
}

/*
 * I2C��ʼ��
 */
static void s3c2440_i2c_init(void)	// �ο�i2c���
{
	struct clk *clk;
	
	// �ο�i2c-s3c2410.o/probe/, linuxΪ��ʡ��, �ϵ�ʱ, �ò�����ģ�鶼�ر�, ʹ��ʱ��
	clk = clk_get(NULL, "i2c");
	clk_enable(clk);
	
  // GPEUP  |= 0xc000;       // ��ֹ�ڲ�����															// �ڲ���������ν, mini2440��·ͼ�����ⲿ��������
  // GPECON |= 0xa0000000;   // ѡ�����Ź��ܣ�GPE15:IICSDA, GPE14:IICSCL  // ��ioremap()������
  s3c_gpio_cfgpin(S3C2410_GPE(14), S3C2410_GPE14_IICSCL);
	s3c_gpio_cfgpin(S3C2410_GPE(15), S3C2410_GPE15_IICSDA);

    // INTMSK &= ~(BIT_IIC);	// �����ж�


    /* bit[7] = 1, ʹ��ACK
     * bit[6] = 0, IICCLK = PCLK/16
     * bit[5] = 1, ʹ���ж�
     * bit[3:0] = 0xf, Tx clock = IICCLK/16
     * PCLK = 50MHz, IICCLK = 3.125MHz, Tx Clock = 0.195MHz
     */
    s3c2440_i2c_regs->iiccon = (1<<7) | (0<<6) | (1<<5) | (0xf);  // 0xaf
    s3c2440_i2c_regs->iicadd  = 0x10;     // S3C24xx slave address = [7:1]
    s3c2440_i2c_regs->iicstat = 0x10;     // I2C�������ʹ��(Rx/Tx)
}

static int i2c_bus_s3c2440_init(void)
{
	/* 2. Ӳ����ص����� */
	s3c2440_i2c_regs = ioremap(0x54000000, sizeof(struct s3c2440_i2c_regs));
	
	s3c2440_i2c_init();

	request_irq(IRQ_IIC, s3c2440_i2c_xfer_irq, 0, "s3c2440-i2c", NULL);	// �����ж�ʱ��arg5 dev_id����ʡ�ԣ�IRQ_IIC�ǹ���

	init_waitqueue_head(&s3c2440_i2c_xfer_data.wait);
	
	/* 3. ע��i2c_adapter */	// ע��֮��, ���Ͽ�����
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
