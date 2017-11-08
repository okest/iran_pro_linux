#ifndef _DEBUG_UART_H_
#define _DEBUG_UART_H_

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
#include <linux/wait.h>  
#include <linux/skbuff.h>  
#include <linux/sched.h> 
#include <linux/jiffies.h>	

#include <linux/serial_core.h>

#define LOGD(format,...)    printk(KERN_INFO "rxhu:[func:%s][Line:%d] "format"\n",__FUNCTION__, __LINE__,##__VA_ARGS__)
#define LOG_ERR(format,...)    printk(KERN_ERR "rxhu:[func:%s][Line:%d] "format"\n",__FUNCTION__, __LINE__,##__VA_ARGS__)

#define DEVICE_NAME "atlas7_uart_debug"

#define UART_RX_BUF_SIZE 128

extern unsigned char uart_recv_buf[UART_RX_BUF_SIZE];

extern struct sk_buff_head uart_recv_queue;

extern wait_queue_head_t read_wait_queue;

extern void itfc_startup_uart(void);

extern int debug_sirfsoc_uart_startup(void);

extern int debug_uart_port_startup(void);

extern void debug_uart_serial_tx(unsigned char *buf,int count);

extern void debug_uart_switch_to_pio(void);

extern void debug_uart_switch_recovery(void);

extern void debug_uart_recv_work_init(void);

extern int debug_uart_port_close(void);

extern void debug_uart_recv_work_deinit(void);
#endif