/*
 * CSR sirfsoc vdss core file
 *
 * Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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

#define VDSS_SUBSYS_NAME "LVDS"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/delay.h>

#include <video/sirfsoc_vdss.h>
#include "lvdsc.h"
#include "vdss.h"

#define RGB_SLOT_TO_REG(a, b, c, d) (a | b << 8 | c << 16 | d << 24)

static struct sirfsoc_lvdsc {
	struct platform_device *pdev;
	void __iomem    *base;
	void __iomem	*rsc_base;

	int irq;
	enum vdss_lvdsc_mode mode;
	/*set to 0 or 1 to select the lcdc0 or lcdc1 */
	unsigned int source;
	struct clk	*clk;
} lvdsc;

static u32 lvdsc_lane_setting_6BIT[] = {
	RGB_SLOT_TO_REG(0x0a, 0x17, 0x16, 0x15), /* lane0_src0 */
	RGB_SLOT_TO_REG(0x14, 0x13, 0x12, 0x00), /* lane0_src1 */

	RGB_SLOT_TO_REG(0x03, 0x02, 0x0f, 0x0e), /* lane1_src0 */
	RGB_SLOT_TO_REG(0x0d, 0x0c, 0x0b, 0x00), /* lane1_src1 */

	RGB_SLOT_TO_REG(0x1a, 0x18, 0x19, 0x07), /* lane2_src0 */
	RGB_SLOT_TO_REG(0x06, 0x05, 0x04, 0x00), /* lane2_src1 */
//adayo chenw
	RGB_SLOT_TO_REG(0x1c, 0x1c, 0x1b, 0x1b), /* lane4_src0 */
	RGB_SLOT_TO_REG(0x1b, 0x1c, 0x1c, 0x00), /* lane4_src1 */
//adayo chenw end
	RGB_SLOT_TO_REG(0x1b, 0x09, 0x08, 0x10), /* lane3_src0 */
	RGB_SLOT_TO_REG(0x11, 0x01, 0x00, 0x00), /* lane3_src1 */

//	RGB_SLOT_TO_REG(0x1c, 0x1c, 0x1b, 0x1b), /* lane4_src0 */
//	RGB_SLOT_TO_REG(0x1b, 0x1c, 0x1c, 0x00), /* lane4_src1 */
};

static u32 lvdsc_lane_setting_8BIT[] = {
	RGB_SLOT_TO_REG(0x08, 0x15, 0x14, 0x13), /* lane0_src0 */
	RGB_SLOT_TO_REG(0x12, 0x11, 0x10, 0x00), /* lane0_src1 */

	RGB_SLOT_TO_REG(0x01, 0x00, 0x0d, 0x0c), /* lane1_src0 */
	RGB_SLOT_TO_REG(0x0b, 0x0a, 0x09, 0x00), /* lane1_src1 */

	RGB_SLOT_TO_REG(0x1a, 0x18, 0x19, 0x05), /* lane2_src0 */
	RGB_SLOT_TO_REG(0x04, 0x03, 0x02, 0x00), /* lane2_src1 */
//adayo chenw 
	RGB_SLOT_TO_REG(0x1c, 0x1c, 0x1b, 0x1b), /* lane4_src0 */
	RGB_SLOT_TO_REG(0x1b, 0x1c, 0x1c, 0x00), /* lane4_src1 */
//adayo chenw end
	RGB_SLOT_TO_REG(0x1b, 0x07, 0x06, 0x0f), /* lane3_src0 */
	RGB_SLOT_TO_REG(0x0e, 0x17, 0x16, 0x00), /* lane3_src1 */

//	RGB_SLOT_TO_REG(0x1c, 0x1c, 0x1b, 0x1b), /* lane4_src0 */
//	RGB_SLOT_TO_REG(0x1b, 0x1c, 0x1c, 0x00), /* lane4_src1 */
};

static unsigned int lvdsc_read_reg(unsigned int offset)
{
	return readl(lvdsc.base + offset);
}

static void lvdsc_write_reg(unsigned int offset, unsigned int value)
{
	writel(value, lvdsc.base + offset);
}

