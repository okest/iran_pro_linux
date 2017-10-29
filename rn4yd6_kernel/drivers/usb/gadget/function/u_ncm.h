/*
 * u_ncm.h
 *
 * Utility definitions for the ncm function
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Andrzej Pietrasiewicz <andrzej.p@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef U_NCM_H
#define U_NCM_H

#include <linux/usb/composite.h>
#include <linux/device.h>
#include <linux/usb/gadget.h>

struct f_ncm_opts {
	struct usb_function_instance	func_inst;
	struct net_device		*net;
	bool				bound;

	/*
	 * Read/write access to configfs attributes is handled by configfs.
	 *
	 * This is to protect the data from concurrent access by read/write
	 * and create symlink/remove symlink.
	 */
	struct mutex			lock;
	int				refcnt;
};
struct carplay_dev {
	const char	*name;
	struct device	*dev;
	int		index;
};

struct iap2_dev{
	struct usb_ep                   *in_ep;
	struct usb_ep                   *out_ep;
	int online;
	int error;

	atomic_t read_excl;
	atomic_t write_excl;
	atomic_t open_excl;

	struct list_head rx_idle;

	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
	struct usb_request *tx_req;
	struct usb_request *rx_req;

	struct usb_gadget	*gadget;

	int tx_done;
	int rx_done;
	int ncm_dev_status;
};

#endif /* U_NCM_H */
