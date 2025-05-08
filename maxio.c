// SPDX-License-Identifier: GPL-2.0+
/*
 * drivers/net/phy/maxio.c
 *
 * Driver for maxio PHYs
 *
 * Author: zhao yang <yang_zhao@maxio-tech.com>
 *
 * Copyright (c) 2021 maxio technology, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/bitops.h>
#include <linux/phy.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/netdevice.h>

#define MAXIO_PHY_VER				"v1.7.6.1"
#define MAXIO_PAGE_SELECT			0x1f
#define MAXIO_MAE0621A_CLK_MODE_REG		0x02
#define MAXIO_MAE0621A_WORK_STATUS_REG		0x1d

#define MAXIO_MAE0621A_LCR_LED2_ACT		BIT(14)
#define MAXIO_MAE0621A_LCR_LED2_LINK1000	BIT(13)
#define MAXIO_MAE0621A_LCR_LED2_LINK100		BIT(11)
#define MAXIO_MAE0621A_LCR_LED2_LINK10		BIT(10)
#define MAXIO_MAE0621A_LCR_LED1_ACT		BIT(9)
#define MAXIO_MAE0621A_LCR_LED1_LINK1000	BIT(8)
#define MAXIO_MAE0621A_LCR_LED1_LINK100		BIT(6)
#define MAXIO_MAE0621A_LCR_LED1_LINK10		BIT(5)


static int maxio_read_paged(struct phy_device *phydev, int page, u32 regnum)
{
	int ret = 0, oldpage;

	oldpage = phy_read(phydev, MAXIO_PAGE_SELECT);
	if (oldpage >= 0) {
		phy_write(phydev, MAXIO_PAGE_SELECT, page);
		ret = phy_read(phydev, regnum);
	}
	phy_write(phydev, MAXIO_PAGE_SELECT, oldpage);

	return ret;
}

static int maxio_write_paged(struct phy_device *phydev, int page, u32 regnum, u16 val)
{
	int ret = 0, oldpage;

	oldpage = phy_read(phydev, MAXIO_PAGE_SELECT);
	if (oldpage >= 0) {
		phy_write(phydev, MAXIO_PAGE_SELECT, page);
		ret = phy_write(phydev, regnum, val);
	}
	phy_write(phydev, MAXIO_PAGE_SELECT, oldpage);

	return ret;
}

static int maxio_mae0621a_clk_init(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.dev;
	u32 workmode,clkmode,oldpage;

	oldpage = phy_read(phydev, MAXIO_PAGE_SELECT);
	if (oldpage == 0xFFFF)	{
		oldpage = phy_read(phydev, MAXIO_PAGE_SELECT);
	}

	/* soft reset */
	phy_write(phydev, MAXIO_PAGE_SELECT, 0x0);
	phy_write(phydev, MII_BMCR, 0x9140);

	/* get workmode */
	phy_write(phydev, MAXIO_PAGE_SELECT, 0xa43);
	workmode = phy_read(phydev, MAXIO_MAE0621A_WORK_STATUS_REG);

	/* get clkmode */
	phy_write( phydev, MAXIO_PAGE_SELECT, 0xd92 );
	clkmode = phy_read( phydev, MAXIO_MAE0621A_CLK_MODE_REG );

	/* abnormal */
	if (0 == (workmode&BIT(5))) {
		if (0 == (clkmode&BIT(8))) {
			/* oscillator */
			phy_write(phydev, 0x02, clkmode | BIT(8));
			dev_dbg(dev,"****maxio_mae0621a_clk_init**clkmode**0x210a: 0x%x\n", phydev->phy_id);
		} else {
			/* crystal */
			dev_dbg(dev,"****maxio_mae0621a_clk_init**clkmode**0x200a: 0x%x\n", phydev->phy_id);
			phy_write(phydev, 0x02, clkmode &(~ BIT(8)));
		}
	}

	phy_write(phydev, MAXIO_PAGE_SELECT, 0x0);
	phy_write(phydev, MAXIO_PAGE_SELECT, oldpage);
	maxio_write_paged(phydev, 0xd04, 0x10, (MAXIO_MAE0621A_LCR_LED2_ACT|MAXIO_MAE0621A_LCR_LED2_LINK1000|MAXIO_MAE0621A_LCR_LED1_LINK1000|MAXIO_MAE0621A_LCR_LED1_LINK100|MAXIO_MAE0621A_LCR_LED1_LINK10));

	msleep(10);

	return 0;
}

