
#include <osl.h>
#include <dhd_linux.h>
#include <linux/gpio.h>

#ifdef CUSTOMER_HW_PLATFORM
#include <plat/sdhci.h>
#define	sdmmc_channel	sdmmc_device_mmc0
#endif /* CUSTOMER_HW_PLATFORM */

#if defined(BUS_POWER_RESTORE) && defined(BCMSDIO)
#include <linux/mmc/core.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#endif /* defined(BUS_POWER_RESTORE) && defined(BCMSDIO) */

#include "../../../mmc/host/sdhci.h"


#ifdef CONFIG_DHD_USE_STATIC_BUF
extern void *dhd_wlan_mem_prealloc(int section, unsigned long size);
#endif /* CONFIG_DHD_USE_STATIC_BUF */

extern int gpio_wl_reg_on ;     
extern int gpio_wl_host_wake ;  
extern int gpio_wl_pwr ;

static int
dhd_wlan_set_power(bool on
#ifdef BUS_POWER_RESTORE
, wifi_adapter_info_t *adapter
#endif /* BUS_POWER_RESTORE */
)
{
	int err = 0;
	int data,data0;
	
	if (on) 
	{
		pr_info("======== PULL WL_REG_ON(%d) HIGH! ========\n", gpio_wl_reg_on);
		if (gpio_wl_reg_on >= 0) 
		{
			err = gpio_direction_output(gpio_wl_reg_on, 1);
			if (err) {
				printk(KERN_ALERT"%s: WL_REG_ON didn't output high\n", __FUNCTION__);
				return -EIO;
			}
		}
		
#if defined(BUS_POWER_RESTORE)

#if defined(BCMSDIO)
		if (adapter->sdio_func && adapter->sdio_func->card && adapter->sdio_func->card->host) {
			mmc_power_restore_host(adapter->sdio_func->card->host);
		}
#elif defined(BCMPCIE)
		OSL_SLEEP(50); /* delay needed to be able to restore PCIe configuration registers */
		if (adapter->pci_dev) {

			pci_set_power_state(adapter->pci_dev, PCI_D0);
			if (adapter->pci_saved_state)
				pci_load_and_free_saved_state(adapter->pci_dev, &adapter->pci_saved_state);
			pci_restore_state(adapter->pci_dev);
			err = pci_enable_device(adapter->pci_dev);
			if (err < 0)
				printk(KERN_ALERT"%s: PCI enable device failed", __FUNCTION__);
			pci_set_master(adapter->pci_dev);
		}
#endif /* BCMPCIE */

#endif /* BUS_POWER_RESTORE */

		/* Lets customer power to get stable */
		//mdelay(100);
	} 
	else 
	{
		
#if defined(BUS_POWER_RESTORE)

#if defined(BCMSDIO)
		if (adapter->sdio_func && adapter->sdio_func->card && adapter->sdio_func->card->host) {

			mmc_power_save_host(adapter->sdio_func->card->host);
		}
#elif defined(BCMPCIE)
		if (adapter->pci_dev) {
			pci_save_state(adapter->pci_dev);
			adapter->pci_saved_state = pci_store_saved_state(adapter->pci_dev);
			if (pci_is_enabled(adapter->pci_dev))
				pci_disable_device(adapter->pci_dev);
			pci_set_power_state(adapter->pci_dev, PCI_D3hot);
		}
#endif /* BCMPCIE */

#endif /* BUS_POWER_RESTORE */

		if (gpio_wl_reg_on >= 0) {
			err = gpio_direction_output(gpio_wl_reg_on, 0);
			if (err) {
				printk(KERN_ALERT"%s: WL_REG_ON didn't output low\n", __FUNCTION__);
				return -EIO;
			}
		}
	}
	
	data  = gpio_get_value(gpio_wl_pwr);
	data0 = gpio_get_value(gpio_wl_reg_on);

	pr_info("********** gpio_wl_pwr=%d  gpio_wl_reg_on=%d  ************\n", data,data0);
	
	return err;
}

static int dhd_wlan_set_reset(int onoff)
{
	return 0;
}

static int dhd_wlan_set_carddetect(bool present)
{
	int err = 0;

#if !defined(BUS_POWER_RESTORE)
	if (present) {
#if defined(BCMSDIO)

		pr_info("======== Card detection to detect SDIO card! ========\n");	
#ifdef CUSTOMER_HW_PLATFORM
//		err = sdhci_force_presence_change(&sdmmc_channel, 1);
#endif /* CUSTOMER_HW_PLATFORM */
		sdhci_bus_scan(1);
		
#elif defined(BCMPCIE)

#endif

	} 
	else 
	{
#if defined(BCMSDIO)
	pr_info("======== Card detection to detect SDIO card! ========\n");
		
#ifdef CUSTOMER_HW_PLATFORM
//		err = sdhci_force_presence_change(&sdmmc_channel, 0);
#endif /* CUSTOMER_HW_PLATFORM */

		sdhci_bus_scan(0);
		
#elif defined(BCMPCIE)

#endif

	}
#endif /* BUS_POWER_RESTORE */

	return err;
}

static int dhd_wlan_get_mac_addr(unsigned char *buf)
{
	int err = 0;

#ifdef EXAMPLE_GET_MAC
	/* EXAMPLE code */
	{
		struct ether_addr ea_example = {{0x00, 0x11, 0x22, 0x33, 0x44, 0xFF}};
		bcopy((char *)&ea_example, buf, sizeof(struct ether_addr));
	}
#endif /* EXAMPLE_GET_MAC */

	return err;
}

