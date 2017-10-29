
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
#include <linux/timer.h>
#include <linux/workqueue.h>

#include <linux/mm.h>
#include <linux/io.h>
#include <linux/sched.h> 
#include <linux/err.h> 
#include <linux/delay.h>
#include <asm/uaccess.h>


#define DEBUG

#ifdef DEBUG 
#define LOGD(fmt...) printk(KERN_ERR fmt)
#else 
#define LOGD(fmt...) 
#endif

struct work_struct time_work;
struct workqueue_struct  *wq_beeper;

#define DEVICE_NAME "atlas7_beeper"

struct beeper_param{
	unsigned int delay_ms;
	unsigned int pwm_value;
};

typedef struct beeper_param beeper_param_t;

struct pwm_beeper {
	struct pwm_device *pwm;
	unsigned long period;
};

static struct pwm_beeper *beeper;
	
#define HZ_TO_NANOSECONDS(x) (1000000000UL/(x))

static unsigned int value;
static unsigned int ms;

static int beeper_Open (struct inode *inode, struct file *filp)
{
    return 0;
}
//ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
static ssize_t beeper_Read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	beeper_param_t beeper_param;
	beeper_param.pwm_value = value;
	beeper_param.delay_ms = ms;
	if (copy_to_user((beeper_param_t __user *)buf,&beeper_param,sizeof(beeper_param)))
	{
		LOGD("copy_to_user the failed !!!\n");
		return -1;
	}
	return sizeof(beeper_param);
}

static int beeper_Release(struct inode *inode, struct file *filp)
{
    return 0;
}
/*
static void beeper_delay(unsigned int msecs)
{
	unsigned long timeout = msecs_to_jiffies(msecs) + 1;

	while (timeout)
		timeout = schedule_timeout_uninterruptible(timeout);
}
*/
//zjc

void time_work_funtion(unsigned long data)
{
	pwm_enable(beeper->pwm);
	unsigned long timeout = msecs_to_jiffies(ms) + 1;
	while (timeout)
		timeout = schedule_timeout_uninterruptible(timeout);
	
	pwm_disable(beeper->pwm);
	//printk(KERN_EMERG "---value = %d ms = %d \n",value,ms);
}

static ssize_t beeper_Write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	beeper_param_t beeper_param;
	unsigned long period;
	int ret;
	
	if (!copy_from_user(&beeper_param, (beeper_param_t __user *)buf, sizeof(beeper_param)))
	{
		value = beeper_param.pwm_value;
		ms = beeper_param.delay_ms;
		if (value == 0) {
			pwm_config(beeper->pwm, 0, 0);
			pwm_disable(beeper->pwm);
		} else {
			period = HZ_TO_NANOSECONDS(value);
			ret = pwm_config(beeper->pwm, period / 2, period);
			if (ret)
				return ret;
			//zjc
			//schedule_work(&time_work);
			queue_work(wq_beeper, &time_work);
			//zjc
		}
	}
	//LOGD("read the the setting from user\n");
	return sizeof(beeper_param);
}


static struct file_operations beeper_fops = {
	.owner    = THIS_MODULE,
	.open     = beeper_Open,    
	.read 	  = beeper_Read,
	.write 	  = beeper_Write,  
	.release =  beeper_Release,
};

static struct miscdevice beeper_miscdev = {
	.minor   = MISC_DYNAMIC_MINOR, 
	.name    = DEVICE_NAME,       
	.fops    = &beeper_fops,          
};

static int pwm_beeper_probe(struct platform_device *pdev)
{
	unsigned long pwm_id = (unsigned long)dev_get_platdata(&pdev->dev);

	int error;
	u32 val;
	int ret;
	

	beeper = kzalloc(sizeof(*beeper), GFP_KERNEL);// alloc the beeper struct mem space
	if (!beeper)
		return -ENOMEM;

	beeper->pwm = pwm_get(&pdev->dev, NULL);//get the pwm device
	if (IS_ERR(beeper->pwm)) {
		dev_dbg(&pdev->dev, "unable to request PWM, trying legacy API\n");
		beeper->pwm = pwm_request(pwm_id, "pwm beeper");
	}

	if (IS_ERR(beeper->pwm)) {
		error = PTR_ERR(beeper->pwm);
		dev_err(&pdev->dev, "Failed to request pwm device: %d\n", error);
		goto err_free;
	}

	ret = misc_register(&beeper_miscdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to misc_register\n");
		goto misc_exit;
	}

	platform_set_drvdata(pdev, beeper);//set the platform data
	val = readl(ioremap(0x10e400e8, 0x4));
	val |= 0x30;
	writel(val,ioremap(0x10e400e8, 0x4));
	
	//zjc test 
	wq_beeper = create_singlethread_workqueue("beeper_wq");
	INIT_WORK(&time_work,(void(*)(void*))time_work_funtion);
	// 
    

	return 0;


misc_exit:
	misc_deregister(&beeper_miscdev);

err_free:
	kfree(beeper);

	return error;
}

static int pwm_beeper_remove(struct platform_device *pdev)
{	
	struct pwm_beeper *beeper = platform_get_drvdata(pdev);	
	misc_deregister(&beeper_miscdev);
	pwm_disable(beeper->pwm);	
	pwm_free(beeper->pwm);	
	kfree(beeper);	
	return 0;
}


#ifdef CONFIG_PM_SLEEP
static int pwm_beeper_suspend(struct device *dev)
{
	struct pwm_beeper *beeper = dev_get_drvdata(dev);

	if (beeper->period)
		pwm_disable(beeper->pwm);

	return 0;
}

static int pwm_beeper_resume(struct device *dev)
{
	struct pwm_beeper *beeper = dev_get_drvdata(dev);

	if (beeper->period) {
		pwm_config(beeper->pwm, beeper->period / 2, beeper->period);
		pwm_enable(beeper->pwm);
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(pwm_beeper_pm_ops,
			 pwm_beeper_suspend, pwm_beeper_resume);

#define PWM_BEEPER_PM_OPS (&pwm_beeper_pm_ops)
#else
#define PWM_BEEPER_PM_OPS NULL
#endif



#ifdef CONFIG_OF
static const struct of_device_id pwm_beeper_match[] = {
	{ .compatible = "pwm-beeper", },
	{ },
};
#endif

static struct platform_driver pwm_beeper_driver = {
	.probe	= pwm_beeper_probe,
	.remove = pwm_beeper_remove,
	.driver = {
		.name	= "pwm-beeper",
		.owner	= THIS_MODULE,
		.pm	= PWM_BEEPER_PM_OPS,
		.of_match_table = of_match_ptr(pwm_beeper_match),
	},
};

module_platform_driver(pwm_beeper_driver);

MODULE_AUTHOR("Lars-Peter Clausen <lars@metafoo.de>");
MODULE_DESCRIPTION("PWM beeper driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pwm-beeper");
