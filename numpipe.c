#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

MODULE_LICENSE("DUAL BSD/GPL");
#define DRIVER_AUTHOR "div4bing"
#define DRIVER_DESC   "Named Pipe for Exchanging Numbers"
#define DEVICE_NAME "numpipe"
#define SUCCESS 0
#define MAX_LEN 100

static DEFINE_SEMAPHORE(semMutual);
static DEFINE_SEMAPHORE(semEmpty);
static DEFINE_SEMAPHORE(semFull);

struct FifoQueue
{
  char **data;
  int head;
  int tail;
  int queueCount;
};

struct FifoQueue fifo;

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
static ssize_t device_read(struct file *filePtr, char __user *uBuffer, size_t sizeBuffer, loff_t *offsetBuff);
static ssize_t device_write(struct file *filePtr,const char *uBuffer,size_t sizeBuffer,loff_t *offsetBuff);
int dequeueFifo(char *dequeuData);
int IsQueueEmpty(void);
int IsQueueFull(void);

static struct file_operations fileOps = {
	.owner = THIS_MODULE,
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release
};

int dequeueFifo(char *dequeueData)
{
  if (IsQueueEmpty() == -1)
  {
    // printf("Error! Queue is Empty\n");
    return -1;
  }

	strcpy(dequeueData, fifo.data[fifo.head]);

  fifo.head++;

  if(fifo.head == buffer_size)
  {
    fifo.head = 0;
  }

  fifo.queueCount--;

  return 0;
}

int enqueueFifo(char *enqueueData)
{
  if(IsQueueFull() == 0)
  {
    if(fifo.tail == buffer_size-1)
    {
       fifo.tail = -1;    // Reset Tail
    }

		strcpy(fifo.data[++fifo.tail], enqueueData);
    fifo.queueCount++;
    return 0;
  }
  else
  {
    return -1;
  }
}

int IsQueueEmpty(void)
{
  if (fifo.queueCount == 0)
  {
    return -1;
  }

  return 0;
}

int IsQueueFull(void)
{
  if (fifo.queueCount == buffer_size)
  {
    return -1;
  }

  return 0;
}

static int __init init_numpipe(void)
{
	int i = 0;

	printk(KERN_INFO "%s: Welcome to Num Pipe! Passed FIFO size is-> %d\n", DEVICE_NAME, buffer_size);
	printk(KERN_INFO "%s: Registering char device\n", DEVICE_NAME);

	major_num = register_chrdev(0, DEVICE_NAME, &fileOps);

	if (major_num < 0)
	{
	  printk(KERN_ALERT "%s: Registering char device failed with %d\n", DEVICE_NAME, major_num);
	  return major_num;
	}

	printk(KERN_INFO "%s: Assigned Major number is: %d\n", DEVICE_NAME, major_num);
	printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, major_num);

	fifo.data = kmalloc(sizeof(char *) * buffer_size, GFP_KERNEL);						//  Assign enough FIFO

	if (fifo.data == NULL)
	{
		printk(KERN_ALERT "Failed to allocate enough FIFO\n");
		return -ENOMEM;
	}

	for (i = 0; i < buffer_size; i++)																					// Assign enough memory for each FIFO
	{
		fifo.data[i] = kmalloc(MAX_LEN, GFP_KERNEL);

		if (fifo.data[i] == NULL)
		{
			printk(KERN_ALERT "Failed to allocate enough memory for FIFO\n");
			return -ENOMEM;
		}
	}

	fifo.head = 0;			// Default values
  fifo.tail = -1;			// Default values

	sema_init(&semMutual, 1);		// Have a binary semaphore for Mutual Exclusion on shared resource
  sema_init(&semEmpty, 0);		// Have a binary semaphore for Empty FIFO
  sema_init(&semFull, buffer_size);		// Have a binary semaphore for Full FIFO

	return 0;
}

static void __exit cleanup_numpipe(void)
{
	printk(KERN_INFO "%s: Unregistering the char device\n", DEVICE_NAME);
	kfree(fifo.data);
	unregister_chrdev(major_num, DEVICE_NAME);
}

static int device_open(struct inode *inode, struct file *file)		// Module open function, keeps track of number of times the module is openned
{
	dev_open++;
	printk( KERN_INFO "%s: Oppend Module\n", DEVICE_NAME);

	return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file)
{
	dev_open--;														// Release module for next user
	return 0;
}

static ssize_t device_read(struct file *filePtr, char __user *uBuffer, size_t sizeBuffer, loff_t *offsetBuff)
{
	int ret = 0;
	char data[MAX_LEN];
	int str_size = 0, tempSize = 0;

  if (*offsetBuff > sizeBuffer)
  {
    return 0;
  }

	down_interruptible(&semEmpty);
  down_interruptible(&semMutual);

	if (dequeueFifo(&data[0]) == 0)		// Proceed only if FIFO is not empty
	{
		str_size = strlen(data);
		tempSize = str_size;

		printk(KERN_INFO "%s: Read requested, Buffer is: %s size is: %d\n", DEVICE_NAME, data, str_size);

		ret = copy_to_user(uBuffer, data, str_size);			// Copy the data to use space

		if (ret != 0)
		{
			 printk(KERN_INFO "%s: Failed to send %d characters to the user\n", DEVICE_NAME, ret);
       up(&semEmpty);
			 up(&semMutual);
			 return -EFAULT;
		}
	}
	str_size = 0;
	up(&semMutual);
  up(&semFull);

  *offsetBuff = sizeBuffer+1;

	return sizeBuffer;
}

static ssize_t device_write(struct file *filePtr,const char *uBuffer,size_t sizeBuffer,loff_t *offsetBuff)
{
	unsigned int ret;
	char data[MAX_LEN];

  down_interruptible(&semFull);
	down_interruptible(&semMutual);
	if(sizeBuffer > (MAX_LEN - 1))
	{
    up(&semFull);
		up(&semMutual);
		return -EINVAL;
	}

	ret = copy_from_user(data, uBuffer, sizeBuffer);

	if(ret)
	{
    up(&semFull);
		up(&semMutual);
		return -EFAULT;
	}

	data[sizeBuffer] = '\0';
	if (enqueueFifo(data) == -1)			// Check if FIFO is not full, and add the new data
  {
    up(&semFull);
		up(&semMutual);
    return -1;
  }

	printk(KERN_INFO "Write Performed:of size: %d, written size=%zu & Data: [%s] \n", (int) strlen(data), sizeBuffer, data);
	data[0] = '\0';		// NULL the data
	up(&semMutual);
  up(&semEmpty);

	return sizeBuffer;
}

module_init(init_numpipe);
module_exit(cleanup_numpipe);

MODULE_AUTHOR(DRIVER_AUTHOR);	/* Who wrote this module? */
MODULE_DESCRIPTION(DRIVER_DESC);	/* What does this module do */