static int maxio_read_mmd(struct phy_device *phydev, int devnum, u16 regnum)
{
	int ret = 0, oldpage;
	oldpage = phy_read(phydev, MAXIO_PAGE_SELECT);

	if (devnum == MDIO_MMD_AN && regnum == MDIO_AN_EEE_ADV) { /* eee info */
		phy_write(phydev, MAXIO_PAGE_SELECT ,0);
		phy_write(phydev, 0xd, MDIO_MMD_AN);
		phy_write(phydev, 0xe, MDIO_AN_EEE_ADV);
		phy_write(phydev, 0xd, 0x4000 | MDIO_MMD_AN);
		ret = phy_read(phydev, 0x0e);
	} else {
		ret = -EOPNOTSUPP;
	}
	phy_write(phydev, MAXIO_PAGE_SELECT, oldpage);

	return ret;
}

static int maxio_write_mmd(struct phy_device *phydev, int devnum, u16 regnum, u16 val)
{
	int ret = 0, oldpage;
	oldpage = phy_read(phydev, MAXIO_PAGE_SELECT);

	if (devnum == MDIO_MMD_AN && regnum == MDIO_AN_EEE_ADV) { /* eee info */
		phy_write(phydev, MAXIO_PAGE_SELECT ,0);
		ret |= phy_write(phydev, 0xd, MDIO_MMD_AN);
		ret |= phy_write(phydev, 0xe, MDIO_AN_EEE_ADV);
		ret |= phy_write(phydev, 0xd, 0x4000 | MDIO_MMD_AN);
		ret |= phy_write(phydev, 0xe, val);
		msleep(5);
		ret |= genphy_restart_aneg(phydev);

	} else {
		ret = -EOPNOTSUPP;
	}
	phy_write(phydev, MAXIO_PAGE_SELECT, oldpage);

	return ret;
}

static int maxio_adcc_check(struct phy_device *phydev)
{
	int ret = 0;
	int adcvalue;
	u32 regval;
	int i;

	maxio_write_paged(phydev, 0xd96, 0x2, 0x1fff );
	maxio_write_paged(phydev, 0xd96, 0x2, 0x1000 );

	for(i = 0; i < 4;i++)
	{
		regval = 0xf908 + i * 0x100;
		maxio_write_paged(phydev, 0xd8f, 0xb, regval );
		adcvalue = maxio_read_paged(phydev, 0xd92, 0xb);
		if(adcvalue & 0x1ff)
		{
			 continue;
		}
		else
		{
			ret = -1;
			break;
		}
	}

	return ret;
}

static int maxio_self_check(struct phy_device *phydev,int checknum)
{
	struct device *dev = &phydev->mdio.dev;
	int ret = 0;
	int i;

	for(i = 0;i < checknum; i++)
	{
		ret = maxio_adcc_check(phydev);
		if(0 == ret)
		{
			dev_dbg(dev,"MAE0621A READY\n");
			break;
		}
		else
		{
			maxio_write_paged(phydev, 0x0, 0x0, 0x1940 );
			phy_write(phydev, MAXIO_PAGE_SELECT, 0x0);
			msleep(5);
			maxio_write_paged(phydev, 0x0, 0x0, 0x1140 );
			maxio_write_paged(phydev, 0x0, 0x0, 0x9140 );
		}
	}

	maxio_write_paged(phydev, 0xd96, 0x2, 0xfff );
	maxio_write_paged(phydev, 0x0, 0x0, 0x9140 );
	phy_write(phydev, MAXIO_PAGE_SELECT, 0x0);

	return ret;
}

static int maxio_mae0621a_config_aneg(struct phy_device *phydev)
{
	return genphy_config_aneg(phydev);
}

