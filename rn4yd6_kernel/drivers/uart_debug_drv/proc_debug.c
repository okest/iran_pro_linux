#include <linux/kernel.h>  
#include <linux/module.h> 
#include <linux/types.h>
#include <linux/kern_levels.h>
#include "proc_debug.h"

typedef struct
{
    unsigned char *puc_proc_path;
    unsigned int ui_cmdnum;
    PROC_DEBUG_SUB_S st_sub[PROC_DEBUG_NUM_MAX];
}PROC_DEBUG_INFO_S;

static PROC_DEBUG_INFO_S st_proc_dbg_info=
{
    .puc_proc_path = NULL,
    .ui_cmdnum = 0,
};

#define ADAYO_DEBUG_SUB(_idx, _st_dbgsub) do{\
    printk(KERN_EMERG"   %d. echo -n \"%s %s\" > %s ; ex: echo -n \"%s %s\" > %s  \n", _idx,(_st_dbgsub).puc_cmd,\
        (((_st_dbgsub).puc_instruction != ((unsigned char *)NULL)) ? (_st_dbgsub).puc_instruction : ((unsigned char *)POOC_DEBUG_NULL_STR)),\
        st_proc_dbg_info.puc_proc_path,\
        (_st_dbgsub).puc_cmd, (((_st_dbgsub).puc_ex != ((unsigned char *)NULL)) ? (_st_dbgsub).puc_ex : ((unsigned char *)POOC_DEBUG_NULL_STR)), \
        st_proc_dbg_info.puc_proc_path);\
}while(0)



#define STR_TO_INT(_str, _int) do{\
    int _i_mm;\
    (_int) = 0;\
    for (_i_mm = 0; _i_mm < strlen(_str); _i_mm++)\
    {\
        (_int) = ((_int) * 10) + (_str[_i_mm] - '0');\
    }\
}while(0)

static int __proc_cmd_getidx(PROC_DEBUG_SUB_S *pst_info)
{
    int i_mm;
    for (i_mm = 0; i_mm < st_proc_dbg_info.ui_cmdnum; i_mm++)
    {
    }
    
    return st_proc_dbg_info.ui_cmdnum;
}

static void __proc_cmd_help(void)
{
    int i_mm;

    printk(KERN_EMERG"============================ porc[0x%x] cmd help in ============================= \n", (unsigned int)&st_proc_dbg_info);
    
    for (i_mm = 0; i_mm < PROC_DEBUG_NUM_MAX; i_mm++)
    {   
        if (st_proc_dbg_info.st_sub[i_mm].puc_cmd != NULL)
        {
            ADAYO_DEBUG_SUB(i_mm, st_proc_dbg_info.st_sub[i_mm]);
        }
    }
    printk(KERN_EMERG"============================ porc[0x%x] cmd help out ============================ \n", (unsigned int)&st_proc_dbg_info);

    return;
}

int adayo_proc_cmd_init(unsigned char *puc_proc_path)
{
    if (st_proc_dbg_info.puc_proc_path == NULL)
    {
        memset(&st_proc_dbg_info, 0, sizeof(st_proc_dbg_info));
        st_proc_dbg_info.puc_proc_path = puc_proc_path;
        printk("%s.%d init cmd proc_path=%s \n", __FUNCTION__, __LINE__, puc_proc_path);
    }
    
    return 0;
}

int adayo_proc_cmd_add(PROC_DEBUG_SUB_S *pst_info)
{
    int i_idx;

    if ((pst_info == NULL) || (pst_info->puc_cmd == NULL) ||\
        (pst_info->pf_cbl == NULL) || (pst_info->ui_paramnum >= PROC_DEBUG_PARAMLIST_MAX))
    {
        return -1;
    }

    i_idx = __proc_cmd_getidx(pst_info);
    if (i_idx != PROC_DEBUG_NUM_MAX)
    {
        memcpy(&(st_proc_dbg_info.st_sub[i_idx]), pst_info, sizeof(PROC_DEBUG_SUB_S));
        st_proc_dbg_info.ui_cmdnum++;
    }
        
    return 0;
}

int adayo_proc_cmd_del(PROC_DEBUG_SUB_S *pst_info)
{
    int i_idx;

    i_idx = __proc_cmd_getidx(pst_info);
    if (i_idx != PROC_DEBUG_NUM_MAX)
    {
        memset(&(st_proc_dbg_info.st_sub[i_idx]), 0, sizeof(PROC_DEBUG_SUB_S));
        st_proc_dbg_info.ui_cmdnum--;
    }
        
    return 0;
}

