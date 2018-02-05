/*
 * FPGA Manager Driver for Lattice ECP5.
 *
 *  Copyright (c) 2018 Aleksandr Gusarov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This driver adds support to the FPGA manager for configuring the SRAM of
 * Lattice ECP5 FPGAs through slave SPI.
 */

// TN1260
// FPGA-DS-02012-1.8

#include <linux/fpga/fpga-mgr.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/spi/spi.h>

#include "lattice/SSPIEm.h"

struct lattice_fpga_sspi_firmware
{
	u32 algo_size;
	u32 data_size;

	unsigned char payload[];
};

struct ecp5_fpga_priv
{
	struct spi_device *dev;
	struct gpio_desc *cs;
	struct gpio_desc *done;
	struct gpio_desc *init;
	struct gpio_desc *program;
	unsigned char *rx_tx_buff;
};

static DEFINE_MUTEX(programming_lock);
struct ecp5_fpga_priv *current_programming_ecp5;

#define ECP5_SPI_MAX_SPEED 30000000 /* Hz */
#define ECP5_SPI_MIN_SPEED 1000000 /* Hz */

int lattice_spi_pull_cs_low()
{
	gpiod_set_value(current_programming_ecp5->cs, 1);
	return 1;
}

int lattice_spi_pull_cs_high()
{
	gpiod_set_value(current_programming_ecp5->cs, 0);
	return 1;
}

int lattice_spi_transmit(unsigned char *trBuffer, int trCount)
{
	int i = 0;
	int res = 0;
	int n_bytes = trCount >> 3;

	for (i = 0; i < n_bytes; ++i)
	{
		current_programming_ecp5->rx_tx_buff[i] = trBuffer[i];
	}
	res = spi_write(current_programming_ecp5->dev, current_programming_ecp5->rx_tx_buff, n_bytes);

	return (res);
}

int lattice_spi_receive(unsigned char *rcBuffer, int rcCount)
{
	int i = 0;
	int res = 0;
	int n_bytes = rcCount >> 3;

	res = spi_read(current_programming_ecp5->dev, current_programming_ecp5->rx_tx_buff, n_bytes);

	for (i = 0; i < n_bytes; ++i)
	{
		rcBuffer[i] = current_programming_ecp5->rx_tx_buff[i];
	}
	return (res);
}

static enum fpga_mgr_states ecp5_fpga_ops_state(struct fpga_manager *mgr)
{
	struct ecp5_fpga_priv *priv = mgr->priv;

	return gpiod_get_value(priv->done) ? FPGA_MGR_STATE_OPERATING :
		FPGA_MGR_STATE_UNKNOWN;
}

static int ecp5_fpga_ops_write_init(struct fpga_manager *mgr,
				     struct fpga_image_info *info,
				     const char *buf, size_t count)
{
	struct ecp5_fpga_priv *priv = mgr->priv;
	struct spi_device *dev = priv->dev;

	if ((info->flags & FPGA_MGR_PARTIAL_RECONFIG)) {
		dev_err(&dev->dev,
			"Partial reconfiguration is not supported\n");
		return -ENOTSUPP;
	}

	/* assert PROGRAM */
	gpiod_set_value(priv->program, 1);

	/* delay for at least 70ns - tDPPINT, */
	/* this also more than 50ns - tPRGMRJ */
	mdelay(100);

	/* Check INIT is asserted i.e. the FPGA in initialization mode */
	if (!gpiod_get_value(priv->init)) {
		dev_err(&dev->dev, "Device program failed, FPGA can't go to initialization mode\n");
		return -EIO;
	}

	/* de-assert PROGRAM */
	gpiod_set_value(priv->program, 0);

	/* delay for at least 55ns - tINITL */
	mdelay(100);

	/* Check INIT is de-asserted i.e. the FPGA in configuration mode */
	if (gpiod_get_value(priv->init)) {
		dev_err(&dev->dev, "Device program failed, FPGA can't go to configuration mode\n");
		return -EIO;
	}

	return 0;
}

static int ecp5_fpga_ops_write(struct fpga_manager *mgr,
				const char *buf, size_t count)
{
	struct ecp5_fpga_priv *priv = mgr->priv;
	struct spi_device *dev = priv->dev;
	struct lattice_fpga_sspi_firmware *firmware = (struct lattice_fpga_sspi_firmware *) buf;
	int ret;

	if (!firmware)
		return -EIO;

	/* check firmware length */
	if (firmware->algo_size < 0 || firmware->algo_size > count ||
		firmware->data_size < 0 || firmware->data_size > count ||
		firmware->algo_size + firmware->data_size + 2 * sizeof(u32) > count)
		return -EIO;

	if (!mutex_trylock(&programming_lock))
	{
		dev_warn(&dev->dev, "can't lock programming mutex\n(maybe someone already programming your chip?)");
		return -EBUSY;
	}

	current_programming_ecp5 = priv;


	// here we call lattice programming code
	// 1 - preparing data
	ret = SSPIEm_preset(firmware->payload, firmware->algo_size, firmware->payload + firmware->algo_size,
			firmware->data_size);
	dev_dbg(&dev->dev, "SSPIEm_preset result %d\n", ret);
	// 2 - programming here
	ret = SSPIEm(0xFFFFFFFF);

	mutex_unlock(&programming_lock);

	if (ret != 2)
		dev_err(&dev->dev, "FPGA programming failed with code %d\n", ret);
	else
		dev_info(&dev->dev, "FPGA programming success\n");

	return 0;

}

