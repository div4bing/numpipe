#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

MODULE_LICENSE("DUAL BSD/GPL");
#define DRIVER_AUTHOR "Divyeshkumar Maisuria <dmaisur1@binghamton.edu>"
#define DRIVER_DESC   "Named Pipe for Exchanging Numbers"

static int buffer_size = 0;			// Buffer size for the FIFO

// Module Parameter information
module_param(buffer_size, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(buffer_size, "Size of the FIFO for Number Pipe");

static int __init init_numpipe(void)
{
	printk(KERN_INFO "Passed Parameter is: %d", buffer_size);
	return 0;
}

static void __exit cleanup_numpipe(void)
{
	printk(KERN_INFO "Goodbye!\n");
}

module_init(init_numpipe);
module_exit(cleanup_numpipe);

MODULE_AUTHOR(DRIVER_AUTHOR);	/* Who wrote this module? */
MODULE_DESCRIPTION(DRIVER_DESC);	/* What does this module do */
