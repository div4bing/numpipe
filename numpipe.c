#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>

MODULE_LICENSE("DUAL BSD/GPL");
#define DRIVER_AUTHOR "Divyeshkumar Maisuria <dmaisur1@binghamton.edu>"
#define DRIVER_DESC   "Named Pipe for Exchanging Numbers"
#define DEVICE_NAME "numpipe"
#define SUCCESS 0

static int buffer_size = 0;			// Buffer size for the FIFO
// Module Parameter information
module_param(buffer_size, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(buffer_size, "Size of the FIFO for Number Pipe");

static int major_num;						// Major number of the character device
static int dev_open = 0;				// Counter of the number of times the device is openned and closed

static int __init init_numpipe(void);
static void __exit cleanup_numpipe(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);

static struct file_operations fileOps = {
	.open = device_open,
	.release = device_release
};

static int __init init_numpipe(void)
{
	printk(KERN_INFO "%s: Welcome to Num Pipe! Passed FIFO size is-> %d\n", DEVICE_NAME, buffer_size);
	printk(KERN_INFO "%s: Registering char device\n", DEVICE_NAME);

	major_num = register_chrdev(0, DEVICE_NAME, &fileOps);

	if (major_num < 0)
	{
	  printk(KERN_ALERT "%s: Registering char device failed with %d\n", DEVICE_NAME, major_num);
	  return major_num;
	}

	printk(KERN_INFO "%s: Assigned Major number is: %d\n", DEVICE_NAME, major_num);
	return 0;
}

static void __exit cleanup_numpipe(void)
{
	printk(KERN_INFO "%s: Unregistering the char device\n", DEVICE_NAME);
	unregister_chrdev(major_num, DEVICE_NAME);
}

static int device_open(struct inode *inode, struct file *file)		// Module open function, keeps track of number of times the module is openned
{
	if (dev_open)															// Module already in use
		return -EBUSY;

	dev_open++;
	printk( KERN_INFO "%s: Oppend Module\n", DEVICE_NAME);
	try_module_get(THIS_MODULE);

	return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file)
{
	dev_open--;														// Release module for next user
	module_put(THIS_MODULE);
	return 0;
}

module_init(init_numpipe);
module_exit(cleanup_numpipe);

MODULE_AUTHOR(DRIVER_AUTHOR);	/* Who wrote this module? */
MODULE_DESCRIPTION(DRIVER_DESC);	/* What does this module do */
