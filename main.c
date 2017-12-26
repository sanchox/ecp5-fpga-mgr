#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <asm/uaccess.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>

#include <linux/spi/spi.h>

#include "lattice/SSPIEm.h"

//#define _SPIDEV_REPLACE_

struct ecp5
{
	struct spi_device *spi;
	int programming_result;
	int algo_size;
	unsigned char *algo_mem;
	int data_size;
	unsigned char *data_mem;
	struct miscdevice algo_char_device;
	struct miscdevice data_char_device;
};

struct spi_device *current_programming_ecp5;

#ifdef _SPIDEV_REPLACE_
struct spi_master *_spi_master = NULL;
struct spi_device *created_ecp5_spi_dev = NULL;

static struct spi_board_info ecp5_spi_device __initdata =
{
		.modalias =	"ecp5-device",
		.max_speed_hz =	30000000,
		.bus_num =	1,
		.chip_select = 0,
		.controller_data = 0,
		.mode =	SPI_MODE_0,
};

static struct spi_board_info generic_spi_device =
{
		.modalias =	"spidev",
		.max_speed_hz =	30000000,
		.bus_num =	1,
		.chip_select = 0,
		.controller_data = 0,
		.mode =	SPI_MODE_0,
};
#endif

/*
 * File operations
 */
int ecp5_sspi_algo_open(struct inode *inode, struct file *fp)
{
	struct ecp5 *ecp5_info = container_of(fp->private_data, struct ecp5, algo_char_device);

	fp->private_data = ecp5_info;

	return (0);
}


int ecp5_sspi_algo_release(struct inode *inode, struct file *fp)
{
	return (0);
}

int ecp5_sspi_data_open(struct inode *inode, struct file *fp)
{
	struct ecp5 *ecp5_info = container_of(fp->private_data, struct ecp5, data_char_device);

	fp->private_data = ecp5_info;

	return (0);
}


int ecp5_sspi_data_release(struct inode *inode, struct file *fp)
{
	return (0);
}

ssize_t ecp5_sspi_algo_read(struct file *fp, char __user *ubuf, size_t len,
		loff_t *offp)
{
	struct ecp5 *ecp5_info = fp->private_data;

	if (*offp > ecp5_info->algo_size)
		return (0);

	if (*offp + len > ecp5_info->algo_size)
		len = ecp5_info->algo_size - *offp;

	if (copy_to_user(ubuf, ecp5_info->algo_mem + *offp, len) != 0 )
	        return (-EFAULT);

	return (len);
}

ssize_t ecp5_sspi_data_read(struct file *fp, char __user *ubuf, size_t len,
		loff_t *offp)
{
	struct ecp5 *ecp5_info = fp->private_data;

	if (*offp > ecp5_info->data_size)
		return (0);

	if (*offp + len > ecp5_info->data_size)
		len = ecp5_info->data_size - *offp;

	if (copy_to_user(ubuf, ecp5_info->data_mem + *offp, len) != 0 )
	        return (-EFAULT);

	return (len);
}

ssize_t ecp5_sspi_algo_write(struct file *fp, const char __user *ubuf, size_t len,
		loff_t *offp)
{
	struct ecp5 *ecp5_info = fp->private_data;

	ecp5_info->algo_size = len + *offp;
	ecp5_info->algo_mem = krealloc(ecp5_info->algo_mem, len, GFP_KERNEL);
	if (!ecp5_info->algo_mem)
	{
		pr_err("ECP5: can't allocate enough memory\n");
		return (-EFAULT);
	}
	if (copy_from_user(ecp5_info->algo_mem + *offp, ubuf, len) != 0)
		return (-EFAULT);

	return (len);
}

