#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>


extern int errcode_repo(int ID,int code,int level);
static int __init test3_init_(void)
{
    printk("---------3 test_init --------------\n");

    errcode_repo(1,2,8);
	
    return 0;
}

static void __exit test3_exit_(void)
{
    printk("----------3  test_exit --------------\n");
}

subsys_initcall(test3_init_);
module_exit(test3_exit_);



