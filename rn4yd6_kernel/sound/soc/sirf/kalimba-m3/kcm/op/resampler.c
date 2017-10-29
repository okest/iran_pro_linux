/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include "../kasobj.h"
#include "../kasop.h"
#include "../kcm.h"

static struct kasop_impl resampler_impl = {
};

static int __init kasop_init_resampler(void)
{
	return kcm_register_cap(CAPABILITY_ID_RESAMPLER, &resampler_impl);
}

subsys_initcall(kasop_init_resampler);
