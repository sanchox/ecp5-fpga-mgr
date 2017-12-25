//#include <linux/init.h>
//#include <linux/module.h>
//#include <linux/version.h>
//#include <linux/kernel.h>

#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/sysfs.h>
//#include <linux/major.h>
#include <linux/slab.h>
#include <linux/delay.h>
//#include <linux/proc_fs.h>
//#include <linux/devfs_fs_kernel.h>
//#include <linux/stat.h>
//#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>

//#include <linux/tty.h>
//#include <linux/selection.h>
//#include <linux/kmod.h>

#include <linux/spi/spi.h>

#include "lattice/SSPIEm.h"

int algo_size;
unsigned char *algo_mem;
int data_size;
unsigned char *data_mem;

struct spi_master *_spi_master = NULL;
struct spi_device *created_ecp5_spi_dev = NULL;
struct spi_device *current_programming_spi_dev;

struct ecp5
{
	struct spi_device *spi;
	int programming_result;
};

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

/*
 * File operations
 */
int ecp5_sspi_open(struct inode *inode, struct file *fp)
{
	return (0);
}


int ecp5_sspi_release(struct inode *inode, struct file *fp)
{
	return (0);
}

ssize_t ecp5_sspi_algo_read(struct file *fp, char __user *ubuf, size_t len,
		loff_t *offp)
{
	if (*offp > algo_size)
		return (0);

	if (*offp + len > algo_size)
		len = algo_size - *offp;

	if (copy_to_user(ubuf, algo_mem + *offp, len) != 0 )
	        return (-EFAULT);

	return (len);
}

ssize_t ecp5_sspi_data_read(struct file *fp, char __user *ubuf, size_t len,
		loff_t *offp)
{
	if (*offp > data_size)
		return (0);

	if (*offp + len > data_size)
		len = data_size - *offp;

	if (copy_to_user(ubuf, data_mem + *offp, len) != 0 )
	        return (-EFAULT);

	return (len);
}

ssize_t ecp5_sspi_algo_write(struct file *fp, const char __user *ubuf, size_t len,
		loff_t *offp)
{
	algo_size = len + *offp;
	algo_mem = krealloc(algo_mem, len, GFP_KERNEL);
	if (!algo_mem)
	{
		pr_err("can't allocate enough memory\n");
		return (-EFAULT);
	}
	if (copy_from_user(algo_mem + *offp, ubuf, len) != 0)
		return (-EFAULT);

	return (len);
}

ssize_t ecp5_sspi_data_write(struct file *fp, const char __user *ubuf, size_t len,
		loff_t *offp)
{
	data_size = len + *offp;
	data_mem = krealloc(data_mem, len, GFP_KERNEL);
	if (!data_mem)
	{
		pr_err("can't allocate enough memory\n");
		return (-EFAULT);
	}
	if (copy_from_user(data_mem + *offp, ubuf, len) != 0)
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
	.open = ecp5_sspi_open,
	.release = ecp5_sspi_release,
	.llseek = no_llseek
};

struct file_operations data_fops = {
	.owner = THIS_MODULE,
	.read = ecp5_sspi_data_read,
	.write = ecp5_sspi_data_write,
	.open = ecp5_sspi_open,
	.release = ecp5_sspi_release,
	.llseek = no_llseek
};

struct miscdevice firmware_algo_input_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ecp5-sspi-algo",
	.fops = &algo_fops,
};

struct miscdevice firmware_data_input_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ecp5-sspi-data",
	.fops = &data_fops,
};

ssize_t algo_size_show(struct device *dev, struct device_attribute *attr, char *buf)
{

	return (sprintf(buf, "%d\n", algo_size));
}

static ssize_t algo_size_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return (count);
}

ssize_t data_size_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return (sprintf(buf, "%d\n", data_size));
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

