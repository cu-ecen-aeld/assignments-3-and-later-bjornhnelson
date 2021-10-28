/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Bjorn Nelson"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
	struct aesd_dev* dev_ptr;
	PDEBUG("open");
	/**
	 * TODO: handle open
	 */

	dev_ptr = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = dev_ptr;
	return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
	PDEBUG("release");
	/**
	 * TODO: handle release
	 */
	return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	ssize_t retval = 0;
	struct aesd_dev* dev_ptr = (struct aesd_dev*)(filp->private_data);
	size_t buf_index = 0;
	size_t bytes_left = count;
	size_t bytes_read;
	size_t bytes_missing;
	struct aesd_buffer_entry* cur_entry;
	size_t entry_offset;

	PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
	/**
	 * TODO: handle read
	 */

	if (mutex_lock_interruptible(&dev_ptr->lock)) {
		return -ERESTARTSYS;
	}

	do {
		cur_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev_ptr->queue, *f_pos, &entry_offset);

		if (cur_entry == NULL) {
			goto exit;
		}

		bytes_read = cur_entry->size - entry_offset;
        if (count < bytes_read) {
            bytes_read = count;
        }

		bytes_missing = copy_to_user(&buf[buf_index], &cur_entry->buffptr[entry_offset], bytes_read);
		if (bytes_missing != 0) {
			PDEBUG("Error: not all bytes copied in copy_to_user\n");
		}
		bytes_left -= (bytes_read - bytes_missing);
		*f_pos += (bytes_read - bytes_missing);
		buf_index += (bytes_read - bytes_missing);
		retval += (bytes_read - bytes_missing);
	} while (bytes_left > 0);

 exit:
	mutex_unlock(&dev_ptr->lock);
	return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct aesd_dev* dev_ptr = (struct aesd_dev*)(filp->private_data);
	size_t bytes_missed;
	char* newline_status;
	const char* overwrite_status;
	ssize_t retval = -ENOMEM;
	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	/**
	 * TODO: handle write
	 */
	
    if (mutex_lock_interruptible(&dev_ptr->lock)) {
		return -ERESTARTSYS;
	}

	if (dev_ptr->entry.size == 0) {
		dev_ptr->entry.buffptr = kmalloc(count, GFP_KERNEL);
	}
	else {
		dev_ptr->entry.buffptr = krealloc(dev_ptr->entry.buffptr, dev_ptr->entry.size + count, GFP_KERNEL);
	}
	if (dev_ptr->entry.buffptr == NULL) {
		goto exit;
	}

    bytes_missed = copy_from_user((void *)(&dev_ptr->entry.buffptr[dev_ptr->entry.size]), buf, count);
	if (bytes_missed != 0) {
		printk("Bad copy_from_user in aesd_write\n");
	}
	retval = count - bytes_missed;
	dev_ptr->entry.size += retval;

	newline_status = (char*) memchr(dev_ptr->entry.buffptr, '\n', dev_ptr->entry.size);

	if (newline_status != NULL) {
		overwrite_status = aesd_circular_buffer_add_entry(&dev_ptr->queue, &dev_ptr->entry);
		if (overwrite_status) {
			kfree(overwrite_status);
		}
		dev_ptr->entry.buffptr = NULL;
		dev_ptr->entry.size = 0;
	}

 exit:
    mutex_unlock(&dev_ptr->lock);
	return retval;
}

struct file_operations aesd_fops = {
	.owner =    THIS_MODULE,
	.read =     aesd_read,
	.write =    aesd_write,
	.open =     aesd_open,
	.release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
	int err, devno = MKDEV(aesd_major, aesd_minor);

	cdev_init(&dev->cdev, &aesd_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &aesd_fops;
	err = cdev_add (&dev->cdev, devno, 1);
	if (err) {
		printk(KERN_ERR "Error %d adding aesd cdev", err);
	}
	return err;
}



int aesd_init_module(void)
{
	dev_t dev = 0;
	int result;
	result = alloc_chrdev_region(&dev, aesd_minor, 1,
			"aesdchar");
	aesd_major = MAJOR(dev);
	if (result < 0) {
		printk(KERN_WARNING "Can't get major %d\n", aesd_major);
		return result;
	}
	memset(&aesd_device,0,sizeof(struct aesd_dev));

	/**
	 * TODO: initialize the AESD specific portion of the device
	 */

	mutex_init(&aesd_device.lock);
	aesd_circular_buffer_init(&aesd_device.queue);

	result = aesd_setup_cdev(&aesd_device);

	if( result ) {
		unregister_chrdev_region(dev, 1);
	}

	return result;

}

void aesd_cleanup_module(void)
{
	dev_t devno = MKDEV(aesd_major, aesd_minor);

	cdev_del(&aesd_device.cdev);

	/**
	 * TODO: cleanup AESD specific poritions here as necessary
	 */

	unregister_chrdev_region(devno, 1);

	aesd_circular_buffer_free(&aesd_device.queue);
}


module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