static int maxio_mae0621a_config_init(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.dev;
	int ret = 0;
	u32 broken = 0;

	dev_dbg(dev,"MAXIO_PHY_VER: %s \n",MAXIO_PHY_VER);

	/* mdc set */
	ret = maxio_write_paged(phydev, 0xda0, 0x10, 0xf13 );

	maxio_mae0621a_clk_init(phydev);

	/* disable eee */
	dev_dbg(dev,"eee value: 0x%x \n",maxio_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_EEE_ADV));
	maxio_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_EEE_ADV, 0);
	dev_dbg(dev,"eee value: 0x%x \n",maxio_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_EEE_ADV));
	broken |= MDIO_EEE_100TX;
	broken |= MDIO_EEE_1000T;
	phydev->eee_broken_modes = broken;

	/* enable auto_speed_down */
	ret |= maxio_write_paged(phydev, 0xd8f, 0x0, 0x300 );

	/* adjust ANE */
	ret |= maxio_write_paged(phydev, 0xd90, 0x2, 0x1555);
	ret |= maxio_write_paged(phydev, 0xd90, 0x5, 0x2b15);
	ret |= maxio_write_paged(phydev, 0xd96, 0x13, 0x7bc );
	ret |= maxio_write_paged(phydev, 0xd8f, 0x8, 0x2500 );
	ret |= maxio_write_paged(phydev, 0xd91, 0x6, 0x6880 );
	ret |= maxio_write_paged(phydev, 0xd92, 0x14, 0xa);
	ret |= maxio_write_paged(phydev, 0xd91, 0x7, 0x5b00);
	/* clkout 125MHZ */
	ret |= maxio_write_paged(phydev, 0xa43, 0x19, 0x823);

	phy_write(phydev, MAXIO_PAGE_SELECT, 0x0);
	ret |= maxio_self_check(phydev,50);

	// ret = maxio_write_paged(phydev, 0xd04, 0x10, (MAXIO_MAE0621A_LCR_LED2_ACT|MAXIO_MAE0621A_LCR_LED2_LINK1000|MAXIO_MAE0621A_LCR_LED1_LINK1000|MAXIO_MAE0621A_LCR_LED1_LINK100|MAXIO_MAE0621A_LCR_LED1_LINK10));
	// if (ret < 0) {
	// 	dev_err(dev, "Failed to update the LCR register\n");
	// 	return ret;
	// } else if (ret) {
	// 	dev_dbg(dev, "the LCR register has been amended\n");
	// } else {
	// 	dev_dbg(dev, "the LCR register does not need to be modified\n");
	// }

	msleep(5);

	return 0;
}

static int maxio_mae0621a_resume(struct phy_device *phydev)
{
	int ret = genphy_resume(phydev);
	/* soft reset */
	ret |= phy_write(phydev, MII_BMCR, BMCR_RESET | phy_read(phydev, MII_BMCR));
	msleep(5);

	return ret;
}

static int maxio_mae0621a_suspend(struct phy_device *phydev)
{
	int ret = genphy_suspend(phydev);
	/* back to 0 page */
	ret |= phy_write(phydev, MAXIO_PAGE_SELECT ,0);

	return ret;
}

static int maxio_mae0621a_status(struct phy_device *phydev)
{
	return genphy_read_status(phydev);
}

static int maxio_mae0621a_probe(struct phy_device *phydev)
{
	int ret = maxio_mae0621a_clk_init(phydev);
	msleep(5);
	return ret;
}

