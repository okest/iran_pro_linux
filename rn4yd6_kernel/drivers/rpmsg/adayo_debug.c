#include <linux/kernel.h>  
#include <linux/module.h> 
#include <linux/types.h>
#include <linux/kern_levels.h>


#include "adayo_debug.h"

#ifdef RPMSG_RVC_DEBUG

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

void adayo_debug_rpmsg(const unsigned char*puc_func, unsigned int ui_line, unsigned char *puc_buff, unsigned int ui_len)
{
    unsigned int ui_tmpcp = 0 ;
    unsigned int ui_tmplen, ui_tmpsync, ui_tmpfn, ui_tmpcmd, ui_ii;
    unsigned char uc_tmpchar, *puc_tmpact;
    unsigned char auc_tmpbuff[256];
    unsigned int ui_sum_inidx, ui_sum_outidx;
            
    while (ui_tmpcp < (ui_len - 1))
    {
        ui_tmpsync = 0;
        ui_tmpfn = 0;
        ui_tmplen = 0;
        ui_tmpcmd = 0;

        ui_tmpsync = (puc_buff[ui_tmpcp++] << 8);
        ui_tmpsync |= puc_buff[ui_tmpcp++];

        ui_sum_inidx = ui_tmpcp;

        uc_tmpchar = puc_buff[ui_tmpcp++];
        switch(uc_tmpchar & 0x3)
        {
            case 0:
                puc_tmpact = "no ack";
                break;
            case 1:
                puc_tmpact = "ack req";
                break;
            case 2:
                puc_tmpact = "ack answer";
                break;
            default:
                puc_tmpact = "ack error";
                break;
        }

        ui_tmpfn = (puc_buff[ui_tmpcp++] << 8);
        ui_tmpfn |= puc_buff[ui_tmpcp++];

        ui_tmplen = (puc_buff[ui_tmpcp++] << 8);
        ui_tmplen |= puc_buff[ui_tmpcp++];
        ui_tmplen = (ui_tmplen - 2);

        ui_tmpcmd = (puc_buff[ui_tmpcp++] << 8);
        ui_tmpcmd |= puc_buff[ui_tmpcp++];
        
        memset(auc_tmpbuff, 0, sizeof(auc_tmpbuff));
        sprintf(auc_tmpbuff, " sync=0x%x [%s:%s] fn=%d len=%d, cmd=0x%x ", \
            ui_tmpsync, ((uc_tmpchar & 0x80) != 0) ? "MCU->Mpu" : "Mpu->MCU", puc_tmpact,\
            ui_tmpfn, ui_tmplen, ui_tmpcmd);
        
        printk(KERN_EMERG"\nprintf rpmsg --------------------- [%s.%d]   in \n", puc_func, ui_line);

        printk(KERN_EMERG"%s \n", auc_tmpbuff);

        if (ui_tmplen >= (ui_len - ui_tmpcp))
        {
            printk(KERN_EMERG" len is error [ui_tmplen=%d] [%d]  \n", ui_tmplen, (ui_len - ui_tmpcp));
            printk(KERN_EMERG" printf rpmsg ------------------------------------   out \n");
            
            break;
        }
        if (ui_tmplen > 0)
        {
            if ((ui_tmplen >= 5) && (ui_tmpcmd == 0x8900))
            {
                printk(KERN_EMERG" PARAM: [fidx=%d] [send=%d] [recv=%d] [funcid=%d] [cmdid=%d]\n", \
                    puc_buff[ui_tmpcp++], puc_buff[ui_tmpcp++], puc_buff[ui_tmpcp++], puc_buff[ui_tmpcp++], puc_buff[ui_tmpcp++]);
                ui_tmplen = ui_tmplen - 5;
            }
            
            printk(KERN_EMERG" param list:   \n");
            memset(auc_tmpbuff, 0, sizeof(auc_tmpbuff));
            sprintf(&(auc_tmpbuff[strlen(auc_tmpbuff)]), "  ");\
            for (ui_ii = 0; ui_ii < ui_tmplen; ui_ii++)
            {
                /* 为了跟文档匹配上，param的一开始idx是0 */
                sprintf(&(auc_tmpbuff[strlen(auc_tmpbuff)]), "[%d.%#x] ", ui_ii, puc_buff[ui_tmpcp++]);
                if ((ui_ii != 0) && ((ui_ii & 0x7) == 0))\
                {
                    printk(KERN_EMERG"%s \n", auc_tmpbuff);
                    memset(auc_tmpbuff, 0, sizeof(auc_tmpbuff));
                    sprintf(&(auc_tmpbuff[strlen(auc_tmpbuff)]), "  ");
                }
            }
            if (((ui_tmplen - 1) & 0x7) != 0)
            {
                printk(KERN_EMERG"%s \n", auc_tmpbuff);
            }
        }
        else
        {
            printk(KERN_EMERG" no param list:   \n");
        }
        
		// 计算校验值  in
        ui_sum_outidx = ui_tmpcp;
        for (uc_tmpchar = 0, ui_ii = ui_sum_inidx; ui_ii < ui_sum_outidx; ui_ii++)
        {
            uc_tmpchar += puc_buff[ui_ii];
        }
        uc_tmpchar = ~(uc_tmpchar) + 1;
        // 计算校验值  out
        
        printk(KERN_EMERG"printf rpmsg [checksum=0x%x] ------------------------------------ [%d.%d, cpsum=0x%x]  out \n\n", \
            puc_buff[ui_tmpcp++], ui_sum_inidx, ui_sum_outidx, uc_tmpchar);
    }

    return;
}

#endif //RPMSG_RVC_DEBUG



