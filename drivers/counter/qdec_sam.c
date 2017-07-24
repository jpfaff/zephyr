/*
 * Copyright (c) 2017 comsuisse AG
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 * @brief Atmel SAM MCU family Quadrature Decoder (TC) driver.
 */

#include <errno.h>
#include <misc/__assert.h>
#include <misc/util.h>
#include <device.h>
#include <init.h>
#include <soc.h>
#include <counter.h>

#define SYS_LOG_DOMAIN "dev/qdec_sam"
#define SYS_LOG_LEVEL 3
#include <logging/sys_log.h>

/* Device constant configuration parameters */
struct qdec_sam_dev_cfg {
	Tc *regs;
	void (*irq_config)(void);
	const struct soc_gpio_pin *pin_list;
	u8_t pin_list_size;
	u8_t periph_id[TCCHANNEL_NUMBER];
	u8_t irq_id[TCCHANNEL_NUMBER];
};

/* FIXME: remove if unused */
/* Device run time data */
struct qdec_sam_dev_data {
	u32_t position;
};

#define DEV_NAME(dev) ((dev)->config->name)
#define DEV_CFG(dev) \
	((const struct qdec_sam_dev_cfg *const)(dev)->config->config_info)
#define DEV_DATA(dev) \
	((struct qdec_sam_dev_data *const)(dev)->driver_data)

static int qdec_sam_start(struct device *dev)
{
	const struct qdec_sam_dev_cfg *const dev_cfg = DEV_CFG(dev);
	Tc *const tc = dev_cfg->regs;

	/* TODO: implement */

	return 0;
}

static int qdec_sam_stop(struct device *dev)
{
	const struct qdec_sam_dev_cfg *const dev_cfg = DEV_CFG(dev);
	Tc *const tc = dev_cfg->regs;

	/* TODO: implement */

	return 0;
}

static u32_t qdec_sam_read(struct device *dev)
{
	const struct qdec_sam_dev_cfg *const dev_cfg = DEV_CFG(dev);
	Tc *const tc = dev_cfg->regs;
	TcChannel *tc_ch0 = &tc->TC_CHANNEL[0];

	return tc_ch0->TC_CV;
}

static int qdec_sam_alarm(struct device *dev, counter_callback_t callback,
			u32_t count, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(callback);
	ARG_UNUSED(count);
	ARG_UNUSED(user_data);

	return -ENOTSUP;
}

/* FIXME: remove if interrupts are unused */
static void qdec_sam_isr(void *arg)
{
	struct device *dev = (struct device *)arg;
	const struct qdec_sam_dev_cfg *const dev_cfg = DEV_CFG(dev);
	struct qdec_sam_dev_data *const dev_data = DEV_DATA(dev);
	Tc *const tc = dev_cfg->regs;

	/* Clear QDEC Interrupt Status Register */
	(void)tc->TC_QISR;
}

static int qdec_sam_configure(struct device *dev)
{
	/* TODO: implement */

	return 0;
}

static int qdec_sam_initialize(struct device *dev)
{
	__ASSERT_NO_MSG(dev != NULL);
	const struct qdec_sam_dev_cfg *const dev_cfg = DEV_CFG(dev);
	int ret;

	/* Configure interrupts */
	dev_cfg->irq_config();

	/* Connect pins to the peripheral */
	soc_gpio_list_configure(dev_cfg->pin_list, dev_cfg->pin_list_size);

	for (int i = 0; i < ARRAY_SIZE(dev_cfg->periph_id); i++) {
		/* Enable module's clock */
		soc_pmc_peripheral_enable(dev_cfg->periph_id[i]);

		/* Enable module's IRQ */
		irq_enable(dev_cfg->irq_id[i]);
	}

	ret = qdec_sam_configure(dev);
	if (ret < 0) {
		return ret;
	}

	SYS_LOG_INF("Device %s initialized", DEV_NAME(dev));

	return 0;
}

static const struct counter_driver_api qdec_sam_driver_api = {
	.start = qdec_sam_start,
	.stop = qdec_sam_stop,
	.read = qdec_sam_read,
	.set_alarm = qdec_sam_alarm,
};

/* QDEC_0 */

#ifdef CONFIG_QDEC_0_SAM
static struct device DEVICE_NAME_GET(tc0_sam);

static void tc0_sam_irq_config(void)
{
	IRQ_CONNECT(TC0_IRQn, CONFIG_QDEC_0_SAM_IRQ_PRI, qdec_sam_isr,
		    DEVICE_GET(tc0_sam), 0);
	IRQ_CONNECT(TC1_IRQn, CONFIG_QDEC_0_SAM_IRQ_PRI, qdec_sam_isr,
		    DEVICE_GET(tc0_sam), 0);
	IRQ_CONNECT(TC2_IRQn, CONFIG_QDEC_0_SAM_IRQ_PRI, qdec_sam_isr,
		    DEVICE_GET(tc0_sam), 0);
}

static const struct soc_gpio_pin pins_tc0[] = {PIN_TC0_TIOA0, PIN_TC0_TIOB0};

static const struct qdec_sam_dev_cfg tc0_sam_config = {
	.regs = TC0,
	.irq_config = tc0_sam_irq_config,
	.pin_list = pins_tc0,
	.pin_list_size = ARRAY_SIZE(pins_tc0),
	.periph_id = {ID_TC0, ID_TC1, ID_TC2},
	.irq_id = {TC0_IRQn, TC1_IRQn, TC2_IRQn},
};

