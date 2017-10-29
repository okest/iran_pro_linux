/*
 * CSR sirfsoc vdss core file
 *
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>

#include <video/sirfsoc_vdss.h>

#include "vdss.h"

//static ssize_t screen_toplayer_show(struct sirfsoc_vdss_screen *scn,char *buf)
ssize_t screen_toplayer_show(struct sirfsoc_vdss_screen *scn,char *buf)
{
	struct sirfsoc_vdss_screen_info sinfo;
	scn->get_info(scn, &sinfo);
	return snprintf(buf, PAGE_SIZE, "%d\n", sinfo.top_layer);
}

ssize_t screen_toplayer_store(struct sirfsoc_vdss_screen *scn,const char *buf, size_t size)
//static ssize_t screen_toplayer_store(struct sirfsoc_vdss_screen *scn,const char *buf, size_t size)
{
	int toplayer;
	struct sirfsoc_vdss_screen_info sinfo;
	int r = 0;

	r = kstrtouint(buf, 0, &toplayer);
	if (r)
		return r;
	
	if (toplayer < SIRFSOC_VDSS_LAYER0 || toplayer > SIRFSOC_VDSS_LAYER3)
		return -EINVAL;

	scn->get_info(scn, &sinfo);
	
	if (sinfo.top_layer != toplayer) {
		sinfo.top_layer = toplayer;
		r = scn->set_info(scn, &sinfo);
		if (r)
			return r;
		r = scn->apply(scn);
		if (r)
			return r;
	}

	return size;
}

struct screen_attribute {
	struct attribute attr;
	ssize_t (*show)(struct sirfsoc_vdss_screen *, char *);
	ssize_t (*store)(struct sirfsoc_vdss_screen *, const char *, size_t);
};

#define SCREEN_ATTR(_name, _mode, _show, _store) \
	struct screen_attribute screen_attr_##_name = \
	__ATTR(_name, _mode, _show, _store)

static SCREEN_ATTR(screen_toplayer, S_IRUGO|S_IWUSR,
	screen_toplayer_show, screen_toplayer_store);

static const struct attribute *screen_sysfs_attrs[] = {
	&screen_attr_screen_toplayer.attr,
	NULL
};

static ssize_t screen_attr_show(struct kobject *kobj, struct attribute *attr,
		char *buf)
{
	struct sirfsoc_vdss_screen *scn;
	struct screen_attribute *screen_attr;

	scn = container_of(kobj, struct sirfsoc_vdss_screen, kobj);
	screen_attr = container_of(attr, struct screen_attribute, attr);

	if (!screen_attr->show)
		return -ENOENT;

	return screen_attr->show(scn, buf);
}

static ssize_t screen_attr_store(struct kobject *kobj, struct attribute *attr,
		const char *buf, size_t size)
{
	struct sirfsoc_vdss_screen *scn;
	struct screen_attribute *screen_attr;

	scn = container_of(kobj, struct sirfsoc_vdss_screen, kobj);
	screen_attr = container_of(attr, struct screen_attribute, attr);

	if (!screen_attr->store)
		return -ENOENT;

	return screen_attr->store(scn, buf, size);
}

static const struct sysfs_ops screen_sysfs_ops = {
	.show = screen_attr_show,
	.store = screen_attr_store,
};

static struct kobj_type screen_ktype = {
	.sysfs_ops = &screen_sysfs_ops,
	.default_attrs = (struct attribute **)screen_sysfs_attrs,
};

int vdss_init_screens_sysfs(u32 lcdc_index)
{
	int i;
	int r;
	const int num_scns = sirfsoc_vdss_get_num_screens(lcdc_index);
	struct platform_device *pdev = vdss_get_core_pdev();

	for (i = 0; i < num_scns; ++i) {
		struct sirfsoc_vdss_screen *scn =
			sirfsoc_vdss_get_screen(lcdc_index, i);
		if (!scn) {
			VDSSERR("failed to get valid screen\n");
			return -EINVAL;
		}

		r = kobject_init_and_add(&scn->kobj, &screen_ktype,
				&pdev->dev.kobj, "lcd%d-%s",
				lcdc_index, scn->name);
		if (r) {
			VDSSERR("failed to create screen sysfs files\n");
			goto err;
		}
	}

	return 0;

err:
	vdss_uninit_screens_sysfs(lcdc_index);

	return r;
}

void vdss_uninit_screens_sysfs(u32 lcdc_index)
{
	int i;
	const int num_scn = sirfsoc_vdss_get_num_screens(lcdc_index);

	for (i = 0; i < num_scn; ++i) {
		struct sirfsoc_vdss_screen *scn =
			sirfsoc_vdss_get_screen(lcdc_index, i);

		kobject_del(&scn->kobj);
		kobject_put(&scn->kobj);
	}
}