static int ecp5_fpga_ops_write_complete(struct fpga_manager *mgr,
					 struct fpga_image_info *info)
{
	struct ecp5_fpga_priv *priv = mgr->priv;
	struct spi_device *dev = priv->dev;

	/* Check DONE is asserted */
	if (!gpiod_get_value(priv->done)) {
		dev_err(&dev->dev,
			"DONE was not asserted after firmware transfer\n");
		return -EIO;
	}

	return 0;
}

static const struct fpga_manager_ops ecp5_fpga_ops = {
	.state = ecp5_fpga_ops_state,
	.write_init = ecp5_fpga_ops_write_init,
	.write = ecp5_fpga_ops_write,
	.write_complete = ecp5_fpga_ops_write_complete,
};

static int ecp5_fpga_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct ecp5_fpga_priv *priv = NULL;
	int ret;

	struct fpga_manager *mgr;
	struct fpga_image_info *info;

	priv = devm_kzalloc(&spi->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->rx_tx_buff = kzalloc(4096, GFP_KERNEL);
	if (!priv->rx_tx_buff)
	{
		dev_err(dev, "can't allocate enough memory for rx_tx_buff\n");
		return -ENOMEM;
	}

	priv->dev = spi;

	/* Check board setup data. */
	/* TODO: Check this in TN1260 */
	if (spi->max_speed_hz > ECP5_SPI_MAX_SPEED) {
		dev_err(dev, "SPI speed is too high, maximum speed is "
			__stringify(ECP5_SPI_MAX_SPEED) "\n");
		return -EINVAL;
	}

	if (spi->max_speed_hz < ECP5_SPI_MIN_SPEED) {
		dev_err(dev, "SPI speed is too low, minimum speed is "
			__stringify(ECP5_SPI_MIN_SPEED) "\n");
		return -EINVAL;
	}

	/* TODO: Check this in TN1260 */
	if (spi->mode != SPI_MODE_0) {
		dev_err(dev, "Bad SPI mode\n");
		return -EINVAL;
	}

	/* TODO: Check this in TN1260 */
	if (spi->bits_per_word != 8) {
		dev_err(dev, "Must be 8 bits per word\n");
		return -EINVAL;
	}

	priv->cs = devm_gpiod_get(dev, "cs", GPIOD_OUT_LOW);
	if (IS_ERR(priv->cs)) {
		ret = PTR_ERR(priv->cs);
		dev_err(dev, "Failed to get CS GPIO: %d\n", ret);
		return ret;
	}

	/* Set up the GPIOs */
	/* TODO: Make this GPIO's optional, and if they not present in DT,*/
	/*       communicate to FPGA through SPI interface only */
	priv->done = devm_gpiod_get(dev, "done", GPIOD_IN);
	if (IS_ERR(priv->done)) {
		ret = PTR_ERR(priv->done);
		dev_err(dev, "Failed to get DONE GPIO: %d\n", ret);
		return ret;
	}

	priv->program = devm_gpiod_get(dev, "programn", GPIOD_OUT_LOW);
	if (IS_ERR(priv->program)) {
		ret = PTR_ERR(priv->program);
		dev_err(dev, "Failed to get PROGRAMN GPIO: %d\n", ret);
		return ret;
	}

	priv->init = devm_gpiod_get(dev, "initn", GPIOD_IN);
	if (IS_ERR(priv->init)) {
		ret = PTR_ERR(priv->init);
		dev_err(dev, "Failed to get INITN GPIO: %d\n", ret);
		return ret;
	}
	/* TODO: Support HOLDN signal */

	/* Register with the FPGA manager */
	ret = fpga_mgr_register(dev, "Lattice ECP5 FPGA Manager",
				 &ecp5_fpga_ops, priv);

	if (ret)
		return ret;

	mgr = fpga_mgr_get(dev);

	/* struct with information about the FPGA image to program. */
	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	/* flags indicates whether to do full or partial reconfiguration */
	info->flags = 0;

	/* Load the buffer to the FPGA */
	ret = fpga_mgr_firmware_load(mgr, info, "ecp5_sspi_fw.img");

	if (ret)
		fpga_mgr_unregister(dev);

	/* Release the FPGA manager */
	fpga_mgr_put(mgr);

	/* Deallocate the image info if you're done with it */
	devm_kfree(dev, info);

	return ret;
}

static int ecp5_fpga_remove(struct spi_device *spi)
{
	fpga_mgr_unregister(&spi->dev);
	return (0);
}

static const struct of_device_id ecp5_fpga_of_match[] = {
	{ .compatible = "lattice,ecp5-fpga-mgr", },
	{},
};
MODULE_DEVICE_TABLE(of, ecp5_fpga_of_match);

static struct spi_driver ecp5_fpga_driver = {
	.probe    = ecp5_fpga_probe,
	.remove   = ecp5_fpga_remove,
	.driver = {
		.name	= "ecp5spi",
		.of_match_table = of_match_ptr(ecp5_fpga_of_match),
	},
};

module_spi_driver(ecp5_fpga_driver);

MODULE_AUTHOR("Aleksandr Gusarov <alexandergusarov@gmail.com>");
MODULE_DESCRIPTION("Lattice ECP5 FPGA Manager");
MODULE_LICENSE("GPL v2");
