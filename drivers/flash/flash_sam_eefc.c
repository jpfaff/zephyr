/*
 * Copyright (c) 2017 comsuisse AG
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <logging/sys_log.h>
#include <kernel.h>
#include <device.h>
#include <string.h>
#include <flash.h>
#include <errno.h>
#include <init.h>
#include <soc.h>

struct flash_sam_dev_cfg {
	Efc *regs;
	u8_t periph_id;
	u32_t flash_offset;
};

struct flash_sam_dev_data {
	u32_t flash_id;
	u32_t flash_size;
	u32_t page_size;
	u32_t erase_len;
	u32_t planes_number;
	u32_t lock_bits_number;
	u8_t locked;
};

#if defined(CONFIG_FLASH_PAGE_LAYOUT)
static struct flash_pages_layout dev_layout;
#endif

#define DEV_NAME(dev) ((dev)->config->name)
#define DEV_CFG(dev) \
	((const struct flash_sam_dev_cfg *const)(dev)->config->config_info)
#define DEV_DATA(dev) \
	((struct flash_sam_dev_data *const)(dev)->driver_data)

static inline bool is_aligned(u32_t data, u32_t byte_alignment)
{
	return (data & (byte_alignment - 1)) ? false : true;
}

static int flash_sam_erase(struct device *dev, off_t offset, size_t len)
{
	const struct flash_sam_dev_cfg *dev_cfg = DEV_CFG(dev);
	struct flash_sam_dev_data *const dev_data = DEV_DATA(dev);

	Efc *const efc = dev_cfg->regs;
	u16_t start_page;
	int ret;

	if (dev_data->locked) {
		return -EACCES;
	}

	/* Check incoming data alignment */
	if (!is_aligned(offset, dev_data->page_size)) {
		return -EINVAL;
	}

	/* Check if controller is ready */
	if ((efc->EEFC_FSR & 0x1) != 1) {
		return -EBUSY;
	}

	start_page = offset / dev_data->page_size;

	/* Erase entire flash */
	if ((start_page == 0) && (len == dev_data->flash_size)) {
		/* Erase entire flash */
		ret = soc_iap_send_flash_cmd(0, EEFC_FCR_FCMD_EA
					      | EEFC_FCR_FARG(0)
					      | EEFC_FCR_FKEY_PASSWD);
		if (ret != 0x1) {
			return -EIO;
		} else {
			return 0;
		}
	}

	if (is_aligned(len, dev_data->erase_len)
				&& is_aligned(offset, dev_data->erase_len)) {
		/* erase 16 pages at a time */
		for (int i = 0; i < (len / (dev_data->erase_len)); i++) {
			/* erase 16 pages */
			ret = soc_iap_send_flash_cmd(0, EEFC_FCR_FCMD_EPA
					| EEFC_FCR_FARG(start_page | 0x2)
					| EEFC_FCR_FKEY_PASSWD);
			if (ret != 0x1) {
				return -EIO;
			}
			start_page += dev_data->erase_len / dev_data->page_size;
		}
		return 0;
	}

	return -EINVAL;
}

static int flash_sam_read(struct device *dev, off_t offset,
				void *data, size_t len)
{
	const struct flash_sam_dev_cfg *dev_cfg = DEV_CFG(dev);
	struct flash_sam_dev_data *const dev_data = DEV_DATA(dev);
	void *addr;

	/* Check incoming data */
	if ((offset + len) > dev_data->flash_size) {
		return -EINVAL;
	}

	addr = (void *)(offset + dev_cfg->flash_offset);
	memcpy(data, addr, len);

	return 0;
}

static int flash_sam_write(struct device *dev, off_t offset,
				const void *data, size_t len)
{
	const struct flash_sam_dev_cfg *dev_cfg = DEV_CFG(dev);
	struct flash_sam_dev_data *const dev_data = DEV_DATA(dev);

	Efc *const efc = dev_cfg->regs;
	void *buffer_pointer;
	u16_t start_page;
	u16_t end_page;
	u32_t copy_len;
	int ret;

	if (dev_data->locked) {
		return -EACCES;
	}

	/* Check incoming data, 128bit aligned, not exceeding flash size */
	if ((!is_aligned(len, 16)) || (!is_aligned(offset, 16))) {
		return -EINVAL;
	}

	if ((offset + len) > dev_data->flash_size) {
		return -EINVAL;
	}

	buffer_pointer = (void *)(dev_cfg->flash_offset);

	start_page = offset / dev_data->page_size;
	end_page = (offset + (len - 4)) / dev_data->page_size;

	buffer_pointer += offset;
	for (int i = start_page; i <= end_page; i++) {
		/* write data into latch buffer, done by page */
		if (len > dev_data->page_size) {
			copy_len = dev_data->page_size;
		} else {
			copy_len = len;
		}

		memcpy(buffer_pointer, data, copy_len);
		buffer_pointer += copy_len;
		data += copy_len;

		/* Check if controller is ready */
		if ((efc->EEFC_FSR & 0x1) != 1) {
			return -EBUSY;
		}

		/* write command to controller */
		ret = soc_iap_send_flash_cmd(0, EEFC_FCR_FCMD_WP
					      | EEFC_FCR_FARG(i)
					      | EEFC_FCR_FKEY_PASSWD);

		if (ret != 0x1) {
			return -EIO;
		}
	}

	return 0;
}

