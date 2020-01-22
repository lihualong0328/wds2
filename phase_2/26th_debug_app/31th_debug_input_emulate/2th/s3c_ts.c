#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#include <asm/plat-s3c24xx/ts.h>

#include <asm/arch/regs-adc.h>
#include <asm/arch/regs-gpio.h>

struct s3c_ts_regs {
	unsigned long adccon;
	unsigned long adctsc;
	unsigned long adcdly;
	unsigned long adcdat0;
	unsigned long adcdat1;
	unsigned long adcupdn;
};

static struct input_dev *s3c_ts_dev;
static volatile struct s3c_ts_regs *s3c_ts_regs;

static struct timer_list ts_timer;

#define MYLOG_BUF_LEN (1024*1024)
#define INPUT_REPLAY   0
#define INPUT_TAG      1

static char *replay_buf;
static int replay_r = 0;
static int replay_w = 0;
static int major = 0;
static struct class *cls;
static struct timer_list replay_timer;

extern int myprintk(const char *fmt, ...);

static ssize_t replay_write(struct file * file, const char __user *buf, size_t size, loff_t *offset)
{
	int err;
	
	/* ��Ӧ�ó����������д��replay_buf */
	if (replay_w + size >= MYLOG_BUF_LEN)
	{
		printk("replay_buf full!\n");
		return -EIO;
	}
	
	err = copy_from_user(replay_buf + replay_w, buf, size);
	if (err)
	{
		return -EIO;
	}
	else
	{
		replay_w += size;
	}

	return size;
}

/* app: ioctl(fd, CMD, ..); */
static int replay_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	char buf[100];
	switch (cmd)
	{
		case INPUT_REPLAY:
		{
			/* �����ط�: ����replay_buf����������ϱ��¼� */
			replay_timer.expires = jiffies + 1;
			printk("replay_ioctl add_timer\n");
			add_timer(&replay_timer);
			break;
		}
		case INPUT_TAG:
		{
			copy_from_user(buf, (const void __user *)arg, 100);
			buf[99] = '\0';
			myprintk("%s\n", buf);
			break;
		}
	}
	
	return 0;
}

/* ����ֵ: 0 - ������ */
static int replay_get_line(char *line)
{
	int i = 0;
	
	/* �Ե�ǰ���Ŀո񡢻س��� */
	while (replay_r <= replay_w)
	{
		if ((replay_buf[replay_r] == ' ') || (replay_buf[replay_r] == '\n') || (replay_buf[replay_r] == '\r') || (replay_buf[replay_r] == '\t'))
			replay_r++;
		else
			break;
	}
	// �洢�������ݡ�
	while (replay_r <= replay_w)
	{
		if ((replay_buf[replay_r] == '\n') || (replay_buf[replay_r] == '\r'))
			break;
		else
		{
			line[i] = replay_buf[replay_r];
			replay_r++;	
			i++;
		}
	}

	line[i] = '\0';
	return i;	
}

static void input_replay_timer_func(unsigned long data)
{
	/* ��replay_buf���һЩ����ȡ�����ϱ� 
	 * ������1������, ȷ��timeֵ, �ϱ���1��
	 * ��������1������, �������time���ڵ�1�е�time, �ϱ�
	 *                  ����: mod_timer  
	 */

	unsigned int time;
	unsigned int type;
	unsigned int code;
	int val;

	static unsigned int pre_time = 0, pre_type = 0, pre_code = 0;
	static int pre_val = 0;

	static int cnt = 0;

	
	char line[100];	//1��<100�ַ�
	int ret;

	//printk("input_replay_timer_func : %d\n", cnt++);

	if (pre_time != 0)
	{
		/* �ϱ��¼� */
		input_event(s3c_ts_dev, pre_type, pre_code, pre_val);
	}
	
	while (1)
	{
		ret = replay_get_line(line);
		if (ret == 0)
		{
			printk("end of input replay\n");
			del_timer(&replay_timer);
			pre_time = pre_type = pre_code = 0;
			pre_val = 0;
			replay_r = replay_w = 0;
			break;
		}

		/* �������� */
		time = 0;
		type = 0;
		code = 0;
		val  = 0;
		sscanf(line, "%x %x %x %d", &time, &type, &code, &val);

		//printk("%x %x %x %d\n", time, type, code, val);
		
		if (!time && !type && !code && !val)
			continue;
		else
		{
			if ((pre_time == 0) || (time == pre_time))
			{
				/* �ϱ��¼� */
				input_event(s3c_ts_dev, type, code, val);
				
				if (pre_time == 0)
					pre_time = time;
			}
			else
			{
				/* ������һ��Ҫ�ϱ������ݵ�ʱ�� mod_timer */
				mod_timer(&replay_timer, jiffies + (time - pre_time));				

				pre_time = time;
				pre_type = type;
				pre_code = code;
				pre_val  = val;
				
				break;
			}
		}
	}
	
}

static struct file_operations replay_fops = {
	.owner   = THIS_MODULE,
	.write   = replay_write,
	.ioctl   = replay_ioctl,
};

static void enter_wait_pen_down_mode(void)
{
	s3c_ts_regs->adctsc = 0xd3;
}

