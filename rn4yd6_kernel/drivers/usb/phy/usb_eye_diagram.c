#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#define USB0_CONF_PHY 0x17060200
#define USB0_SIZE 0x4

#define USB1_CONF_PHY 0x17070200
#define USB1_SIZE 0x4

void * reg_base0;
void * reg_base1;

#define REG_VEL  0x917fcfce 
static int __init write_usb_drv_init(void)
{

	// 1,硬件的初始化---地址映射
	reg_base0= ioremap(USB0_CONF_PHY, USB0_SIZE);
	reg_base1= ioremap(USB1_CONF_PHY, USB1_SIZE);
	
	//printk(KERN_EMERG "----------read usb0 register [%x]---\n",readl(reg_base0));
	//printk(KERN_EMERG "----------read usb1 register [%x]---\n",readl(reg_base1));
	writel(REG_VEL,reg_base0);
	writel(REG_VEL,reg_base1);
	
	//printk(KERN_EMERG "----------read usb0 register [%x]---\n",readl(reg_base0));
	//printk(KERN_EMERG "----------read usb1 register [%x]---\n",readl(reg_base1));
	
	return 0;

}


static void __exit write_usb_drv_exit(void)
{
	//模块卸载函数一般来说都是完成资源的释放--都是void
	iounmap(reg_base0);
	iounmap(reg_base1);
}


module_init(write_usb_drv_init);
module_exit(write_usb_drv_exit);
MODULE_LICENSE("GPL");