static int maxio_mae0621aq3ci_config_init(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.dev;
	int ret = 0;

	dev_dbg(dev,"MAXIO_PHY_VER: %s \n",MAXIO_PHY_VER);

	/* MDC set */
	ret = maxio_write_paged(phydev, 0xdab, 0x17, 0xf13);

	/* auto speed down enable */
	ret |= maxio_write_paged(phydev, 0xd8f, 0x10, 0x300);

	/* adjust ANE */
	ret |= maxio_write_paged(phydev, 0xd96, 0x15, 0xc08a);
	ret |= maxio_write_paged(phydev, 0xda4, 0x12, 0x7bc);
	ret |= maxio_write_paged(phydev, 0xd8f, 0x16, 0x2500);

	ret |= maxio_write_paged(phydev, 0xd90, 0x16, 0x1555);
	ret |= maxio_write_paged(phydev, 0xd92, 0x11, 0x2b15);

	ret |= maxio_write_paged(phydev, 0xd96, 0x16, 0x4010);
	ret |= maxio_write_paged(phydev, 0xda5, 0x11, 0x4a12);
	ret |= maxio_write_paged(phydev, 0xda5, 0x12, 0x4a12);
	ret |= maxio_write_paged(phydev, 0xda8, 0x11, 0x175);
	ret |= maxio_write_paged(phydev, 0xd99, 0x16, 0xa);
	ret |= maxio_write_paged(phydev, 0xd95, 0x13, 0x5b00);

	/* clkout 125MHZ */
	ret |= maxio_write_paged(phydev, 0xa43, 0x19, 0x823);

	/* soft reset */
	ret |= maxio_write_paged(phydev, 0x0, 0x0, 0x9140);

	/* back to 0 page */
	ret |= phy_write(phydev, MAXIO_PAGE_SELECT, 0);

	ret |= maxio_write_paged(phydev, 0xd04, 0x10, (MAXIO_MAE0621A_LCR_LED2_ACT|MAXIO_MAE0621A_LCR_LED2_LINK1000|MAXIO_MAE0621A_LCR_LED1_LINK1000|MAXIO_MAE0621A_LCR_LED1_LINK100|MAXIO_MAE0621A_LCR_LED1_LINK10));

	return 0;
}

static int maxio_mae0621aq3ci_resume(struct phy_device *phydev)
{
	int ret = 0;

	ret = genphy_resume(phydev);
	ret |= maxio_write_paged(phydev, 0xdaa, 0x17, 0x1001 );

	/* led set */
	ret |= maxio_write_paged(phydev, 0xdab, 0x15, 0x0 );
	ret |= phy_write(phydev, MAXIO_PAGE_SELECT, 0);

	return ret;
}

static int maxio_mae0621aq3ci_suspend(struct phy_device *phydev)
{
	int ret = 0;

	ret = maxio_write_paged(phydev, 0xdaa, 0x17, 0x1011 );

	/* led set */
	ret |= maxio_write_paged(phydev, 0xdab, 0x15, 0x5550 );
	ret |= phy_write(phydev, MAXIO_PAGE_SELECT, 0);
	ret |= genphy_suspend(phydev);
	ret |= phy_write(phydev, MAXIO_PAGE_SELECT ,0);

	return ret;
}

static struct phy_driver maxio_nc_drvs[] = {
	{
		.phy_id		= 0x7b744411,
		.phy_id_mask	= 0x7fffffff,
		.name       = "MAE0621A-Q2C Gigabit Ethernet",
		.features	= PHY_GBIT_FEATURES,
		.probe          = maxio_mae0621a_probe,
		.config_init	= maxio_mae0621a_config_init,
		.config_aneg    = maxio_mae0621a_config_aneg,
		.read_status    = maxio_mae0621a_status,
		.suspend    = maxio_mae0621a_suspend,
		.resume     = maxio_mae0621a_resume,
	 },
	 {
		.phy_id		= 0x7b744412,
		.phy_id_mask	= 0x7fffffff,
		.name       = "MAE0621A/B-Q3C(I) Gigabit Ethernet",
		.features	= PHY_GBIT_FEATURES,
		.config_init	= maxio_mae0621aq3ci_config_init,
		.suspend    = maxio_mae0621aq3ci_suspend,
		.resume     = maxio_mae0621aq3ci_resume,
	 },
};

module_phy_driver(maxio_nc_drvs);
static struct mdio_device_id __maybe_unused maxio_nc_tbl[] = {
	{ 0x7b744411, 0x7fffffff },
	{ 0x7b744412, 0x7fffffff },
	{ }
};

MODULE_DEVICE_TABLE(mdio, maxio_nc_tbl);

MODULE_DESCRIPTION("Maxio PHY driver");
MODULE_AUTHOR("Zhao Yang");
MODULE_LICENSE("GPL");
