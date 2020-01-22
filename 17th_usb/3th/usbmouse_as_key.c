/*
 * drivers\hid\usbhid\usbmouse.c
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>

static struct input_dev *uk_dev;
static char *usb_buf;
static dma_addr_t usb_buf_phys;// pa
static int len;
static struct urb *uk_urb;

static struct usb_device_id usbmouse_as_key_id_table [] = {
	{ USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT,
		USB_INTERFACE_PROTOCOL_MOUSE) },
	//{USB_DEVICE(0x1234,0x5678)},
	{ }	/* Terminating entry */
};

//完成函数：鼠标是中断传输，没能力主动打断主机，通过主机控制器不断查询来实现中断传输的实时
//当usb主机控制器得到数据后，缓存到buffer并产生中断，导致usb总线驱动调用 complete_fn
static void usbmouse_as_key_irq(struct urb *urb)
{
	int i;
	static int cnt = 0;
	printk("data cnt %d: ", ++cnt);
	for (i = 0; i < len; i++)
	{
		//看看鼠标里数据含义
		printk("%02x ", usb_buf[i]);	//不同鼠标usb_buf[]大小可能不同
	}
	printk("\n");

	/* 重新提交urb */
	usb_submit_urb(uk_urb, GFP_KERNEL);
}

static int usbmouse_as_key_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	struct usb_host_interface *interface;
	struct usb_endpoint_descriptor *endpoint;
	int pipe;
	
	//这里默认认为是鼠标, 不同Usbmouse.c里去做各种判断
	interface = intf->cur_altsetting;
	endpoint = &interface->endpoint[0].desc;

	/* a. 分配一个input_dev */
	uk_dev = input_allocate_device();
	
	/* b. 设置 */
	/* b.1 能产生哪类事件 */
	set_bit(EV_KEY, uk_dev->evbit);
	set_bit(EV_REP, uk_dev->evbit);
	
	/* b.2 能产生哪些事件 */
	set_bit(KEY_L, uk_dev->keybit);
	set_bit(KEY_S, uk_dev->keybit);
	set_bit(KEY_ENTER, uk_dev->keybit);
	
	/* c. 注册 */
	input_register_device(uk_dev);
	
	/* d. 硬件相关操作 *///usb特有，不同于buttons & lcd & ts
	/* 数据传输3要素: 源,目的,长度 */
	/* 源: USB设备的某个端点 */
	//pipe：含有端点类型，端点方向，端点地址编号，设备地址编号(每个usb设备都有地址编号)
	pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);

	/* 长度: */
	len = endpoint->wMaxPacketSize;//endpoint[0]端点描述符的最大包大小

	/* 目的: */	//缓冲区不能用kmalloc()
	usb_buf = usb_buffer_alloc(dev, len, GFP_ATOMIC, &usb_buf_phys);	//usb_buf : va & usb_buf_phys : pa

	/* 使用"3要素" */
	/* 分配 usb request block */
	uk_urb = usb_alloc_urb(0, GFP_KERNEL);
	/* 使用"3要素"设置urb（填充中断类型的urb） */
	usb_fill_int_urb(uk_urb, dev, pipe, usb_buf, len, usbmouse_as_key_irq, NULL, endpoint->bInterval);//NULL 给 usbmouse_as_key_irq 用
	
	//usb主机控制器得到数据后，需缓存到内存，但usb主机控制器不智能，需要pa
	uk_urb->transfer_dma = usb_buf_phys;
	uk_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	/* 使用URB */
	usb_submit_urb(uk_urb, GFP_KERNEL);//usb总线驱动提供
	
	return 0;
}

static void usbmouse_as_key_disconnect(struct usb_interface *intf)
{
	struct usb_device *dev = interface_to_usbdev(intf);

	//printk("disconnect usbmouse!\n");
	usb_kill_urb(uk_urb);
	usb_free_urb(uk_urb);

	usb_buffer_free(dev, len, usb_buf, usb_buf_phys);
	input_unregister_device(uk_dev);
	input_free_device(uk_dev);
}

/* 1. 分配/设置usb_driver */
static struct usb_driver usbmouse_as_key_driver = {
	.name		= "usbmouse_as_key_",
	.probe		= usbmouse_as_key_probe,
	.disconnect	= usbmouse_as_key_disconnect,
	.id_table	= usbmouse_as_key_id_table,
};


static int usbmouse_as_key_init(void)
{
	/* 2. 注册 */
	usb_register(&usbmouse_as_key_driver);
	return 0;
}

static void usbmouse_as_key_exit(void)
{
	usb_deregister(&usbmouse_as_key_driver);	
}

module_init(usbmouse_as_key_init);
module_exit(usbmouse_as_key_exit);

MODULE_LICENSE("GPL");