ssize_t ecp5_sspi_data_write(struct file *fp, const char __user *ubuf, size_t len,
		loff_t *offp)
{
	struct ecp5 *ecp5_info = fp->private_data;

	ecp5_info->data_size = len + *offp;
	ecp5_info->data_mem = krealloc(ecp5_info->data_mem, len, GFP_KERNEL);
	if (!ecp5_info->data_mem)
	{
		pr_err("ECP5: can't allocate enough memory\n");
		return (-EFAULT);
	}
	if (copy_from_user(ecp5_info->data_mem + *offp, ubuf, len) != 0)
		return (-EFAULT);

//	pr_info("data_on_write");
//	int i,j;
//	for (i = 0; i < 1024;){
//		char buf[1024];
//		int l = 0;
//		for (j = 0; j < 16; ++j, ++i)
//		{
//			l += sprintf(buf + l,"%02x",data_mem[i]);
//		}
//		pr_info("%s", buf);
//		msleep(1);
//	}

	return (len);
}

struct file_operations algo_fops = {
	.owner = THIS_MODULE,
	.read = ecp5_sspi_algo_read,
	.write = ecp5_sspi_algo_write,
	.open = ecp5_sspi_algo_open,
	.release = ecp5_sspi_algo_release,
	.llseek = no_llseek
};

struct file_operations data_fops = {
	.owner = THIS_MODULE,
	.read = ecp5_sspi_data_read,
	.write = ecp5_sspi_data_write,
	.open = ecp5_sspi_data_open,
	.release = ecp5_sspi_data_release,
	.llseek = no_llseek
};


ssize_t algo_size_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ecp5 *dev_info = dev_get_drvdata(dev);
	return (sprintf(buf, "%d\n", dev_info->algo_size));
}

static ssize_t algo_size_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return (count);
}

ssize_t data_size_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ecp5 *dev_info = dev_get_drvdata(dev);
	return (sprintf(buf, "%d\n", dev_info->data_size));
}

static ssize_t data_size_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return (count);
}

ssize_t program_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ecp5 *dev_info = dev_get_drvdata(dev);
	return (sprintf(buf, "%d\n", dev_info->programming_result));
}

static ssize_t program_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ecp5 *dev_info = dev_get_drvdata(dev);
	current_programming_ecp5 = dev_info->spi;

	if (dev_info->spi != to_spi_device(dev)) {
		pr_err("\n");
	}

	dev_info->programming_result = SSPIEm_preset(dev_info->algo_mem,
			dev_info->algo_size, dev_info->data_mem,
			dev_info->data_size);
	pr_info("ECP5: Firmware preset result %d\n",
					dev_info->programming_result);
	dev_info->programming_result = SSPIEm(0xFFFFFFFF);

	if (dev_info->programming_result != 2)
		pr_err("ECP5: FPGA programming failed with code %d\n",
				dev_info->programming_result);
	else
		pr_info("ECP5: FPGA programming success\n");
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

static int __devinit ecp5_probe(struct spi_device *spi)
{
	int ret;
	struct ecp5 *ecp5_info = NULL;
	unsigned char *algo_cdev_name = NULL;
	unsigned char *data_cdev_name = NULL;

	pr_info("ECP5: device probing\n");

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	spi->max_speed_hz = 30000000;
	ret = spi_setup(spi);
	if (ret < 0)
		return (ret);

	ecp5_info = devm_kzalloc(&spi->dev, sizeof(*ecp5_info), GFP_KERNEL);
	if (!ecp5_info)
		return (-ENOMEM);

	spi_set_drvdata(spi, ecp5_info);
	ecp5_info->spi = spi;
	ecp5_info->programming_result = 0;

	ecp5_info->algo_char_device.minor = MISC_DYNAMIC_MINOR;
	algo_cdev_name = kzalloc(64, GFP_KERNEL);
	if (!algo_cdev_name) return (-ENOMEM);
//	algo_cdev_name[0] = 'a';
//	algo_cdev_name[1] = 'b';
//	algo_cdev_name[2] = 0;
	sprintf(algo_cdev_name, "ecp5-spi%d.%d-algo", spi->master->bus_num, spi->chip_select);
	ecp5_info->algo_char_device.name = algo_cdev_name;
	ecp5_info->algo_char_device.fops = &algo_fops;
	ret = misc_register(&ecp5_info->algo_char_device);
	if (ret) {
		pr_err("ECP5: can't register firmware algo image device\n");
		goto error_return;
	}

	ecp5_info->data_char_device.minor = MISC_DYNAMIC_MINOR;
	data_cdev_name = kzalloc(64, GFP_KERNEL);
	if (!data_cdev_name) return (-ENOMEM);
//	data_cdev_name[0] = 'a';
//	data_cdev_name[1] = 'c';
//	data_cdev_name[2] = 0;
	sprintf(data_cdev_name, "ecp5-spi%d.%d-data", spi->master->bus_num, spi->chip_select);
	ecp5_info->data_char_device.name = data_cdev_name;
	ecp5_info->data_char_device.fops = &data_fops;
	ret = misc_register(&ecp5_info->data_char_device);
	if (ret) {
		pr_err("ECP5: can't register firmware data image device\n");
		goto error_return;
	}

	ret = sysfs_create_group(&spi->dev.kobj, &ecp5_attr_group);
	if (ret)
	{
		pr_err("ECP5: failed to create attribute files\n");
		goto error_return;
	}

	pr_info("ECP5: device probed\n");

	return (0);

error_return:
	kzfree(algo_cdev_name);
	kzfree(data_cdev_name);
	return (-ENOMEM);
}