static void enter_wait_pen_up_mode(void)
{
	s3c_ts_regs->adctsc = 0x1d3;
}

static void enter_measure_xy_mode(void)
{
	s3c_ts_regs->adctsc = (1<<3)|(1<<2);
}

static void start_adc(void)
{
	s3c_ts_regs->adccon |= (1<<0);
}

void write_input_event_to_file(unsigned int time, unsigned int type, unsigned int code, int val)
{
	myprintk("0x%08x 0x%08x 0x%08x %d\n", time, type, code, val);	
}

static int s3c_filter_ts(int x[], int y[])
{
#define ERR_LIMIT 10

	int avr_x, avr_y;
	int det_x, det_y;

	avr_x = (x[0] + x[1])/2;
	avr_y = (y[0] + y[1])/2;

	det_x = (x[2] > avr_x) ? (x[2] - avr_x) : (avr_x - x[2]);
	det_y = (y[2] > avr_y) ? (y[2] - avr_y) : (avr_y - y[2]);

	if ((det_x > ERR_LIMIT) || (det_y > ERR_LIMIT))
		return 0;

	avr_x = (x[1] + x[2])/2;
	avr_y = (y[1] + y[2])/2;

	det_x = (x[3] > avr_x) ? (x[3] - avr_x) : (avr_x - x[3]);
	det_y = (y[3] > avr_y) ? (y[3] - avr_y) : (avr_y - y[3]);

	if ((det_x > ERR_LIMIT) || (det_y > ERR_LIMIT))
		return 0;
	
	return 1;
}

static void s3c_ts_timer_function(unsigned long data)
{
	if (s3c_ts_regs->adcdat0 & (1<<15))
	{
		/* �Ѿ��ɿ� : �ϱ����Ҵ�ӡ��procȥ  
		 * jiffies, type, code, value
		 */
		input_report_abs(s3c_ts_dev, ABS_PRESSURE, 0);
		write_input_event_to_file(jiffies, EV_ABS, ABS_PRESSURE, 0);
		
		input_report_key(s3c_ts_dev, BTN_TOUCH, 0);
		write_input_event_to_file(jiffies, EV_KEY, BTN_TOUCH, 0);

		input_sync(s3c_ts_dev);
		write_input_event_to_file(jiffies, EV_SYN, SYN_REPORT, 0);
		
		enter_wait_pen_down_mode();
	}
	else
	{
		/* ����X/Y���� */
		enter_measure_xy_mode();
		start_adc();
	}
}


static irqreturn_t pen_down_up_irq(int irq, void *dev_id)
{
	if (s3c_ts_regs->adcdat0 & (1<<15))
	{
		//printk("pen up\n");
		input_report_abs(s3c_ts_dev, ABS_PRESSURE, 0);
		write_input_event_to_file(jiffies, EV_ABS, ABS_PRESSURE, 0);

		input_report_key(s3c_ts_dev, BTN_TOUCH, 0);
		write_input_event_to_file(jiffies, EV_KEY, BTN_TOUCH, 0);

		input_sync(s3c_ts_dev);
		write_input_event_to_file(jiffies, EV_SYN, SYN_REPORT, 0);

		enter_wait_pen_down_mode();
	}
	else
	{
		//printk("pen down\n");
		//enter_wait_pen_up_mode();
		enter_measure_xy_mode();
		start_adc();
	}
	return IRQ_HANDLED;
}

static irqreturn_t adc_irq(int irq, void *dev_id)
{
	static int cnt = 0;
	static int x[4], y[4];
	int adcdat0, adcdat1;
	
	
	/* �Ż���ʩ2: ���ADC���ʱ, ���ִ������Ѿ��ɿ�, �����˴ν�� */
	adcdat0 = s3c_ts_regs->adcdat0;
	adcdat1 = s3c_ts_regs->adcdat1;

	if (s3c_ts_regs->adcdat0 & (1<<15))
	{
		/* �Ѿ��ɿ� */
		cnt = 0;
		input_report_abs(s3c_ts_dev, ABS_PRESSURE, 0);
		write_input_event_to_file(jiffies, EV_ABS, ABS_PRESSURE, 0);
		
		input_report_key(s3c_ts_dev, BTN_TOUCH, 0);
		write_input_event_to_file(jiffies, EV_KEY, BTN_TOUCH, 0);
		
		input_sync(s3c_ts_dev);
		write_input_event_to_file(jiffies, EV_SYN, SYN_REPORT, 0);
		
		enter_wait_pen_down_mode();
	}
	else
	{
		// printk("adc_irq cnt = %d, x = %d, y = %d\n", ++cnt, adcdat0 & 0x3ff, adcdat1 & 0x3ff);
		/* �Ż���ʩ3: ��β�����ƽ��ֵ */
		x[cnt] = adcdat0 & 0x3ff;
		y[cnt] = adcdat1 & 0x3ff;
		++cnt;
		if (cnt == 4)
		{
			/* �Ż���ʩ4: �������� */
			if (s3c_filter_ts(x, y))
			{			
				//printk("x = %d, y = %d\n", (x[0]+x[1]+x[2]+x[3])/4, (y[0]+y[1]+y[2]+y[3])/4);
				input_report_abs(s3c_ts_dev, ABS_X, (x[0]+x[1]+x[2]+x[3])/4);
				write_input_event_to_file(jiffies, EV_ABS, ABS_X, (x[0]+x[1]+x[2]+x[3])/4);

				input_report_abs(s3c_ts_dev, ABS_Y, (y[0]+y[1]+y[2]+y[3])/4);
				write_input_event_to_file(jiffies, EV_ABS, ABS_Y, (y[0]+y[1]+y[2]+y[3])/4);
				
				input_report_abs(s3c_ts_dev, ABS_PRESSURE, 1);
				write_input_event_to_file(jiffies, EV_ABS, ABS_PRESSURE, 1);

				input_report_key(s3c_ts_dev, BTN_TOUCH, 1);
				write_input_event_to_file(jiffies, EV_KEY, BTN_TOUCH, 1);

				input_sync(s3c_ts_dev);
				write_input_event_to_file(jiffies, EV_SYN, SYN_REPORT, 0);
			}
			cnt = 0;
			enter_wait_pen_up_mode();

			/* ������ʱ����������/��������� */
			mod_timer(&ts_timer, jiffies + HZ/100);
		}
		else
		{
			enter_measure_xy_mode();
			start_adc();
		}		
	}
	
	return IRQ_HANDLED;
}