static struct qdec_sam_dev_data tc0_sam_data;

DEVICE_AND_API_INIT(tc0_sam, CONFIG_QDEC_0_SAM_NAME, &qdec_sam_initialize,
		    &tc0_sam_data, &tc0_sam_config, PRE_KERNEL_1,
		    CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &qdec_sam_driver_api);
#endif

/* QDEC_1 */

#ifdef CONFIG_QDEC_1_SAM
static struct device DEVICE_NAME_GET(tc1_sam);

static void tc1_sam_irq_config(void)
{
	IRQ_CONNECT(TC3_IRQn, CONFIG_QDEC_1_SAM_IRQ_PRI, qdec_sam_isr,
		    DEVICE_GET(tc1_sam), 0);
	IRQ_CONNECT(TC4_IRQn, CONFIG_QDEC_1_SAM_IRQ_PRI, qdec_sam_isr,
		    DEVICE_GET(tc1_sam), 0);
	IRQ_CONNECT(TC5_IRQn, CONFIG_QDEC_1_SAM_IRQ_PRI, qdec_sam_isr,
		    DEVICE_GET(tc1_sam), 0);
}

static const struct soc_gpio_pin pins_tc1[] = {PIN_TC1_TIOA0, PIN_TC1_TIOB0};

static const struct qdec_sam_dev_cfg tc1_sam_config = {
	.regs = TC1,
	.irq_config = tc1_sam_irq_config,
	.pin_list = pins_tc1,
	.pin_list_size = ARRAY_SIZE(pins_tc1),
	.periph_id = {ID_TC3, ID_TC4, ID_TC5},
	.irq_id = {TC3_IRQn, TC4_IRQn, TC5_IRQn},
};

static struct qdec_sam_dev_data tc1_sam_data;

DEVICE_AND_API_INIT(tc1_sam, CONFIG_QDEC_1_SAM_NAME, &qdec_sam_initialize,
		    &tc1_sam_data, &tc1_sam_config, PRE_KERNEL_1,
		    CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &qdec_sam_driver_api);
#endif

/* QDEC_2 */

#ifdef CONFIG_QDEC_2_SAM
static struct device DEVICE_NAME_GET(tc2_sam);

static void tc2_sam_irq_config(void)
{
	IRQ_CONNECT(TC6_IRQn, CONFIG_QDEC_2_SAM_IRQ_PRI, qdec_sam_isr,
		    DEVICE_GET(tc2_sam), 0);
	IRQ_CONNECT(TC7_IRQn, CONFIG_QDEC_2_SAM_IRQ_PRI, qdec_sam_isr,
		    DEVICE_GET(tc2_sam), 0);
	IRQ_CONNECT(TC8_IRQn, CONFIG_QDEC_2_SAM_IRQ_PRI, qdec_sam_isr,
		    DEVICE_GET(tc2_sam), 0);
}

static const struct soc_gpio_pin pins_tc2[] = {PIN_TC2_TIOA0, PIN_TC2_TIOB0};

static const struct qdec_sam_dev_cfg tc2_sam_config = {
	.regs = TC2,
	.irq_config = tc2_sam_irq_config,
	.pin_list = pins_tc2,
	.pin_list_size = ARRAY_SIZE(pins_tc2),
	.periph_id = {ID_TC6, ID_TC7, ID_TC8},
	.irq_id = {TC6_IRQn, TC7_IRQn, TC8_IRQn},
};

static struct qdec_sam_dev_data tc2_sam_data;

DEVICE_AND_API_INIT(tc2_sam, CONFIG_QDEC_2_SAM_NAME, &qdec_sam_initialize,
		    &tc2_sam_data, &tc2_sam_config, PRE_KERNEL_1,
		    CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &qdec_sam_driver_api);
#endif

/* QDEC_3 */

#ifdef CONFIG_QDEC_3_SAM
static struct device DEVICE_NAME_GET(tc3_sam);

static void tc3_sam_irq_config(void)
{
	IRQ_CONNECT(TC9_IRQn, CONFIG_QDEC_3_SAM_IRQ_PRI, qdec_sam_isr,
		    DEVICE_GET(tc3_sam), 0);
	IRQ_CONNECT(TC10_IRQn, CONFIG_QDEC_3_SAM_IRQ_PRI, qdec_sam_isr,
		    DEVICE_GET(tc3_sam), 0);
	IRQ_CONNECT(TC11_IRQn, CONFIG_QDEC_3_SAM_IRQ_PRI, qdec_sam_isr,
		    DEVICE_GET(tc3_sam), 0);
}

static const struct soc_gpio_pin pins_tc3[] = {PIN_TC3_TIOA0, PIN_TC3_TIOB0};

static const struct qdec_sam_dev_cfg tc3_sam_config = {
	.regs = TC3,
	.irq_config = tc3_sam_irq_config,
	.pin_list = pins_tc3,
	.pin_list_size = ARRAY_SIZE(pins_tc3),
	.periph_id = {ID_TC9, ID_TC10, ID_TC11},
	.irq_id = {TC9_IRQn, TC10_IRQn, TC11_IRQn},
};

static struct qdec_sam_dev_data tc3_sam_data;

DEVICE_AND_API_INIT(tc3_sam, CONFIG_QDEC_3_SAM_NAME, &qdec_sam_initialize,
		    &tc3_sam_data, &tc3_sam_config, PRE_KERNEL_1,
		    CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &qdec_sam_driver_api);
#endif
