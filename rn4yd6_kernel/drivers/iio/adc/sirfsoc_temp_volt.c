/*
 * sirfsoc temperature sensor driver
 *
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/iio/consumer.h>
#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/uaccess.h>

struct sirfsoc_sensor {
	struct iio_channel	*chan;
	int a;
};


static struct sirfsoc_sensor *my_sensor = NULL; 
static struct proc_dir_entry *adc_dentry = NULL;

static ssize_t adc_read_zjc(struct file *, char __user *, size_t, loff_t *);
static ssize_t adc_wirte_zjc(struct file *, const char __user *, size_t, loff_t *);

static const struct file_operations adc_ops_zjc = {
    .owner = THIS_MODULE,
    .read = adc_read_zjc,
    .write = adc_wirte_zjc,
};
static ssize_t adc_read_zjc(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	printk(KERN_EMERG " my_sensor = %p\n",my_sensor);
	
	int ret, val;
	char * buf;
	ret = iio_read_channel_processed(my_sensor->chan, &val);
	if (ret < 0)
		printk(KERN_EMERG "read channel error\n");
	printk(KERN_EMERG "vlo is %d\n",val);

	return 1;
}

static ssize_t adc_wirte_zjc(struct file *filp, const char __user *buffer, size_t count, loff_t *off)
{
	
	return count;
}


static int sirfsoc_sensor_probe(struct platform_device *pdev)
{
	printk(KERN_EMERG "zjc adc [tmp] probe is begin\n");
	struct device *dev = &pdev->dev;
	
	struct sirfsoc_sensor *sirfsoc_sensor;
	
	int status;
	int ret, val;

	sirfsoc_sensor = devm_kzalloc(dev, sizeof(*sirfsoc_sensor), GFP_KERNEL);
	if (!sirfsoc_sensor)
		return -ENOMEM;
	

	sirfsoc_sensor->chan = iio_channel_get(&pdev->dev, "adc6");
	if (IS_ERR(sirfsoc_sensor->chan)) {
		dev_err(dev, "atlas7 sensor: Unable to get the adc channel\n");
		status = PTR_ERR(sirfsoc_sensor->chan);
		goto err;
	}

	my_sensor = sirfsoc_sensor;
	//printk(KERN_EMERG "my_sensor %p sirfsoc_sensor %p\n",my_sensor,sirfsoc_sensor);
	adc_dentry = proc_create("adc_zjc", 0666, NULL, &adc_ops_zjc);

	ret = iio_read_channel_processed(sirfsoc_sensor->chan, &val);
	if (ret < 0)
		printk(KERN_EMERG "read channel error\n");
	
	printk(KERN_EMERG "first read  adc6 value is %d\n",val);

	platform_set_drvdata(pdev, sirfsoc_sensor);

err:
	return 0;
}

static int sirfsoc_sensor_remove(struct platform_device *pdev)
{
	struct sirfsoc_sensor *sirfsoc_sensor = platform_get_drvdata(pdev);

	//thermal_zone_of_sensor_unregister(sirfsoc_sensor->hwmon_dev,
	//		sirfsoc_sensor->tz);
	//hwmon_device_unregister(sirfsoc_sensor->hwmon_dev);

	return 0;
}

static const struct of_device_id sirfsoc_sensor_ids[] = {
	{ .compatible = "sirf,atlas7-sensor"},
	{}
};

static struct platform_driver sirfsoc_sensor_driver = {
	.driver = {
		   .name = "sirfsoc_sensor",
		   .of_match_table = sirfsoc_sensor_ids,
		   },
	.probe = sirfsoc_sensor_probe,
	.remove = sirfsoc_sensor_remove,
};
//module_platform_driver(sirfsoc_sensor_driver);

static __init int sirfsoc_sensor_driver_init(void)
{
	platform_driver_register(&sirfsoc_sensor_driver);
	return 0;
}
late_initcall(sirfsoc_sensor_driver_init);