static int __devexit ecp5_remove(struct spi_device *spi)
{
	int err_code;
	struct ecp5 *ecp5_info = spi_get_drvdata(spi);

	pr_info("ECP5: device removing\n");

	err_code = misc_deregister(&ecp5_info->algo_char_device);
	if(err_code) {
		pr_err("ECP5: can't unregister firmware image device\n");
		return (err_code);
	}
	kzfree(ecp5_info->algo_char_device.name);

	err_code = misc_deregister(&ecp5_info->data_char_device);
	if(err_code) {
		pr_err("ECP5: can't unregister firmware image device\n");
		return (err_code);
	}
	kzfree(ecp5_info->data_char_device.name);

	sysfs_remove_group(&spi->dev.kobj, &ecp5_attr_group);

	kzfree(ecp5_info->algo_mem);
	kzfree(ecp5_info->data_mem);

	pr_info("ECP5: device removed\n");
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
	.id_table	= ecp5_ids,
	.probe	= ecp5_probe,
	//.remove	= ecp5_remove,
	.remove	= __devexit_p(ecp5_remove),
};

module_spi_driver(ecp5_driver);


/*
 * device initialization
 */
static int __init ecp5_sspi_init(void)
{
	int err_code;

#ifdef _SPIDEV_REPLACE_
	struct device *d;
	char _exist_dev_name[64];
#endif

	pr_info("ECP5: driver initialization\n");

	err_code = spi_register_driver(&ecp5_driver);
	if (err_code) {
		pr_err("can't register spi driver\n");
		return (err_code);
	}

#ifdef _SPIDEV_REPLACE_
	_spi_master = spi_busnum_to_master(ecp5_spi_device.bus_num);

	/* Search for already exist CS */
	sprintf(_exist_dev_name,"%s.%u", dev_name(&(_spi_master->dev)),
			ecp5_spi_device.chip_select);

	d = bus_find_device_by_name(&spi_bus_type, NULL, _exist_dev_name);
	if (d != NULL) {
		spi_unregister_device(to_spi_device(d));
	}

	created_ecp5_spi_dev = spi_new_device(_spi_master, &ecp5_spi_device);
	if (!created_ecp5_spi_dev)
	{
		pr_err("ECP5: can't register spi device\n");
		return (-ENODEV);
	}
#endif

	pr_info("ECP5: driver successfully inited\n");
	return (0);
}

static void __exit ecp5_sspi_exit(void)
{

	pr_info("ECP5: driver exiting\n");

	spi_unregister_driver(&ecp5_driver);

#ifdef _SPIDEV_REPLACE_
	spi_unregister_device(created_ecp5_spi_dev);

	spi_new_device(_spi_master, &generic_spi_device);
#endif

	pr_info("ECP5: driver successfully exited\n");
}

module_init(ecp5_sspi_init);
module_exit(ecp5_sspi_exit);

MODULE_AUTHOR("STC Metrotek");
MODULE_DESCRIPTION("Lattice ECP5 FPGA Slave SPI programming interface driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ecp5_sspi");
