#include "fms6502.h"
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>

#define FMS6502_BUS_NUM 0    		
#define FMS6502_SLAVE_ADDRESS 0x06	
#define FMS6502_NAME "fms6502"  	

#define DBG_ENABLE     0

#define FMS_DBG_ENABLE     0
#if FMS_DBG_ENABLE
#define FMS_DBG(fmt, args...)  printk(KERN_ALERT"\n[%s][%d] " fmt, __func__, __LINE__, ## args)
#else
#define FMS_DBG(fmt, args...)
#endif

#define FMS_LOCK      1
#if FMS_LOCK
spinlock_t fms6502_spinlock;
#define FMS_InitLock()      spin_lock_init(&fms6502_spinlock)
#define FMS_Lock()          spin_lock_irq(&fms6502_spinlock)
#define FMS_UnLock()        spin_unlock_irq(&fms6502_spinlock)
#endif

static struct i2c_adapter *gp_adapter_fms6502 = NULL;
struct i2c_client *fms6502_client = NULL;

static int                      fms6502_Open (struct inode *inode, struct file *filp);
static int                      fms6502_Release(struct inode *inode, struct file *filp);
static ssize_t                  fms6502_Read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t                  fms6502_Write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);

static int i2c_read_buff(struct i2c_client *client,u8 *buf,s32 len)
{
	int  ret = -1;
	int i =0;
    struct i2c_msg msgs[2];
	
	client->addr =0x03;
    msgs[0].flags = !I2C_M_RD;
    msgs[0].addr  = client->addr;
    msgs[0].len   = len;
    msgs[0].buf   = &buf[0]; 

    msgs[1].flags = I2C_M_RD;
    msgs[1].addr  = client->addr;
    msgs[1].len   = len;
    msgs[1].buf   = &buf[1];  

    ret = i2c_transfer(client->adapter, &msgs, 2);         
		
    return (ret == 1) ? len : ret; 
}

static int fms6502_Open (struct inode *inode, struct file *filp)
{
	//FMS_DBG("  \n");
    return 0;
}

static int fms6502_Release(struct inode *inode, struct file *filp)
{
    //FMS_DBG("  \n");
    return 0;
}

static ssize_t fms6502_Read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	int s32Ret = -1;
	int i=0;

	FMS6502_Param_t FMS6502_ReadParam;

	if (!copy_from_user(&FMS6502_ReadParam, (FMS6502_Param_t __user *)buf, sizeof(FMS6502_ReadParam)))
	{	
	
		u8 u32SlaveId 	= FMS6502_ReadParam.u32SlaveId;
		u8 u32Addr 		= FMS6502_ReadParam.u32Addr;
		u32 u32Len 		= FMS6502_ReadParam.u32Len+1;
		u8  u8Data[u32Len];

		u8Data[0] = u32Addr;
		
		//FMS_DBG(" u32SlaveId = 0x%x, u8Data[0] = 0x%x, u32Len = %d \n", u32SlaveId, u8Data[0], u32Len);

		s32Ret = i2c_read_buff(fms6502_client, u8Data, u32Len);
		
		for(i = 1; i < u32Len; i++)
		{
			FMS6502_ReadParam.u32Data[i-1] = u8Data[i];
		}

		if(copy_to_user(((FMS6502_Param_t __user *)buf), &FMS6502_ReadParam, sizeof(FMS6502_ReadParam)))
			s32Ret = -1;
		else 
			s32Ret |= 0;
	}
	return s32Ret;	
}

static int i2c_write_buff(struct i2c_client *client,u8 *buf,s32 len)
{
    int ret=-1; 
	struct i2c_msg msgs;
    s32 retries = 0;
	int i=0;

	client->addr =0x03;
	msgs.flags = !I2C_M_RD;
	msgs.addr  = client->addr;
	msgs.len   = len ;
	msgs.buf   = buf;

#if DBG_ENABLE
	printk(KERN_ALERT"[%s][%d]=======> addr=0x%x len=0x%x flags =0x%x :",__func__, __LINE__,msgs.addr,msgs.len,msgs.flags);
	for(i = 0; i < msgs.len; i++)
	{
		printk(KERN_ALERT"  0x%x   ", msgs.buf[i] );  
	}
	printk(KERN_ALERT"  \n ");  
	
#endif	

	ret = i2c_transfer(client->adapter, &msgs, 1);
	
	return  ( ret == 1)?len:ret;  
}