int adayo_proc_cmd_do(unsigned char *puc_buff, unsigned int ui_bufflen)
{
    unsigned int i_mm, i_jj, i_idx, i_num, i_last_idx;
    unsigned char *puc_tmpstr;
    unsigned int aui_paramlist[PROC_DEBUG_PARAMLIST_MAX];
    int i_ret = -1;

    
    printk("%s.%d puc_buff=[%s] [len=%d]\n", __FUNCTION__, __LINE__, puc_buff, ui_bufflen);
    
    if (st_proc_dbg_info.puc_proc_path == NULL)
    {
        return -1;
    }
    
    for (i_mm = 0; i_mm < PROC_DEBUG_NUM_MAX; i_mm++)
    {
        if (st_proc_dbg_info.st_sub[i_mm].puc_cmd == NULL)
        {
            continue;
        }
        
        if ((puc_tmpstr = strstr(puc_buff, st_proc_dbg_info.st_sub[i_mm].puc_cmd)) == NULL)
        {
            continue;
        }
        if ((puc_tmpstr[strlen(st_proc_dbg_info.st_sub[i_mm].puc_cmd)] != ' ') && \
            (puc_tmpstr[strlen(st_proc_dbg_info.st_sub[i_mm].puc_cmd)] != '\n') && \
            (puc_tmpstr[strlen(st_proc_dbg_info.st_sub[i_mm].puc_cmd)] != '\0'))
        {
            //printk("%s.%d puc_tmpstr=[%c] [len=%d]\n", __FUNCTION__, __LINE__, puc_tmpstr[strlen(st_proc_dbg_info.st_sub[i_mm].puc_cmd)], strlen(st_proc_dbg_info.st_sub[i_mm].puc_cmd));
            continue;
        }

        /* 去除多余空格 */
        /* 计算首字符空格数 */
        for (i_jj = 0; i_jj < ui_bufflen; i_jj++)
        {
            if (puc_buff[i_jj] != ' ')
            {
                break;
            }
        }
        
        i_idx = 0;
        i_num = 0;
        i_last_idx = 0;
        for (; i_jj < ui_bufflen; i_jj++)
        {
            if (puc_buff[i_jj] != ' ')
            {
                puc_buff[i_idx] = puc_buff[i_jj];
                i_idx++;
            }
            else
            {
                if (puc_buff[i_idx - 1] != '\0')
                {
                    puc_buff[i_idx] = '\0';
                    i_last_idx = i_idx;
                    i_idx++;
                    /* 间隔字符串个数 */
                    i_num++;
                }
            }

        }
        
        /* 解决一空格结尾的字符换，多计算一个参数个数的问题 */
        if (i_last_idx == (i_idx - 1))
        {
            i_num = i_num - 1;
        }
        memset(&(puc_buff[i_idx]), 0, (ui_bufflen - (i_idx + 1)));
        
        if (strlen(st_proc_dbg_info.st_sub[i_mm].puc_cmd) != strlen(puc_buff))
        {
            continue;
        }

        if (i_num < st_proc_dbg_info.st_sub[i_mm].ui_paramnum)
        {
            printk("%s.%d init cmd[%s], param is error [%d.%d] \n", \
                __FUNCTION__, __LINE__, st_proc_dbg_info.st_sub[i_mm].puc_cmd, i_num, st_proc_dbg_info.st_sub[i_mm].ui_paramnum);
            ADAYO_DEBUG_SUB(i_mm, st_proc_dbg_info.st_sub[i_mm]);
            continue;
        }

        puc_tmpstr += strlen(st_proc_dbg_info.st_sub[i_mm].puc_cmd) + 1;
            
        for (i_jj = 0; i_jj < i_num; i_jj++)
        {
            STR_TO_INT(puc_tmpstr, aui_paramlist[i_jj]);
            puc_tmpstr += strlen(puc_tmpstr) + 1;
            
            printk(" paramlist: [%d.%d] \n", i_jj, aui_paramlist[i_jj]);
        }

        i_ret = st_proc_dbg_info.st_sub[i_mm].pf_cbl(aui_paramlist, i_num);
        
        break;
    }

    if (i_mm == PROC_DEBUG_NUM_MAX)
    {
        __proc_cmd_help();
    }

    return i_ret;
}

int adayo_proc_cmd_help(void)
{
    __proc_cmd_help();
    return 0;
}