static void lvdsc_write_rsc_reg(unsigned int offset, unsigned int value)
{
	writel(value, lvdsc.rsc_base + offset);
}
/*
 * Programming Guide of RGB888 LVDS panel configuration in PLL slave mode:
 * 1. Select source LCD controller by configuring LCDC_SWITCH_SEL bit in
 * Resource sharing (RSC) module.
 * 2. Initialize the source LCD controller
 *   i. Disable clock pause function
 *   ii. Set LCD controller to run in RGB888 output mode and don't enable
 *   LCD controller,Only pixel clock is generated by the LCD controller while
 *   all other parallel RGB signals are in in-active state
 * 3. Initialize the LVDS TX PHY after the power supply of LVDS PHY is stable
 *   i. Power on LVDS TX PHY by clearing PHY_PD bit.
 *   ii. Configure PLL by setting LVDS_CONFIG2, LVDS_CONFIG3 and LVDS_CONFIG4.
 *   Clear PHY_PLL_MODE bit to configure PLL in slave mode. PHY_M, PHY_N and
 *   PHY_FRACTIONAL have no function in this mode.
 *   iii. Configure the TX lane by setting relevant fields in LVDS_CONFIG1.
 *   iv. Remove LVDS TX PHY's core reset signal (lvdstx_nrst) by setting
 *   PHY_NRST bit.
 * 4. Initialize the Lane FIFO
 *   i. Configure LANE_FIFO_NFULL_THRES with a big value, such as the value of
 *   Lane FIFO's depth. In PLL slave mode, the frequency of Lane FIFO's
 *   write clock is equal to that of read clock.
 *   ii. LANE_FIFO_NEAREMPTY_THRES is useless in PLL slave mode, just load it
 *   with a number less than the value in LANE_FIFO_NEARFULL_THRES, such as
 *   half of the FIFO's depth.
 * 5. Initialize LVDS data mapping scheme
 * 6. Wait until the PLL is locked
 *   i. Wait for the value of PHY_PLL_LOCK going "1"
 * 7. Start PHY's parallel data input function
 *   i. Set LANE_FIFO_OP_ENA bit
 *   ii. Remove PHY_FIFO_RST bit
 * 8. Enable LCD controller to generate valid display timing.
 */

int __lvdsc_set_format(enum vdss_lvdsc_fmt fmt)
{
	int ret = 0;
	u32 *pdata = NULL;
	u32 lvds_tx_lan0_src0;
	u32 lvds_tx_lan0_src1;
	u32 lvds_tx_lan1_src0;
	u32 lvds_tx_lan1_src1;
	u32 lvds_tx_lan2_src0;
	u32 lvds_tx_lan2_src1;
	u32 lvds_tx_lan3_src0;
	u32 lvds_tx_lan3_src1;
	u32 lvds_tx_lan4_src0;
	u32 lvds_tx_lan4_src1;

	switch (fmt) {
	case SIRFSOC_VDSS_LVDSC_FMT_VESA_6BIT:
		lvdsc_write_reg(LVDS_PHY_CFG1, 0x40);
		pdata = &lvdsc_lane_setting_6BIT[0];
		break;

	case SIRFSOC_VDSS_LVDSC_FMT_VESA_8BIT:
		pdata = &lvdsc_lane_setting_8BIT[0];
		break;

	default:
		return -1;
	}

	lvds_tx_lan0_src0 = pdata[0];
	lvds_tx_lan0_src1 = pdata[1];
	lvds_tx_lan1_src0 = pdata[2];
	lvds_tx_lan1_src1 = pdata[3];
	lvds_tx_lan2_src0 = pdata[4];
	lvds_tx_lan2_src1 = pdata[5];
	lvds_tx_lan3_src0 = pdata[6];
	lvds_tx_lan3_src1 = pdata[7];
	lvds_tx_lan4_src0 = pdata[8];
	lvds_tx_lan4_src1 = pdata[9];

	lvdsc_write_reg(LVDS_TX_LANE0_SRC0, lvds_tx_lan0_src0);
	lvdsc_write_reg(LVDS_TX_LANE0_SRC1, lvds_tx_lan0_src1);

	lvdsc_write_reg(LVDS_TX_LANE1_SRC0, lvds_tx_lan1_src0);
	lvdsc_write_reg(LVDS_TX_LANE1_SRC1, lvds_tx_lan1_src1);

	lvdsc_write_reg(LVDS_TX_LANE2_SRC0, lvds_tx_lan2_src0);
	lvdsc_write_reg(LVDS_TX_LANE2_SRC1, lvds_tx_lan2_src1);

	lvdsc_write_reg(LVDS_TX_LANE3_SRC0, lvds_tx_lan3_src0);
	lvdsc_write_reg(LVDS_TX_LANE3_SRC1, lvds_tx_lan3_src1);

	lvdsc_write_reg(LVDS_TX_LANE4_SRC0, lvds_tx_lan4_src0);
	lvdsc_write_reg(LVDS_TX_LANE4_SRC1, lvds_tx_lan4_src1);

	return ret;
}