static ssize_t fms6502_Write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	s32 s32Ret = -1;
	int i = 0;

	//FMS_DBG("  \n");
	FMS6502_Param_t FMS6502_WriteParam;

	if (!copy_from_user(&FMS6502_WriteParam, (FMS6502_Param_t __user *)buf, sizeof(FMS6502_WriteParam)))
	{		
		u8 u32Id = FMS6502_WriteParam.u32SlaveId;
		u8 u32RegAddr = FMS6502_WriteParam.u32Addr;
		u32 u32Len = FMS6502_WriteParam.u32Len+1 ;
		u8	u8Data[u32Len];
		
		u8Data[0] = u32RegAddr;
		
		for(i = 1; i < u32Len; i++)
		{
			u8Data[1] = (u8)FMS6502_WriteParam.u32Data[0];	
			FMS_DBG("   u32Id = 0x%x, u32Data[0] = 0x%x, u8Data = 0x%x, u32Len = %d \n", u32Id, u8Data[0], u8Data[1], u32Len);
		}

		s32Ret =i2c_write_buff(fms6502_client, u8Data, u32Len);
		
		if(copy_to_user(((FMS6502_Param_t __user *)buf), &FMS6502_WriteParam, sizeof(FMS6502_WriteParam)))
			s32Ret = -1;
		else 
			s32Ret |= 0;
	}
	return s32Ret;
}

static int fms6502_sw0_init(void) 
{
	s32 s32Ret = -1;

	u8	u8Data[2]={0x00,0x00};
	u8Data[0] =0x00;
	u8Data[1] =0x00;
	s32Ret =i2c_write_buff(fms6502_client,u8Data, 2);
	if( s32Ret <0 )
	{
		printk(KERN_ALERT"i2c1_write_buff failed\n");
		return -1;
	}
	
	u8Data[0] =0x01;
	u8Data[1] =0x00;
	s32Ret =i2c_write_buff(fms6502_client, u8Data, 2);
	if( s32Ret <0 )
	{
		printk(KERN_ALERT"i2c1_write_buff failed\n");
		return -1;
	}
	
	u8Data[0] =0x02;
	u8Data[1] =0x00;
	s32Ret =i2c_write_buff(fms6502_client, u8Data, 2);
	if( s32Ret <0 )
	{
		printk(KERN_ALERT"i2c1_write_buff failed\n");
		return -1;
	}
	
	u8Data[0] =0x00;
	u8Data[1] =0x02;
	s32Ret =i2c_write_buff(fms6502_client, u8Data, 2);
	if( s32Ret <0 )
	{
		printk(KERN_ALERT"i2c1_write_buff failed\n");
		return -1;
	}
	
	u8Data[0] =0x03;
	u8Data[1] =0xff;
	s32Ret =i2c_write_buff(fms6502_client, u8Data, 2);
	if( s32Ret <0 )
	{
		printk(KERN_ALERT"i2c1_write_buff failed\n");
		return -1;
	}
	
	u8Data[0] =0x04;
	u8Data[1] =0x3f;
	s32Ret =i2c_write_buff(fms6502_client, u8Data, 2);
	if( s32Ret <0 )
	{
		printk(KERN_ALERT"i2c1_write_buff failed\n");
		return -1;
	}
	
	return 0;
}

static int fms6502_probe(struct i2c_client *client,const struct i2c_device_id *id)  
{
	s32 s32Ret = -1;
	
	fms6502_client = client;

	if (!i2c_check_functionality(fms6502_client->adapter, I2C_FUNC_I2C)) 
    {
		printk(KERN_ALERT"I2C check functionality failed\n");
        return -ENODEV;
    }

	//fms6502_sw0_init();
	
    return 0;
}

static int fms6502_remove(struct i2c_client *client)
{
	//FMS_DBG("  \n");
    return 0;
}

static struct file_operations fms6502_i2c_fops = {
	.owner    = THIS_MODULE,
	.open     = fms6502_Open,    
	.read 	  = fms6502_Read,
	.write 	  = fms6502_Write,   
	.release =  fms6502_Release,
};

static struct miscdevice fms6502_i2c_miscdev = {
	.minor   = MISC_DYNAMIC_MINOR, 
	.name    = FMS6502_NAME,       
	.fops    = &fms6502_i2c_fops,          
};

static const struct of_device_id fms6502_match_table[] = {
		{.compatible = "fms6502,fms_6502",},
		{ },
};

static const struct i2c_device_id fms6502_id[] = {
    { FMS6502_NAME, 0 },
    { }
};

static struct i2c_driver fms6502_driver = {
    .probe      = fms6502_probe,
    .remove     = fms6502_remove,
	.id_table   = fms6502_id,
    .driver = {
        .name     = FMS6502_NAME,
        .owner    = THIS_MODULE,
        .of_match_table = fms6502_match_table,
    },
};

static int __init fms6502_init(void)
{
	int ret =-1;
	//FMS_DBG("  \n");

    i2c_add_driver(&fms6502_driver);

	ret = misc_register(&fms6502_i2c_miscdev);
	if (ret < 0) 
		printk(KERN_ALERT" misc_register err\n");
    return 0; 
}

static void __exit fms6502_exit(void)
{
	//FMS_DBG("  \n");

    misc_deregister(&fms6502_i2c_miscdev);

  	i2c_del_driver(&fms6502_driver);

    if( fms6502_client != NULL )
    	i2c_unregister_device(fms6502_client);
}

module_init(fms6502_init);
module_exit(fms6502_exit);

MODULE_DESCRIPTION("fms6502 Driver");
MODULE_LICENSE("GPL");
