#include <linux/init.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/syscalls.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>


#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>

#define MY_FILE 		"/home/root/a"
#define LONG_TIME_FILE "/home/root/b"


//有时间的
/*
char buf[22];
struct file *file = NULL;
struct file * l_file = NULL;

void do_gettimeofday(struct timeval *tv);
void rtc_time_to_tm(unsigned long time, struct rtc_time *tm);
int para = 0;


int  errcode_repo(int ID,int code,int level)
{
	printk("-------drv_name repo errcode ----\n");
	struct timex  txc;
        struct rtc_time tm;
	mm_segment_t old_fs;
        if(file == NULL)
                file = filp_open(MY_FILE, O_RDWR | O_APPEND | O_CREAT, 0666);
        if (IS_ERR(file)) {
                printk("error occured while opening file %s, exiting...\n", MY_FILE);
                return -1;
        }
	//get_current_time
        do_gettimeofday(&(txc.time));

        rtc_time_to_tm(txc.time.tv_sec,&tm);

        sprintf(buf,"%02d%02d%d%02d%d%02d%02d%02d%02d%02d\n",ID,code,level,para,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec);

        old_fs = get_fs();
        set_fs(KERNEL_DS);
        file->f_op->write(file, (char *)buf, sizeof(buf), &file->f_pos);
        set_fs(old_fs);
        
	//create a big file to save info for a long time
	mm_segment_t long_time_fs;
	if(l_file == NULL)
                l_file = filp_open(LONG_TIME_FILE, O_RDWR | O_APPEND | O_CREAT, 0666);
        if (IS_ERR(l_file)) {
                printk("error occured while opening file %s, exiting...\n", LONG_TIME_FILE);
                return -1;
        }
	long_time_fs=get_fs();
	set_fs(KERNEL_DS);
	l_file->f_op->write(l_file, (char *)buf, sizeof(buf), &l_file->f_pos);
        set_fs(long_time_fs);
		
		
		
		
	filp_close(file, NULL);
    filp_close(l_file, NULL);
    l_file = NULL;
	file = NULL;
    return 0;
}
EXPORT_SYMBOL(errcode_repo);
*/


char buf[8];
struct file *file = NULL;
struct file * l_file = NULL;

int para = 0;

//没有时间的
int errcode_repo(int ID,int code,int level)
{
	printk("-------drv_name repo errcode ----\n");
	mm_segment_t old_fs;
    if(file == NULL)
    file = filp_open(MY_FILE, O_RDWR | O_APPEND | O_CREAT, 0666);
        if (IS_ERR(file)) {
                printk("error occured while opening file %s, exiting...\n", MY_FILE);
                return -1;
        }

    sprintf(buf,"%02d%02d%d%02d\n",ID,code,level,para);

    old_fs = get_fs();
    set_fs(KERNEL_DS);
    file->f_op->write(file, (char *)buf, sizeof(buf), &file->f_pos);
    set_fs(old_fs);
        
	//create a big file to save info for a long time
	mm_segment_t long_time_fs;
	if(l_file == NULL)
                l_file = filp_open(LONG_TIME_FILE, O_RDWR | O_APPEND | O_CREAT, 0666);
        if (IS_ERR(l_file)) {
                printk("error occured while opening file %s, exiting...\n", LONG_TIME_FILE);
                return -1;
        }
	long_time_fs=get_fs();
	set_fs(KERNEL_DS);
	l_file->f_op->write(l_file, (char *)buf, sizeof(buf), &l_file->f_pos);
    set_fs(long_time_fs);
		
	filp_close(file, NULL);
    filp_close(l_file, NULL);
    l_file = NULL;
	file = NULL;
    return 0;
}
EXPORT_SYMBOL(errcode_repo);