int __lvdsc_set_mode(enum vdss_lvdsc_mode mode)
{
	u32 lvds_phy_cfg0 = 0;
	u32 lvds_phy_cfg2 = 0;
	u32 lvds_phy_cfg3 = 0;

	lvds_phy_cfg0 = 0x4;
	lvdsc_write_reg(LVDS_PHY_CFG0, lvds_phy_cfg0);

	switch (mode) {
	case SIRFSOC_VDSS_LVDSC_MODE_SLAVE:
		lvds_phy_cfg2 = PLL_MODE(0x0);
		lvds_phy_cfg2 |= IPLLLOGIC_A_SEL(0x1);
		lvds_phy_cfg2 |= IPLLLOGIC_B_SEL(0x1);
		lvds_phy_cfg2 |= LOCKMON_EN(0x1);
		lvds_phy_cfg2 |= LPF_C1_SEL(0x2);
		lvds_phy_cfg2 |= LPF_C2_SEL(0x0);
		lvds_phy_cfg2 |= LPF_R_SEL(0x3);
		lvds_phy_cfg2 |= LPF_R3_SEL(0x3);
		lvds_phy_cfg2 |= CP_I_SET(0x0);
		lvds_phy_cfg2 |= LPF_INTV(0x3);
		lvds_phy_cfg2 |= M(0x2);

		lvds_phy_cfg3 = N(0xa80);
		lvds_phy_cfg3 |= FRACTIONAL(0x0);
		break;

	case SIRFSOC_VDSS_LVDSC_MODE_SYN:
		lvds_phy_cfg2 = PLL_MODE(0x1);
		lvds_phy_cfg2 |= IPLLLOGIC_A_SEL(0x1);
		lvds_phy_cfg2 |= IPLLLOGIC_B_SEL(0x1);
		lvds_phy_cfg2 |= LOCKMON_EN(0x1);
		lvds_phy_cfg2 |= LPF_C1_SEL(0x1);
		lvds_phy_cfg2 |= LPF_C2_SEL(0x1);
		lvds_phy_cfg2 |= LPF_R_SEL(0x4);
		lvds_phy_cfg2 |= LPF_R3_SEL(0x1);
		lvds_phy_cfg2 |= CP_I_SET(0x6);
		lvds_phy_cfg2 |= LPF_INTV(0x3);
		lvds_phy_cfg2 |= M(0x1);

		lvds_phy_cfg3 = N(0x0d2e780);
		lvds_phy_cfg3 |= FRACTIONAL(0x1);
		break;

	default:
		VDSSERR("lvds working mode error\n");
		return -1;
	}

	lvdsc_write_reg(LVDS_PHY_CFG2, lvds_phy_cfg2);
	lvdsc_write_reg(LVDS_PHY_CFG3, lvds_phy_cfg3);

	lvds_phy_cfg0 |= 1 << 0;
	lvdsc_write_reg(LVDS_PHY_CFG0, lvds_phy_cfg0);

	return 0;
}

int lvdsc_setup(enum vdss_lvdsc_fmt fmt)
{
	u32 lvds_lane_fifo_ctrl = 0;
	u32 lvds_phy_cfg0 = 0;

	__lvdsc_set_format(fmt);

	__lvdsc_set_mode(lvdsc.mode);

	/* FIXME: suppose to have timeout to read the pll lock result */
	while (0 == (lvdsc_read_reg(LVDS_PHY_CFG2) & 0x80000000))
		cpu_relax();

	lvds_lane_fifo_ctrl  = 0x20 << 0;
	lvds_lane_fifo_ctrl |= 0x80 << 8;
	lvds_lane_fifo_ctrl |= 0x00 << 16;
	lvds_lane_fifo_ctrl |= 0x01 << 24;

	lvdsc_write_reg(LVDS_LANE_FIFO_CTRL, lvds_lane_fifo_ctrl);
	lvds_phy_cfg0 = 0x1;

	lvdsc_write_reg(LVDS_PHY_CFG0, lvds_phy_cfg0);

	return 0;
}

