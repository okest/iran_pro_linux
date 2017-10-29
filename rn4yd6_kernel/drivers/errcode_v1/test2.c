#include <linux/kernel.h>
#include <linux/module.h>
//#include <linux/slab.h>

extern int errcode_repo(int ID,int code,int level);

static int __init test2_init(void)
{
    printk("----------2  test_init --------------\n");

    errcode_repo(10,2,8);
    return 0;
}

static void __exit test2_exit(void)
{
    printk("----------2  test_exit --------------\n");
}

module_init(test2_init);
module_exit(test2_exit);



