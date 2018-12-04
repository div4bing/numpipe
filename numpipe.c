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

struct FifoQueue fifo;      // Instance of Queue

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

int IsQueueEmpty(void)
{
  if (fifo.queueCount == 0)
  {
    return -1;
  }

  return SUCCESS;
}

int IsQueueFull(void)
{
  if (fifo.queueCount == buffer_size)
  {
    return -1;
  }

  return SUCCESS;
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
	printk(KERN_INFO "%s: 'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, DEVICE_NAME, major_num);

	fifo.data = kmalloc(sizeof(char *) * buffer_size, GFP_KERNEL);						//  Assign enough FIFO

	if (fifo.data == NULL)
	{
		printk(KERN_ALERT "%s: Failed to allocate enough FIFO\n", DEVICE_NAME);
		return -ENOMEM;
	}

	for (i = 0; i < buffer_size; i++)																					// Assign enough memory for each FIFO
	{
		fifo.data[i] = kmalloc(MAX_LEN, GFP_KERNEL);

		if (fifo.data[i] == NULL)
		{
			printk(KERN_ALERT "%s: Failed to allocate enough memory for FIFO\n", DEVICE_NAME);
			return -ENOMEM;
		}
	}

	fifo.head = 0;			// Default values
  fifo.tail = -1;			// Default values

	sema_init(&semMutual, 1);		// Have a binary semaphore for Mutual Exclusion on shared resource
  sema_init(&semEmpty, 0);		// Have a binary semaphore for Empty FIFO
  sema_init(&semFull, buffer_size);		// Have a binary semaphore for Full FIFO

	return SUCCESS;
}

static void __exit cleanup_numpipe(void)
{
  int i = 0;
	printk(KERN_INFO "%s: Unregistering the char device\n", DEVICE_NAME);
  for (i=0; i < buffer_size; i++)           // Free-up the allocated memory
  {
	   kfree(fifo.data[i]);
  }
	unregister_chrdev(major_num, DEVICE_NAME);   // Unregister the character device
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
	return SUCCESS;
}

static ssize_t device_read(struct file *filePtr, char __user *uBuffer, size_t sizeBuffer, loff_t *offsetBuff)
{
	int ret = 0;

	if (down_interruptible(&semEmpty) < 0)   // To handle Ctl+C from userspace process when FIFO is empty
  {
    return SUCCESS;
  }

  down_interruptible(&semMutual);          // Down on mutual Exclusion semaphore

  if (IsQueueEmpty() == -1)                // Is the queue empty
  {
    printk(KERN_INFO "%s: Queue is Empty\n", DEVICE_NAME);
  }

  printk(KERN_INFO "%s: Device Read Request of Size=%d\n", DEVICE_NAME, (int)sizeBuffer);

  ret = copy_to_user(uBuffer, fifo.data[fifo.head], sizeBuffer);			// Copy the data to use space

  if (ret != 0)                           // Failed to copy the data
  {
     printk(KERN_INFO "%s: Failed to send %d characters to the user\n", DEVICE_NAME, ret);
     up(&semEmpty);
     up(&semMutual);
     return -EFAULT;
  }

  // Handle our queue
  fifo.head++;

  if(fifo.head == buffer_size)
  {
    fifo.head = 0;
  }

  fifo.queueCount--;

	up(&semMutual);              // Release mutual Exclusion semaphore
  up(&semFull);                // UP full semaphore

	return sizeBuffer;
}

static ssize_t device_write(struct file *filePtr,const char *uBuffer,size_t sizeBuffer,loff_t *offsetBuff)
{
	unsigned int ret;

  if (down_interruptible(&semFull) < 0)     // To hande Ctl+C on userspace process when FIFO is full
  {
    return SUCCESS;
  }

	down_interruptible(&semMutual);           // Acquire lock to the mutual Exclusion semaphore

	if(sizeBuffer > (MAX_LEN - 1))            // Requested size is more than what one can hold
	{
    up(&semFull);
		up(&semMutual);
		return -EINVAL;
	}

  if(IsQueueFull() == SUCCESS)              // Check if FIFO is full
  {
    if(fifo.tail == buffer_size-1)          // Handle out Queue
    {
       fifo.tail = -1;    // Reset Tail
    }

    ret = copy_from_user(fifo.data[++fifo.tail], uBuffer, sizeBuffer);      // Copy data passed from user

  	if(ret)                                                                 // Check for failure that some bytes could not be copied
  	{
      up(&semFull);
  		up(&semMutual);
  		return -EFAULT;
  	}

    fifo.queueCount++;
  }
  else                                    // FIFO is full
  {
    up(&semFull);
		up(&semMutual);
    return -1;
  }

	up(&semMutual);                         // Release lock
  up(&semEmpty);                          // Up Empty semaphore

	return sizeBuffer;                      // Return size copied to userspace
}

module_init(init_numpipe);
module_exit(cleanup_numpipe);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
