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
#include <asm/plat-s3c24xx/ts.h>
#include <asm/arch/regs-adc.h>
#include <asm/arch/regs-gpio.h>

static struct input_dev *s3c_ts_dev;
static int s3c_ts_init(void)
{
	/* 1. ����һ��input_dev�ṹ�� */
	s3c_ts_dev = input_allocate_device();

	/* 2. ���� */
	/* 2.1 �ܲ��������¼� */
	set_bit(EV_KEY, s3c_ts_dev->evbit);
	set_bit(EV_ABS, s3c_ts_dev->evbit);	//��������λ��, �������Ǿ���λ��

	/* 2.2 �ܲ��������¼������Щ�¼� */
	set_bit(BTN_TOUCH, s3c_ts_dev->keybit);	//�ο�S3c2410_ts.c; BTN_TOUCH�ǳ�������İ���(����������)

	//�ο�S3c2410_ts.c
	//static inline void input_set_abs_params(struct input_dev *dev, int axis, int min, int max, int fuzz, int flat)	//fuzz & flat�����Է���Ĳ���
	//0x3FF => ��s3c2440֧�ֵĴ��������� => Resolution: 10-bit, ���ֵ��3ff
	input_set_abs_params(s3c_ts_dev, ABS_X, 0, 0x3FF, 0, 0);	//x����
	input_set_abs_params(s3c_ts_dev, ABS_Y, 0, 0x3FF, 0, 0);	//y����
	input_set_abs_params(s3c_ts_dev, ABS_PRESSURE, 0, 1, 0, 0);	//ѹ������, Խ����Խ�� => ѹ���кܶ༶��,����ֻ�����ּ��� 0 & 1������ & �ɿ���


	/* 3. ע�� */
	input_register_device(s3c_ts_dev);

	/* 4. Ӳ����صĲ��� */
	
	return 0;
}

static void s3c_ts_exit(void)
{
}

module_init(s3c_ts_init);
module_exit(s3c_ts_exit);
MODULE_LICENSE("GPL");