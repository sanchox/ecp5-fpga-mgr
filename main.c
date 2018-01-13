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

#include <linux/module.h>
#include <linux/delay.h>

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

#include <linux/of_gpio.h>

#include <linux/spi/spi.h>

#include "lattice/SSPIEm.h"

struct ecp5_fpga_priv
{
	struct spi_device *spi;
	struct gpio_desc *done;
	struct gpio_desc *init_n;
	struct gpio_desc *program_n;

	int programming_result;

	int algo_size;
	unsigned char *algo_mem;
	struct mutex algo_lock;
	struct miscdevice algo_char_device;

	int data_size;
	unsigned char *data_mem;
	struct mutex data_lock;
	struct miscdevice data_char_device;
};

static DEFINE_MUTEX(programming_lock);
struct ecp5_fpga_priv *current_programming_ecp5;

#define ECP5_SPI_MAX_SPEED 25000000 /* Hz */
#define ECP5_SPI_MIN_SPEED 1000000 /* Hz */

#define ECP5_SPI_RESET_DELAY 1 /* us (>200ns) */
#define ECP5_SPI_HOUSEKEEPING_DELAY 1200 /* us */

#define ECP5_SPI_NUM_ACTIVATION_BYTES DIV_ROUND_UP(49, 8)

#define GPIO(bank, nr) (((bank) - 1) * 32 + (nr))

#define CFG0          GPIO(1, 6)
#define CFG1          GPIO(1, 7)

#define FPGA_DONE     GPIO(1, 8)
#define FPGA_INITN    GPIO(1, 9)
#define FPGA_PROGRAMN GPIO(7, 11)
#define SPI_CS0       GPIO(5, 12)

unsigned char *rx_tx_buff = NULL;

int ecp5_reset_and_sspi_init()
{
	struct device *dev = &current_programming_ecp5->spi->dev;
	int n_retries = 0;

	rx_tx_buff = kzalloc(4096, GFP_KERNEL);
	if (!rx_tx_buff)
	{
		dev_err(dev, "can't allocate enough memory for rx_tx_buf\n");
		return (0);
	}

	gpio_request(CFG0,"sysfs");
	gpio_request(CFG1,"sysfs");

	gpio_request(FPGA_DONE,"sysfs");
	gpio_request(FPGA_INITN,"sysfs");
	gpio_request(FPGA_PROGRAMN,"sysfs");
	gpio_request(SPI_CS0,"sysfs");
	gpio_direction_output(SPI_CS0, 1);

	// set FPGA SPI slave mode, set SPI mux to redirect FPGA to ECSPI2 ARM pins instead of SPI flash
	gpio_direction_output(CFG0, true);
	gpio_direction_output(CFG1, false);

	// programn low
	gpio_direction_output(FPGA_PROGRAMN, false);

	// hold it...
	msleep(1);	// min 55 ns

	// wait until initn goes low
	gpio_direction_input(FPGA_INITN);

	n_retries = 0;
	while (n_retries < 100)
	{
		int initn = 0;

		msleep(1);
		initn = gpio_get_value(FPGA_INITN);
		if (!initn)
		{
			break;
		}
		++n_retries;
	}

	// programn high
	gpio_set_value(FPGA_PROGRAMN, true);

	// wait until initn goes high
	n_retries = 0;
	while (n_retries < 100)
	{
		int initn = 0;

		msleep(1);
		initn = gpio_get_value(FPGA_INITN);
		if (initn)
		{
			break;
		}
		++n_retries;
	}

	// wait at least 50 ms after toggling programn
	msleep(100);

	return (0);
}

int lattice_spi_transmission_final()
{
	kfree(rx_tx_buff);

	gpio_export(CFG0, 1);
	gpio_export(CFG1, 1);
	gpio_export(FPGA_DONE, 1);
	gpio_export(FPGA_INITN, 1);
	gpio_export(FPGA_PROGRAMN, 1);
	gpio_export(SPI_CS0, 1);

	gpio_free(CFG0);
	gpio_free(CFG1);
	gpio_free(FPGA_DONE);
	gpio_free(FPGA_INITN);
	gpio_free(FPGA_PROGRAMN);
	gpio_free(SPI_CS0);

	return (0);
}

