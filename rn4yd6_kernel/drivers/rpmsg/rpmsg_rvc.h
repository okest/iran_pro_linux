
#ifndef __RPMSG_RVC_H__
#define __RPMSG_RVC_H__



#include <linux/types.h>

int rpmsg_rcv_init(struct rpmsg_channel *rpdev);

int rpmsg_rcv_deinit(void);

int rpmsg_rcv_msgmng(struct rpmsg_channel *rpdev, void *data, int len);


#endif //__RPMSG_RVC_H__


