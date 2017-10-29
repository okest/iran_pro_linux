
#include <linux/input.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <linux/init.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <linux/dma-mapping.h>
#include <linux/of_platform.h>
#include <linux/videodev2.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>	

#include <linux/mm.h>
#include <linux/io.h>
#include <linux/sched.h> 
#include <linux/err.h> 
#include <linux/delay.h>
#include <asm/uaccess.h>

//#define DEBUG
#ifdef DEBUG 
#define LOGD(format,...) printk(KERN_ERR "[File:%s][Line:%d] "format"\n",__FILE__, __LINE__,##__VA_ARGS__)
#else 
#define LOGD(format,...) 
#endif

#define DEVICE_NAME "atlas7_usb_power"


int usb_power_gpio;

int usb_power_drv_open(struct inode *inode, struct file *filp)
{	//dev_err(NULL,"---------*_*- %s----------\n", __FUNCTION__);
	filp->private_data = &usb_power_gpio;
	return 0;
}

int usb_power_drv_close(struct inode *inode, struct file *filp)
{	//dev_err(NULL,"---------*_*- %s----------\n", __FUNCTION__);
	return 0;

}

ssize_t usb_power_drv_write(struct file *filp, const char __user *buf, size_t count, loff_t *fpos)
{
	
	int setdata;
	LOGD("-------------usb_power_drv_write-----------------------------");
	if (!copy_from_user(&setdata, (int *)buf, sizeof(int)))
	{
		LOGD("set the gpio value! = %d",setdata);
		gpio_set_value(usb_power_gpio,setdata);
		return 0;
	}
	return -1;
}

ssize_t  usb_power_drv_read(struct file *filp, char __user *buf, size_t count, loff_t *fpos)
{
	
	int state;
	LOGD("-------------usb_power_drv_read-----------------------------");
	state = gpio_get_value(usb_power_gpio);
	if(copy_to_user((int *)buf,&state,sizeof(int))){
		LOGD("get the value successfull!");
		return -1;
	}
	return 0;
}


const struct file_operations usb_power_fops = {
	.open = usb_power_drv_open,
	.release = usb_power_drv_close,
	.read = usb_power_drv_read,
	.write =usb_power_drv_write,
};

static struct miscdevice usb_power_miscdev = {
	.minor   = MISC_DYNAMIC_MINOR, 
	.name    = DEVICE_NAME,       
	.fops    = &usb_power_fops,          
};



static int usb_power_probe(struct platform_device *pdev)
{
	int ret;
	
	LOGD("-------------usb_power_probe-----------------------------");
	
	usb_power_gpio = of_get_named_gpio(pdev->dev.of_node, "PWR_EN", 0);
#if 0
	ret = gpio_request(usb_power_gpio, "PWR_EN");
    if (ret < 0) 
    {
        dev_err(&pdev->dev,"Failed to request GPIO:%d, ERRNO:%d", usb_power_gpio, ret);
        return -1;
    }
#endif
	ret = misc_register(&usb_power_miscdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to misc_register\n");
		goto misc_exit;
	}
	
	LOGD("------------usb_power_gpio = %d-------------",usb_power_gpio);
	gpio_direction_output(usb_power_gpio,0);


	

	return 0;

misc_exit:
	misc_deregister(&usb_power_miscdev);
	return -1;
}

int usb_power_remove(struct platform_device *pdev)
{	
	LOGD("-------------usb_power_remove-----------------------------");
	misc_deregister(&usb_power_miscdev);
	return 0;
}


static const struct of_device_id usb_power_match[] = {
	{ .compatible = "usb-set-power", },
	{ },
};

static struct platform_driver usb_power_driver = {
	.probe	= usb_power_probe,
	.remove = usb_power_remove,
	.driver = {
		.name	= "usb-power",
		.owner	= THIS_MODULE,
		.pm	= NULL,
		.of_match_table = of_match_ptr(usb_power_match),
	},
};

module_platform_driver(usb_power_driver);

MODULE_DESCRIPTION("USB power driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:USB-power");