#if !defined(WL_WIRELESS_EXT)
struct cntry_locales_custom {
	char iso_abbrev[WLC_CNTRY_BUF_SZ];	/* ISO 3166-1 country abbreviation */
	char custom_locale[WLC_CNTRY_BUF_SZ];	/* Custom firmware locale */
	int32 custom_locale_rev;		/* Custom local revisin default -1 */
};
#endif

static struct cntry_locales_custom brcm_wlan_translate_custom_table[] = {
	/* Table should be filled out based on custom platform regulatory requirement */
	{"",   "XT", 49},  /* Universal if Country code is unknown or empty */
	{"US", "US", 0},
};

static void *dhd_wlan_get_country_code(char *ccode)
{
	struct cntry_locales_custom *locales;
	int size;
	int i;

	if (!ccode)
		return NULL;

	locales = brcm_wlan_translate_custom_table;
	size = ARRAY_SIZE(brcm_wlan_translate_custom_table);

	for (i = 0; i < size; i++)
		if (strcmp(ccode, locales[i].iso_abbrev) == 0)
			return &locales[i];
	return NULL;
}

struct resource dhd_wlan_resources[] = {
	[0] = {
		.name	= "bcmdhd_wlan_irq",
		.start	= 0, /* Dummy */
		.end	= 0, /* Dummy */
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_SHAREABLE
			| IORESOURCE_IRQ_HIGHLEVEL, /* Dummy */
	},
};

struct wifi_platform_data dhd_wlan_control = {
	.set_power	= dhd_wlan_set_power,
	.set_reset	= dhd_wlan_set_reset,
	.set_carddetect	= dhd_wlan_set_carddetect,
	.get_mac_addr	= dhd_wlan_get_mac_addr,
#ifdef CONFIG_DHD_USE_STATIC_BUF
	.mem_prealloc	= dhd_wlan_mem_prealloc,
#endif /* CONFIG_DHD_USE_STATIC_BUF */
	.get_country_code = dhd_wlan_get_country_code,
};

int dhd_wlan_init_gpio(void)
{
	int err = 0;
#ifdef CUSTOMER_OOB
	int host_oob_irq = -1;
	uint host_oob_irq_flags = 0;
#endif

	int gpio_wl_pwr_data , gpio_wl_reg_on_data;
	
	gpio_direction_output(gpio_wl_pwr, 1);
	gpio_wl_pwr_data    = gpio_get_value(gpio_wl_pwr);
	gpio_wl_reg_on_data = gpio_get_value(gpio_wl_reg_on);

	pr_info("**** gpio_wl_pwr=%d  gpio_wl_reg_on=%d  *******\n", gpio_wl_pwr_data ,gpio_wl_reg_on_data );
	
	if (gpio_wl_reg_on >= 0) {
		err = gpio_request(gpio_wl_reg_on, "WL_REG_ON");
		if (err < 0) {
			printk(KERN_ALERT"%s: Faiiled to request gpio %d for WL_REG_ON\n",
				__FUNCTION__, gpio_wl_reg_on);
			gpio_wl_reg_on = -1;
		}
	}

#ifdef CUSTOMER_OOB
	if (gpio_wl_host_wake >= 0) {
		err = gpio_request(gpio_wl_host_wake, "bcmdhd");
		if (err < 0) {
			printk(KERN_ALERT"%s: gpio_request failed\n", __FUNCTION__);
			return -1;
		}
		err = gpio_direction_input(gpio_wl_host_wake);
		if (err < 0) {
			printk(KERN_ALERT"%s: gpio_direction_input failed\n", __FUNCTION__);
			gpio_free(gpio_wl_host_wake);
			return -1;
		}
		host_oob_irq = gpio_to_irq(gpio_wl_host_wake);
		if (host_oob_irq < 0) {
			printk(KERN_ALERT"%s: gpio_to_irq failed\n", __FUNCTION__);
			gpio_free(gpio_wl_host_wake);
			return -1;
		}
	}

#ifdef HW_OOB
#ifdef HW_OOB_LOW_LEVEL
	host_oob_irq_flags = IORESOURCE_IRQ | IORESOURCE_IRQ_LOWLEVEL | IORESOURCE_IRQ_SHAREABLE;
#else
	host_oob_irq_flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL | IORESOURCE_IRQ_SHAREABLE;
#endif
#else
	host_oob_irq_flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE | IORESOURCE_IRQ_SHAREABLE;
#endif

	dhd_wlan_resources[0].start = dhd_wlan_resources[0].end = host_oob_irq;
	dhd_wlan_resources[0].flags = host_oob_irq_flags;

#endif /* CUSTOMER_OOB */

	return 0;
}

static void dhd_wlan_deinit_gpio(void)
{
	if (gpio_wl_reg_on >= 0) {
		gpio_free(gpio_wl_reg_on);
		gpio_wl_reg_on = -1;
	}
#ifdef CUSTOMER_OOB
	if (gpio_wl_host_wake >= 0) {
		gpio_free(gpio_wl_host_wake);
		gpio_wl_host_wake = -1;
	}
#endif /* CUSTOMER_OOB */
}

int dhd_wlan_init_plat_data(void)
{
	int err = 0;
	err = dhd_wlan_init_gpio();
	return err;
}

void dhd_wlan_deinit_plat_data(wifi_adapter_info_t *adapter)
{
	dhd_wlan_deinit_gpio();
}