int lattice_spi_transmit(unsigned char *trBuffer, int trCount)
{
	int i = 0;
	int res = 0;
	int n_bytes = trCount >> 3;

	for (i = 0; i < n_bytes; ++i)
	{
		rx_tx_buff[i] = trBuffer[i];
	}
	res = spi_write(current_programming_ecp5->spi, rx_tx_buff, n_bytes);

	return (!res);
}

int lattice_spi_receive(unsigned char *rcBuffer, int rcCount)
{
	int i = 0;
	int res = 0;
	int n_bytes = rcCount >> 3;

	res = spi_read(current_programming_ecp5->spi, rx_tx_buff, n_bytes);


	for (i = 0; i < n_bytes; ++i)
	{
		rcBuffer[i] = rx_tx_buff[i];
	}

	return (!res);
}

int lattice_spi_pull_cs_low(unsigned char channel)
{
	gpio_set_value(SPI_CS0, 0);
	return 1;
}

int lattice_spi_pull_cs_high(unsigned char channel)
{
	gpio_set_value(SPI_CS0, 1);
	return 1;
}


/*
 * File operations
 */
int ecp5_sspi_algo_open(struct inode *inode, struct file *fp)
{
	struct ecp5_fpga_priv *priv = container_of(fp->private_data, struct ecp5_fpga_priv, algo_char_device);
	struct device *dev = &priv->spi->dev;

	if (!mutex_trylock(&priv->algo_lock))
	{
		dev_err(dev, "ECP5: trying to open algo device while it already locked");
		return(-EBUSY);
	}

	fp->private_data = priv;

	return (0);
}


int ecp5_sspi_algo_release(struct inode *inode, struct file *fp)
{
	struct ecp5_fpga_priv *ecp5_info = fp->private_data;

	mutex_unlock(&ecp5_info->algo_lock);

	return (0);
}

int ecp5_sspi_data_open(struct inode *inode, struct file *fp)
{
	struct ecp5_fpga_priv *priv = container_of(fp->private_data, struct ecp5_fpga_priv, data_char_device);
	struct device *dev = &priv->spi->dev;

	if (!mutex_trylock(&priv->data_lock))
	{
		dev_err(dev, "ECP5: trying to open data device while it already locked");
		return(-EBUSY);
	}

	fp->private_data = priv;

	return (0);
}


int ecp5_sspi_data_release(struct inode *inode, struct file *fp)
{
	struct ecp5_fpga_priv *priv = fp->private_data;

	mutex_unlock(&priv->data_lock);

	return (0);
}

ssize_t ecp5_sspi_algo_read(struct file *fp, char __user *ubuf, size_t len,
		loff_t *offp)
{
	struct ecp5_fpga_priv *priv = fp->private_data;

	if (*offp > priv->algo_size)
		return (0);

	if (*offp + len > priv->algo_size)
		len = priv->algo_size - *offp;

	if (copy_to_user(ubuf, priv->algo_mem + *offp, len) != 0 )
	        return (-EFAULT);

	*offp += len;

	return (len);
}

ssize_t ecp5_sspi_data_read(struct file *fp, char __user *ubuf, size_t len,
		loff_t *offp)
{
	struct ecp5_fpga_priv *priv = fp->private_data;

	if (*offp > priv->data_size)
		return (0);

	if (*offp + len > priv->data_size)
		len = priv->data_size - *offp;

	if (copy_to_user(ubuf, priv->data_mem + *offp, len) != 0 )
	        return (-EFAULT);

	*offp += len;

	return (len);
}

