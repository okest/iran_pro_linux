/*
 *  Copyright (C) 2017, tpeng <tpeng@foryouge.com.cn>
 *  RN4Y56 usb power mode switcher1 driver support
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
 
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/of_gpio.h>

#define USB_SWITCHER_MAJOR 231

dev_t devno1;

static int usb_switcher_major = USB_SWITCHER_MAJOR;

struct usb_switcher {
		struct cdev cdev;
		struct class *cls;
		struct device *dev;
		unsigned gpio[2];
};

struct usb_switcher *switcher1;


int usb_switcher_drv_open(struct inode *inode, struct file *filp)
{	
	filp->private_data = switcher1;
	return 0;

}
int usb_switcher_drv_close(struct inode *inode, struct file *filp)
{	
	return 0;
}

ssize_t  usb_switcher_drv_read(struct file *filp, char __user *buf, size_t count, loff_t *fpos)
{	
	struct usb_switcher *switcher1=filp->private_data;
	int value_stauts[1];
	
	value_stauts[0] = gpio_get_value(switcher1->gpio[0]) ;
			
	//printk(KERN_ALERT"[%s][%d] : value_stauts[0] = %d \n",__func__, __LINE__,value_stauts[0]); 

	if(copy_to_user(buf,value_stauts,count)){
		return -EFAULT;
	}
	
	return count;
}

static ssize_t usb_switcher_drv_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	char wbuf[1];
	int cnt;
	struct usb_switcher *switcher1=filp->private_data;
	int value_stauts,data0;

	memset(wbuf, 0, 1);

	if(count <= 0)
		return -1;
	   
	if(!copy_from_user((char *)wbuf, buf, cnt))
	{  
		value_stauts = gpio_get_value(switcher1->gpio[1]) ;
		
		if(buf[0]==0)
		{
			gpio_direction_output(switcher1->gpio[0],0);
			
			data0  = gpio_get_value(switcher1->gpio[0]);

			pr_info("[%s][%d] : buf[0]=%d  data0 = %d  value_stauts = %d \n",__func__, __LINE__,buf[0],data0,value_stauts); 

		}
		else
		{
			gpio_direction_output(switcher1->gpio[0],1);
			
			data0  = gpio_get_value(switcher1->gpio[0]);

			pr_info("[%s][%d] : buf[0]=%d  data0 = %d  value_stauts = %d \n",__func__, __LINE__,buf[0],data0,value_stauts); 
		}	
	}  
	else
	{
	   return -1;
	}
	return cnt;
}

long usb_switcher_drv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int a,b;
	struct usb_switcher *switcher1=filp->private_data;
	
	switch(cmd){
		default:
			return -EINVAL;
	}
	return 0;
}

const struct file_operations usb_switcher_fops = {
	.open    = usb_switcher_drv_open,
	.release = usb_switcher_drv_close,
	.read    = usb_switcher_drv_read,
	.write   = usb_switcher_drv_write,
	.unlocked_ioctl = usb_switcher_drv_ioctl,
};

int usb_switcher_probe(struct platform_device *pdev)
{	
	int ret,err;
	int data0,data1;

	devno1 = MKDEV(usb_switcher_major,0);
	
	if (usb_switcher_major)
	{	
		ret	= register_chrdev_region(devno1,1,"usbswitcher");			
		if (ret<0)
		{
			dev_err(&pdev->dev,"Error %d usbswitcher\n", err);
			return ret;
		}
	}
	else
	{
		alloc_chrdev_region(&devno1,0,1,"usbswitcher");
		usb_switcher_major = MAJOR(devno1);
	}

	switcher1 = kzalloc(sizeof(struct usb_switcher),GFP_KERNEL);
	if (!switcher1){
		dev_err(&pdev->dev,"kzalloc fail_malloc \n");
		ret =-ENOMEM;
		goto fail_malloc;
	}

	devno1 = MKDEV(usb_switcher_major,0);
	
	cdev_init(&switcher1->cdev,&usb_switcher_fops);
	
	switcher1->cdev.owner = THIS_MODULE;
	
	err = cdev_add(&switcher1->cdev,devno1,1);
	if (err)
		dev_err(&pdev->dev,"Error %d adding usb switcher1\n", err);
	
	switcher1->cls = class_create(THIS_MODULE, "usb_switcher_cls");
	if(IS_ERR(switcher1->cls))
	{
		dev_err(&pdev->dev,"class_create error\n");
		ret = PTR_ERR(switcher1->cls); 
		goto fail_malloc;
	}

	switcher1->dev = device_create(switcher1->cls, NULL,devno1, NULL, "tps254x");	
	
	if(IS_ERR(switcher1->dev))
	{
		dev_err(&pdev->dev,"device_create error \n");
		ret = PTR_ERR(switcher1->dev); 
		goto err_2;
	}

	//get gpio
	
	switcher1->gpio[0]=of_get_named_gpio(pdev->dev.of_node, "CTL", 0);
	switcher1->gpio[1]=of_get_named_gpio(pdev->dev.of_node, "STATUS", 0);
	
	pr_info("usb_switcher_probe : switcher1->gpio[0]=%d , switcher1->gpio[1]=%d  \n",switcher1->gpio[0],switcher1->gpio[1]);

	gpio_direction_output(switcher1->gpio[0],0);
	
	data0  = gpio_get_value(switcher1->gpio[0]);
	data1  = gpio_get_value(switcher1->gpio[1]);

	pr_info("usb_switcher_probe : data0 = %d  data1 = %d \n" ,data0,data1); 

	platform_set_drvdata(pdev, switcher1);
	
	return 0;
			
err_2:
		class_destroy(switcher1->cls);

fail_malloc:
		unregister_chrdev_region(devno1,1);
		kfree(switcher1);
		return ret;
}

int usb_switcher_remove(struct platform_device *pdev)
{	
	struct usb_switcher *switcher1 = platform_get_drvdata(pdev);
	
	device_destroy(switcher1->cls,devno1); 
	class_destroy(switcher1->cls);
	unregister_chrdev_region(devno1,1);
	
	kfree(switcher1);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int usb_switcher_suspend(struct device *dev)
{
	return 0;
}
static int usb_switcher_resume(struct device *dev)
{
	return 0;
}
static SIMPLE_DEV_PM_OPS(usb_switcher_pm_ops,usb_switcher_suspend, usb_switcher_resume);

#define USB_SWITCHER_PM_OPS (&usb_switcher_pm_ops)
#else
#define USB_SWITCHER_PM_OPS NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id usb_switcher_match[] = {
	{ .compatible = "gpio-TPS2549", },
	{ },
};
#endif

static struct platform_driver usb_switcher_driver = {
	.probe	= usb_switcher_probe,
	.remove = usb_switcher_remove,
	.driver = {
		.name	= "tps2596",
		.owner	= THIS_MODULE,
		.pm	= USB_SWITCHER_PM_OPS,
		.of_match_table = of_match_ptr(usb_switcher_match),
	},
};
module_platform_driver(usb_switcher_driver);

MODULE_AUTHOR("tpeng <tpeng@foryouge.com.cn>");
MODULE_DESCRIPTION("usb switcher1 driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:usb_switcher");



