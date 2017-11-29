#ifndef _PROC_DEBUG_H_
#define _PROC_DEBUG_H_
#define PROC_DEBUG_NUM_MAX      30
#define PROC_DEBUG_PARAMLIST_MAX      30

#define POOC_DEBUG_NULL_STR     ""


typedef int (*cmd_callback)(unsigned int *puc_paramlist, unsigned int ui_paramnum);

typedef struct
{
    unsigned char *puc_cmd;                                         /* 命令行 */
    unsigned int ui_paramnum;                                        /* 参数个数 */
    unsigned char *puc_instruction;                                 /* 命令参数说明 */
    unsigned char *puc_ex;                                        /* 命令参数样例 */
    cmd_callback pf_cbl;
}PROC_DEBUG_SUB_S;



#define ADAYO_ADD_CMD(_cmd, _pno, _ins, _ex, _cb) ({\
    int i_ret;\
    PROC_DEBUG_SUB_S st_dbgsub;\
    st_dbgsub.puc_cmd = _cmd;\
    st_dbgsub.ui_paramnum = _pno;\
    st_dbgsub.puc_instruction = _ins;\
    st_dbgsub.puc_ex = _ex;\
    st_dbgsub.pf_cbl = _cb;\
    i_ret = adayo_proc_cmd_add(&st_dbgsub);\
    printk("%s.%d add [cmd=%s] [pno=%d] [ins=%s] [help=%s] [ret=%d] \n", \
        __FUNCTION__, __LINE__, ((_cmd != NULL) ? _cmd : POOC_DEBUG_NULL_STR), _pno, \
        ((_ins != NULL) ? _ins : POOC_DEBUG_NULL_STR), ((_ex != NULL) ? _ex : POOC_DEBUG_NULL_STR), i_ret);\
    i_ret;\
})

extern int adayo_proc_cmd_init(unsigned char *puc_proc_path);

extern int adayo_proc_cmd_add(PROC_DEBUG_SUB_S *pst_info);

extern int adayo_proc_cmd_del(PROC_DEBUG_SUB_S *pst_info);

extern int adayo_proc_cmd_do(unsigned char *puc_buff, unsigned int ui_bufflen);

extern int adayo_proc_cmd_help(void);

extern void adayo_debug_rpmsg(const unsigned char*puc_func, unsigned int ui_line, unsigned char *puc_buff, unsigned int ui_len);

#define ADAYO_DEBUG_BUFF(_buff, _len) do {adayo_debug_rpmsg(__FUNCTION__, __LINE__, _buff, _len);}while (0)

#else

static inline void adayo_debug_rpmsg(const unsigned char*puc_func, unsigned int ui_line, unsigned char *puc_buff, unsigned int ui_len)
{
    return ;
}

#define ADAYO_DEBUG_BUFF(_buff, _len) do {adayo_debug_rpmsg(__FUNCTION__, __LINE__, _buff, _len);}while (0)


#endif