static int s3c_ts_init(void)
{
	struct clk* clk;

	replay_buf = kmalloc(MYLOG_BUF_LEN, GFP_KERNEL);
	if (!replay_buf)
	{
		printk("can't alloc for mylog_buf\n");
		return -EIO;
	}

	
	/* 1. ����һ��input_dev�ṹ�� */
	s3c_ts_dev = input_allocate_device();

	/* 2. ���� */
	/* 2.1 �ܲ��������¼� */
	set_bit(EV_KEY, s3c_ts_dev->evbit);
	set_bit(EV_ABS, s3c_ts_dev->evbit);

	/* 2.2 �ܲ��������¼������Щ�¼� */
	set_bit(BTN_TOUCH, s3c_ts_dev->keybit);

	input_set_abs_params(s3c_ts_dev, ABS_X, 0, 0x3FF, 0, 0);
	input_set_abs_params(s3c_ts_dev, ABS_Y, 0, 0x3FF, 0, 0);
	input_set_abs_params(s3c_ts_dev, ABS_PRESSURE, 0, 1, 0, 0);


	/* 3. ע�� */
	input_register_device(s3c_ts_dev);

	/* 4. Ӳ����صĲ��� */
	/* 4.1 ʹ��ʱ��(CLKCON[15]) */
	clk = clk_get(NULL, "adc");
	clk_enable(clk);
	
	/* 4.2 ����S3C2440��ADC/TS�Ĵ��� */
	s3c_ts_regs = ioremap(0x58000000, sizeof(struct s3c_ts_regs));

	/* bit[14]  : 1-A/D converter prescaler enable
	 * bit[13:6]: A/D converter prescaler value,
	 *            49, ADCCLK=PCLK/(49+1)=50MHz/(49+1)=1MHz
	 * bit[0]: A/D conversion starts by enable. ����Ϊ0
	 */
	s3c_ts_regs->adccon = (1<<14)|(49<<6);

	request_irq(IRQ_TC, pen_down_up_irq, IRQF_SAMPLE_RANDOM, "ts_pen", NULL);
	request_irq(IRQ_ADC, adc_irq, IRQF_SAMPLE_RANDOM, "adc", NULL);

	/* �Ż���ʩ1: 
	 * ����ADCDLYΪ���ֵ, ��ʹ�õ�ѹ�ȶ����ٷ���IRQ_TC�ж�
	 */
	s3c_ts_regs->adcdly = 0xffff;

	/* �Ż���ʩ5: ʹ�ö�ʱ����������,���������
	 * 
	 */
	init_timer(&ts_timer);
	ts_timer.function = s3c_ts_timer_function;
	add_timer(&ts_timer);

	enter_wait_pen_down_mode();

	major = register_chrdev(0, "input_replay", &replay_fops);

	cls = class_create(THIS_MODULE, "input_replay");
	device_create(cls, NULL, MKDEV(major, 0), "input_emu"); /* /dev/input_emu */

	init_timer(&replay_timer);
	replay_timer.function = input_replay_timer_func;
	//add_timer(&replay_timer);
	
	return 0;
}

static void s3c_ts_exit(void)
{
	//del_timer(&replay_timer);
	
	kfree(replay_buf);
	device_destroy(cls, MKDEV(major, 0));
	class_destroy(cls);
	unregister_chrdev(major, "input_replay");
	
	free_irq(IRQ_TC, NULL);
	free_irq(IRQ_ADC, NULL);
	iounmap(s3c_ts_regs);
	input_unregister_device(s3c_ts_dev);
	input_free_device(s3c_ts_dev);
	del_timer(&ts_timer);
}

module_init(s3c_ts_init);
module_exit(s3c_ts_exit);


MODULE_LICENSE("GPL");

