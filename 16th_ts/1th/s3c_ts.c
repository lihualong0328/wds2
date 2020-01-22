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
	/* 1. 分配一个input_dev结构体 */
	s3c_ts_dev = input_allocate_device();

	/* 2. 设置 */
	/* 2.1 能产生哪类事件 */
	set_bit(EV_KEY, s3c_ts_dev->evbit);
	set_bit(EV_ABS, s3c_ts_dev->evbit);	//鼠标是相对位移, 触摸屏是绝对位移

	/* 2.2 能产生这类事件里的哪些事件 */
	set_bit(BTN_TOUCH, s3c_ts_dev->keybit);	//参考S3c2410_ts.c; BTN_TOUCH是抽象出来的按键(触摸屏按键)

	//参考S3c2410_ts.c
	//static inline void input_set_abs_params(struct input_dev *dev, int axis, int min, int max, int fuzz, int flat)	//fuzz & flat是线性方面的参数
	//0x3FF => 见s3c2440支持的触摸屏精度 => Resolution: 10-bit, 最大值是3ff
	input_set_abs_params(s3c_ts_dev, ABS_X, 0, 0x3FF, 0, 0);	//x方向
	input_set_abs_params(s3c_ts_dev, ABS_Y, 0, 0x3FF, 0, 0);	//y方向
	input_set_abs_params(s3c_ts_dev, ABS_PRESSURE, 0, 1, 0, 0);	//压力方向, 越用力越粗 => 压力有很多级别,这里只有两种级别 0 & 1（按下 & 松开）


	/* 3. 注册 */
	input_register_device(s3c_ts_dev);

	/* 4. 硬件相关的操作 */
	
	return 0;
}

static void s3c_ts_exit(void)
{
}

module_init(s3c_ts_init);
module_exit(s3c_ts_exit);
MODULE_LICENSE("GPL");