bool lvdsc_is_syn_mode(void)
{
	return (lvdsc.mode == SIRFSOC_VDSS_LVDSC_MODE_SYN);
}

int lvdsc_select_src(u32 lcdc_index)
{
	lvdsc.source = lcdc_index;

	if (lvdsc.source == 1)
		lvdsc_write_rsc_reg(RSC_PIN_MUX_SET, LVDSC_LCDCSRC_SEL);

	return 0;

}

static int sirfsoc_lvdsc_probe(struct platform_device *pdev)
{
	struct device_node *dn = pdev->dev.of_node;
	struct resource *res;
	const char *mode;
	int ret = 0;

	ret = of_property_read_string(dn, "lvds-mode", &mode);
	if (!ret) {
		if (!strcmp(mode, "SYN"))
			lvdsc.mode = SIRFSOC_VDSS_LVDSC_MODE_SYN;
		else if (!strcmp(mode, "SLAVE"))
			lvdsc.mode = SIRFSOC_VDSS_LVDSC_MODE_SLAVE;
		else {
			dev_info(&pdev->dev, "invalid lvds working mode, set to SYN\n");
			lvdsc.mode = SIRFSOC_VDSS_LVDSC_MODE_SYN;
		}
	} else {
		dev_info(&pdev->dev, "invalid lvds working mode, set to SYN\n");
		lvdsc.mode = SIRFSOC_VDSS_LVDSC_MODE_SYN;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		VDSSERR("can't get IORESOURCE_MEM\n");
		return -EINVAL;
	}

	lvdsc.pdev = pdev;
	lvdsc.base = devm_ioremap(&pdev->dev, res->start,
		resource_size(res));
	if (!lvdsc.base) {
		VDSSERR("can't ioremap lvds controller regisger\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		VDSSERR("can't get IORESOURCE_MEM\n");
		return -EINVAL;
	}

	lvdsc.rsc_base = devm_ioremap(&pdev->dev, res->start,
		resource_size(res));
	if (!lvdsc.rsc_base) {
		VDSSERR("can't ioremap rsc register\n");
		return -ENOMEM;
	}

	lvdsc.clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(lvdsc.clk)) {
		VDSSERR("Failed to get lvds clock!\n");
		return -ENODEV;
	}

	ret = clk_prepare_enable(lvdsc.clk);
	if (ret) {
		VDSSERR("Can't enable clock\n");
		return -EINVAL;
	}

	ret = device_reset(&pdev->dev);
	if (ret) {
		VDSSERR("Failed to reset lvdsc\n");
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sirfsoc_lvdsc_resume_early(struct device *dev)
{
	return clk_prepare_enable(lvdsc.clk);
}

static int sirfsoc_lvdsc_suspend(struct device *dev)
{
	clk_disable_unprepare(lvdsc.clk);
	return 0;
}

static const struct dev_pm_ops sirfsoc_lvdsc_pm_ops = {
	.resume_early	= sirfsoc_lvdsc_resume_early,
	.suspend	= sirfsoc_lvdsc_suspend,
};
#define SIRFVDSS_LVDS_PM_OPS (&sirfsoc_lvdsc_pm_ops)

#else

#define SIRFVDSS_LVDS_PM_OPS NULL

#endif /* CONFIG_PM_SLEEP */

static const struct of_device_id lvdsc_of_match[] = {
	{ .compatible = "sirf,atlas7-lvdsc", },
	{},
};

static struct platform_driver sirfsoc_lvdsc_driver = {
	.driver         = {
		.name   = "sirfsoc_lvdsc",
		.owner  = THIS_MODULE,
		.of_match_table = lvdsc_of_match,
		.pm	= SIRFVDSS_LVDS_PM_OPS,
	},
};

int __init lvdsc_init_platform_driver(void)
{
	return platform_driver_probe(&sirfsoc_lvdsc_driver,
		sirfsoc_lvdsc_probe);
}

void lvdsc_uninit_platform_driver(void)
{
	platform_driver_unregister(&sirfsoc_lvdsc_driver);
}
