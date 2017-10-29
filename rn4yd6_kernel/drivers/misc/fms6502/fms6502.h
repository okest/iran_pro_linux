#ifndef _fms6502_H_
#define _fms6502_H_

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/sysfs.h>
#include <linux/ctype.h>
#include <linux/err.h>


#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>


#define _INPUT                  1
#define _OUTPUT                 0
#define _HIGH                   1
#define _LOW                    0
#define SWIIC_READ              0
#define SWIIC_WRITE             1

#define FMS6502_ID 0x06


struct FMS6502_Param
{
	u8 bFlag;			// I2C_CHANNEL/GPIO_CHANNEL 
	u8 bDirect;		// Reserved		
	u8  u32SlaveId;	// Device slave ID: 0xXX    
	u8  u32GpioId;		// GPIO ID: 0 - 178
	u8  u32Addr;		// i2c reg addr: 0xXX
	u8  *u32Data;		// gpio or i2c data: 0xXX	
	u32  u32Len;
};

typedef struct FMS6502_Param FMS6502_Param_t;


#endif /* _fms6502_H_ */