ssize_t ecp5_sspi_algo_write(struct file *fp, const char __user *ubuf, size_t len,
		loff_t *offp)
{
	struct ecp5_fpga_priv *priv = fp->private_data;
	struct device *dev = &priv->spi->dev;

	ssize_t size = max_t(ssize_t, len + *offp, priv->algo_size);

	priv->algo_size = len + *offp;
	priv->algo_mem = krealloc(priv->algo_mem, size, GFP_KERNEL);
	if (!priv->algo_mem)
	{
		dev_err(dev, "ECP5: can't allocate enough memory\n");
		return (-EFAULT);
	}

	if (!mutex_trylock(&programming_lock))
	{
		dev_err(dev, "ECP5: can't write to algo device while programming");
		return(-EBUSY);
	}

	if (copy_from_user(priv->algo_mem + *offp, ubuf, len) != 0)
		return (-EFAULT);

	mutex_unlock(&programming_lock);

	priv->algo_size = size;
	*offp += len;

	return (len);
}

ssize_t ecp5_sspi_data_write(struct file *fp, const char __user *ubuf, size_t len,
		loff_t *offp)
{
	struct ecp5_fpga_priv *priv = fp->private_data;
	struct device *dev = &priv->spi->dev;

	ssize_t size = max_t(ssize_t, len + *offp, priv->data_size);

	priv->data_mem = krealloc(priv->data_mem, size, GFP_KERNEL);
	if (!priv->data_mem)
	{
		dev_err(dev, "ECP5: can't allocate enough memory\n");
		return (-EFAULT);
	}

	if (!mutex_trylock(&programming_lock))
	{
		dev_err(dev, "ECP5: can't write to data device while programming");
		return(-EBUSY);
	}

	if (copy_from_user(priv->data_mem + *offp, ubuf, len) != 0)
		return (-EFAULT);

	mutex_unlock(&programming_lock);


	priv->data_size = size;
	*offp += len;

	return (len);
}

loff_t ecp5_sspi_algo_lseek(struct file *fp, loff_t off, int whence)
{
	struct ecp5_fpga_priv *priv = fp->private_data;

        loff_t newpos;
        uint32_t size;

        size = priv->algo_size;

        switch(whence) {

        case SEEK_SET:
                newpos = off;
                break;

        case SEEK_CUR:
                newpos = fp->f_pos + off;
                break;

        case SEEK_END:
                newpos = size + off;
                break;

        default:
                return (-EINVAL);
        }

        if (newpos < 0)
                return (-EINVAL);

        if (newpos > size)
                newpos = size;

        fp->f_pos = newpos;

        return (newpos);
}

loff_t ecp5_sspi_data_lseek(struct file *fp, loff_t off, int whence)
{
	struct ecp5_fpga_priv *priv = fp->private_data;

        loff_t newpos;
        uint32_t size;

        size = priv->data_size;

        switch(whence) {

        case SEEK_SET:
                newpos = off;
                break;

        case SEEK_CUR:
                newpos = fp->f_pos + off;
                break;

        case SEEK_END:
                newpos = size + off;
                break;

        default:
                return (-EINVAL);
        }

        if (newpos < 0)
                return (-EINVAL);

        if (newpos > size)
                newpos = size;

        fp->f_pos = newpos;

        return (newpos);
}

struct file_operations algo_fops = {
	.owner = THIS_MODULE,
	.read = ecp5_sspi_algo_read,
	.write = ecp5_sspi_algo_write,
	.open = ecp5_sspi_algo_open,
	.release = ecp5_sspi_algo_release,
	.llseek = ecp5_sspi_algo_lseek,
};

struct file_operations data_fops = {
	.owner = THIS_MODULE,
	.read = ecp5_sspi_data_read,
	.write = ecp5_sspi_data_write,
	.open = ecp5_sspi_data_open,
	.release = ecp5_sspi_data_release,
	.llseek = ecp5_sspi_data_lseek,
};


ssize_t algo_size_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ecp5_fpga_priv *priv = dev_get_drvdata(dev);
	return (sprintf(buf, "%d\n", priv->algo_size));
}

static ssize_t algo_size_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return (count);
}