static int flash_sam_write_protection(struct device *dev, bool enable)
{
	const struct flash_sam_dev_cfg *dev_cfg = DEV_CFG(dev);
	struct flash_sam_dev_data *const dev_data = DEV_DATA(dev);

	Efc *const efc = dev_cfg->regs;
	u32_t pages_per_lock;
	u32_t lock_size;
	int ret;
	int status;

	lock_size = dev_data->flash_size / dev_data->lock_bits_number;
	pages_per_lock = lock_size / dev_data->page_size;

	/* Check if controller is ready */
	if ((efc->EEFC_FSR & 0x1) != 1) {
		return -EBUSY;
	}

	/* Check lock status */
	ret = soc_iap_send_flash_cmd(0, EEFC_FCR_FCMD_GLB
				      | EEFC_FCR_FARG(0)
				      | EEFC_FCR_FKEY_PASSWD);
	status = efc->EEFC_FRR;

	if (status != 0x0) {
		/* Disable all HW Locks */
		for (int i = 0; i < dev_data->lock_bits_number; i++) {
			ret = soc_iap_send_flash_cmd(0, EEFC_FCR_FCMD_CLB
					    | EEFC_FCR_FARG(i * pages_per_lock)
					    | EEFC_FCR_FKEY_PASSWD);
		}
	}

	/* Set SW Lock */
	dev_data->locked = enable;

	/* Lock / Unlock entire Flash */
	/*for (int i = 0; i < dev_data->lock_bits_number; i++) {
		if (enable) {
			ret = IAP_function(0, EEFC_FCR_FCMD_SLB
					    | EEFC_FCR_FARG(i * pages_per_lock)
					    | EEFC_FCR_FKEY_PASSWD);
		} else {
			ret = IAP_function(0, EEFC_FCR_FCMD_CLB
					    | EEFC_FCR_FARG(i * pages_per_lock)
					    | EEFC_FCR_FKEY_PASSWD);
		}
		if ((ret & 0xFFFE) != 0) {
			return -EIO;
		}
	}*/

	return 0;
}

#if defined(CONFIG_FLASH_PAGE_LAYOUT)
static void flash_sam_pages_layout(struct device *dev,
				     const struct flash_pages_layout **layout,
				     size_t *layout_size)
{
	*layout = &dev_layout;
	*layout_size = 1;
}
#endif

static int flash_sam_init(struct device *dev)
{
	const struct flash_sam_dev_cfg *dev_cfg = DEV_CFG(dev);
	struct flash_sam_dev_data *const dev_data = DEV_DATA(dev);

	Efc *const efc = dev_cfg->regs;

	efc->EEFC_FCR = EEFC_FCR_FCMD_GETD
		      | EEFC_FCR_FARG(0)
		      | EEFC_FCR_FKEY_PASSWD;

	while ((efc->EEFC_FSR & 0x1) != 1) {
		/* Wait until flash controller ready */
	}

	dev_data->flash_id = efc->EEFC_FRR;
	dev_data->flash_size = efc->EEFC_FRR;
	dev_data->page_size = efc->EEFC_FRR;
	dev_data->planes_number = efc->EEFC_FRR;

	dev_data->erase_len = dev_data->page_size * 16;

	for (int i = 0; i < dev_data->planes_number; i++) {
		efc->EEFC_FRR;
	}

	dev_data->lock_bits_number = efc->EEFC_FRR;
	dev_data->locked = 0;

#if defined(CONFIG_FLASH_PAGE_LAYOUT)
	dev_layout.pages_count = dev_data->flash_size / dev_data->erase_len;
	dev_layout.pages_size = dev_data->erase_len;
#endif

	SYS_LOG_INF("Device %s initialized", DEV_NAME(dev));

	return 0;
}

static const struct flash_driver_api flash_sam_api = {
	.write_protection = flash_sam_write_protection,
	.erase = flash_sam_erase,
	.write = flash_sam_write,
	.read = flash_sam_read,
	.write_block_size = 16,
#if defined(CONFIG_FLASH_PAGE_LAYOUT)
	.page_layout = flash_sam_pages_layout,
#endif
};

static const struct flash_sam_dev_cfg flash_sam_config = {
	.regs = EFC,
	.periph_id = ID_EFC,
	.flash_offset = CONFIG_FLASH_BASE_ADDRESS,
};

static struct flash_sam_dev_data flash_sam_data;

DEVICE_AND_API_INIT(flash_sam, CONFIG_SOC_FLASH_SAM_DEV_NAME,
			flash_sam_init, &flash_sam_data, &flash_sam_config,
			POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
			&flash_sam_api);