static ssize_t program_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	/***************************************************************
	*
	* Supported SSPIEm versions.
	*
	***************************************************************/

	#define VME_VERSION_NUMBER "4.0"

	/***************************************************************
	*
	* SSPI Embedded Return Codes.
	*
	***************************************************************/

	#define VME_VERIFICATION_FAILURE		-1
	#define VME_FILE_READ_FAILURE			-2
	#define VME_VERSION_FAILURE				-3
	#define VME_INVALID_FILE				-4
	#define VME_ARGUMENT_FAILURE			-5
	#define VME_CRC_FAILURE					-6


	struct ecp5 *dev_info = dev_get_drvdata(dev);
	current_programming_spi_dev = to_spi_device(dev);
	pr_info("Lattice Semiconductor Corp.\n");
	pr_info("SSPI Embedded(tm) V%s 2012\n", VME_VERSION_NUMBER);
	dev_info->programming_result = SSPIEm_preset(algo_mem, algo_size, data_mem,
			data_size);
	pr_info("SSPIEm_preset_result = %d\n", dev_info->programming_result);
	dev_info->programming_result = SSPIEm(0xFFFFFFFF);
	if (dev_info->programming_result != 2) {

		//print_out_string("\n\n");
		pr_info("+=======+\n");
		pr_info("| FAIL! |\n");
		pr_info("+=======+\n");
		pr_err("programming_result = %d\n", dev_info->programming_result);

	} else {
		pr_info("+=======+\n");
		pr_info("| PASS! |\n");
		pr_info("+=======+\n\n");
	}
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

	pr_info("Lattice ECP5 FPGA Slave SPI programming interface device probing\n");

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

	ret = sysfs_create_group(&spi->dev.kobj, &ecp5_attr_group);
	if (ret)
	{
		pr_debug("failed to create attribute files\n");
		return (ret);
	}

	pr_info("Lattice ECP5 FPGA Slave SPI programming interface device probed\n");

	return (0);
}


static int __devexit ecp5_remove(struct spi_device *spi)
{
	pr_info("Lattice ECP5 FPGA Slave SPI programming interface device removing\n");

	sysfs_remove_group(&spi->dev.kobj, &ecp5_attr_group);

	pr_info("Lattice ECP5 FPGA Slave SPI programming interface device removed\n");
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
	struct device *d;
	char _exist_dev_name[64];

	pr_info("Lattice ECP5 FPGA Slave SPI programming interface driver initialization\n");

	err_code = misc_register(&firmware_algo_input_device);
	if (err_code) {
		pr_err("can't register firmware algo image device\n");
		return (err_code);
	}

	pr_info("Lattice ECP5 FPGA Slave SPI programming interface driver initialization #1\n");


	err_code = misc_register(&firmware_data_input_device);
	if (err_code) {
		pr_err("can't register firmware data image device\n");
		return (err_code);
	}

	pr_info("Lattice ECP5 FPGA Slave SPI programming interface driver initialization #2\n");

	pr_info("Lattice ECP5 FPGA Slave SPI programming interface driver initialization #3\n");

	pr_info("Lattice ECP5 FPGA Slave SPI programming interface driver initialization #4\n");

	err_code = spi_register_driver(&ecp5_driver);
	if (err_code) {
		pr_err("can't register spi driver\n");
		return (err_code);
	}

	pr_info("Lattice ECP5 FPGA Slave SPI programming interface driver initialization #5\n");

	_spi_master = spi_busnum_to_master(ecp5_spi_device.bus_num);

	pr_info("Lattice ECP5 FPGA Slave SPI programming interface driver initialization #6\n");

	/* Search for already exist CS */
	sprintf(_exist_dev_name,"%s.%u", dev_name(&(_spi_master->dev)),
			ecp5_spi_device.chip_select);

	pr_info("Lattice ECP5 FPGA Slave SPI programming interface driver initialization #7\n");

	d = bus_find_device_by_name(&spi_bus_type, NULL, _exist_dev_name);
	if (d != NULL) {
		spi_unregister_device(to_spi_device(d));
	}

	pr_info("Lattice ECP5 FPGA Slave SPI programming interface driver initialization #8\n");

	created_ecp5_spi_dev = spi_new_device(_spi_master, &ecp5_spi_device);
	if (!created_ecp5_spi_dev)
	{
		pr_err("can't register spi device\n");
		return (-ENODEV);
	}


	pr_info("Lattice ECP5 FPGA Slave SPI programming interface driver successfully inited\n");
	return (0);
}

static void __exit ecp5_sspi_exit(void)
{
	int err_code;

	pr_info("Lattice ECP5 FPGA Slave SPI programming interface driver exiting\n");

	err_code = misc_deregister(&firmware_algo_input_device);
	if(err_code) {
		pr_err("can't unregister firmware image device\n");
		return;
	}

	err_code = misc_deregister(&firmware_data_input_device);
	if(err_code) {
		pr_err("can't unregister firmware image device\n");
		return;
	}

	kfree(algo_mem);
	kfree(data_mem);

	spi_unregister_driver(&ecp5_driver);

	spi_unregister_device(created_ecp5_spi_dev);

	spi_new_device(_spi_master, &generic_spi_device);

	pr_info("Lattice ECP5 FPGA Slave SPI programming interface driver successfully exited\n");
}

module_init(ecp5_sspi_init);
module_exit(ecp5_sspi_exit);

MODULE_AUTHOR("STC Metrotek");
MODULE_DESCRIPTION("Lattice ECP5 FPGA Slave SPI programming interface driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ecp5_sspi");