ssize_t data_size_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ecp5_fpga_priv *priv = dev_get_drvdata(dev);
	return (sprintf(buf, "%d\n", priv->data_size));
}

static ssize_t data_size_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return (count);
}

ssize_t program_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ecp5_fpga_priv *priv = dev_get_drvdata(dev);
	return (sprintf(buf, "%d\n", priv->programming_result));
}

static ssize_t program_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ecp5_fpga_priv *priv = dev_get_drvdata(dev);

	if (!mutex_trylock(&programming_lock))
	{
		dev_warn(dev, "ECP5: can't lock programming mutex \n");
		dev_dbg(dev, "ECP5: (maybe someone already programming your chip?)");
		return (count);
	}

	current_programming_ecp5 = priv;

	if (priv->spi != to_spi_device(dev)) {
		dev_err(dev, "ECP5: Mystical error occurred\n");
	}

	/* here we call lattice programming code */
	/* 1 - preparing data*/
	priv->programming_result = SSPIEm_preset(priv->algo_mem,
			priv->algo_size, priv->data_mem,
			priv->data_size);
	pr_debug("ECP5: SSPIEm_preset result %d\n",
					priv->programming_result);
	/* 2 - programming here */
	priv->programming_result = SSPIEm(0xFFFFFFFF);

	mutex_unlock(&programming_lock);

	if (priv->programming_result != 2)
		dev_err(dev, "ECP5: FPGA programming failed with code %d\n",
				priv->programming_result);
	else
		dev_dbg(dev, "ECP5: FPGA programming success\n");

	return (count);
}

struct device_attribute ecp5_algo_size_attr =
__ATTR(algo_size, 0666, algo_size_show, algo_size_store);

struct device_attribute ecp5_data_size_attr =
__ATTR(data_size, 0666, data_size_show, data_size_store);

struct device_attribute ecp5_program_attr =
__ATTR(program, 0666, program_show, program_store);

struct attribute *ecp5_attrs[] = {
	&ecp5_algo_size_attr.attr,
	&ecp5_data_size_attr.attr,
	&ecp5_program_attr.attr,
	NULL,
};

struct attribute_group ecp5_attr_group = {
	.attrs = ecp5_attrs,
};

//static const struct fpga_manager_ops ecp5_fpga_ops = {
//	.state = ecp5_fpga_ops_state,
//	.write_init = ecp5_fpga_ops_write_init,
//	.write = ecp5_fpga_ops_write,
//	.write_complete = ecp5_fpga_ops_write_complete,
//};

static int ecp5_fpga_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct ecp5_fpga_priv *priv = NULL;
	unsigned char *algo_cdev_name = NULL;
	unsigned char *data_cdev_name = NULL;
	int ret;

	pr_debug("ECP5: device spi%d.%d probing\n", spi->master->bus_num, spi->chip_select);

	priv = devm_kzalloc(&spi->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return (-ENOMEM);

	spi_set_drvdata(spi, priv);

	priv->spi = spi;

	/* Check board setup data. */
	if (spi->max_speed_hz > ECP5_SPI_MAX_SPEED) {
		dev_err(dev, "SPI speed is too high, maximum speed is "
			__stringify(ICE40_SPI_MAX_SPEED) "\n");
		return -EINVAL;
	}

	if (spi->max_speed_hz < ECP5_SPI_MIN_SPEED) {
		dev_err(dev, "SPI speed is too low, minimum speed is "
			__stringify(ICE40_SPI_MIN_SPEED) "\n");
		return -EINVAL;
	}

	if (spi->mode & SPI_CPHA) {
		dev_err(dev, "Bad SPI mode, CPHA not supported\n");
		return -EINVAL;
	}

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	ret = spi_setup(spi);
	if (ret < 0)
		return (ret);

	priv->programming_result = 0;

	priv->algo_char_device.minor = MISC_DYNAMIC_MINOR;
	algo_cdev_name = kzalloc(64, GFP_KERNEL);
	if (!algo_cdev_name) return (-ENOMEM);
	sprintf(algo_cdev_name, "ecp5-spi%d.%d-algo", spi->master->bus_num, spi->chip_select);
	priv->algo_char_device.name = algo_cdev_name;
	priv->algo_char_device.fops = &algo_fops;
	ret = misc_register(&priv->algo_char_device);
	if (ret) {
		dev_err(dev, "ECP5: can't register firmware algo image device\n");
		goto error_return;
	}
	mutex_init(&priv->algo_lock);

	priv->data_char_device.minor = MISC_DYNAMIC_MINOR;
	data_cdev_name = kzalloc(64, GFP_KERNEL);
	if (!data_cdev_name) return (-ENOMEM);
	sprintf(data_cdev_name, "ecp5-spi%d.%d-data", spi->master->bus_num, spi->chip_select);
	priv->data_char_device.name = data_cdev_name;
	priv->data_char_device.fops = &data_fops;
	ret = misc_register(&priv->data_char_device);
	if (ret) {
		dev_err(dev, "ECP5: can't register firmware data image device\n");
		goto error_return;
	}
	mutex_init(&priv->data_lock);

	ret = sysfs_create_group(&spi->dev.kobj, &ecp5_attr_group);
	if (ret)
	{
		dev_err(dev, "ECP5: failed to create attribute files\n");
		goto error_return;
	}

	dev_dbg(dev, "ECP5: device spi%d.%d probed\n", spi->master->bus_num, spi->chip_select);

	return (0);

error_return:
	kzfree(algo_cdev_name);
	kzfree(data_cdev_name);
	return (ret);
}


static int ecp5_remove(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct ecp5_fpga_priv *priv = spi_get_drvdata(spi);
	int err_code;

	dev_info(dev, "ECP5: device spi%d.%d removing\n", spi->master->bus_num, spi->chip_select);

	err_code = misc_deregister(&priv->algo_char_device);
	if(err_code) {
		dev_err(dev, "ECP5: can't unregister firmware image device\n");
		return (err_code);
	}
	kzfree(priv->algo_char_device.name);
	mutex_destroy(&priv->algo_lock);

	err_code = misc_deregister(&priv->data_char_device);
	if(err_code) {
		dev_err(dev, "ECP5: can't unregister firmware image device\n");
		return (err_code);
	}
	kzfree(priv->data_char_device.name);
	mutex_destroy(&priv->data_lock);

	sysfs_remove_group(&spi->dev.kobj, &ecp5_attr_group);

	kzfree(priv->algo_mem);
	kzfree(priv->data_mem);

	dev_info(dev, "ECP5: device spi%d.%d removed\n", spi->master->bus_num, spi->chip_select);
	return (0);
}

static const struct spi_device_id ecp5_ids[] = {
	{"ecp5-device"},
	{ },
};
MODULE_DEVICE_TABLE(spi, ecp5_ids);

static struct spi_driver ecp5_driver = {
	.driver = {
		.name	= "ecp5-driver",
		.owner	= THIS_MODULE,
	},
	.id_table = ecp5_ids,
	.probe    = ecp5_fpga_probe,
	.remove   = ecp5_remove,
};

module_spi_driver(ecp5_driver);


/*
 * module initialization
 */
static int __init ecp5_sspi_init(void)
{
	int err_code;

	pr_info("ECP5: driver initialization\n");

	err_code = spi_register_driver(&ecp5_driver);
	if (err_code) {
		pr_err("can't register spi driver\n");
		return (err_code);
	}

	pr_info("ECP5: driver successfully inited\n");
	return (0);
}

static void __exit ecp5_sspi_exit(void)
{

	pr_info("ECP5: driver exiting\n");

	spi_unregister_driver(&ecp5_driver);

	pr_info("ECP5: driver successfully exited\n");
}

module_init(ecp5_sspi_init);
module_exit(ecp5_sspi_exit);

MODULE_AUTHOR("STC Metrotek");
MODULE_DESCRIPTION("Lattice ECP5 FPGA Slave SPI programming interface driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ecp5-spi");
