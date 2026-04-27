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
#include "aesdchar.h"
#include "aesd_ioctl.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Your Name Here"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);
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
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
     
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *entry;
    size_t offset = 0;
    
    if (mutex_lock_interruptible(&dev->lock)) 
    	{
    	return -ERESTARTSYS;
    	}
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &offset);
    if (entry) {
        size_t remain = entry->size - offset;
        size_t read = min(count, remain);

        if (copy_to_user(buf, entry->buffptr + offset, read)) {
            retval = -EFAULT;
        } else {
            retval = read;
            *f_pos += retval;
        }
    }

    mutex_unlock(&dev->lock);
    
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
     
    struct aesd_dev *dev = filp->private_data;

    if (mutex_lock_interruptible(&dev->lock)){
        return -ERESTARTSYS;
        }

    dev->entry.buffptr = krealloc(dev->entry.buffptr, dev->entry.size + count, GFP_KERNEL);
    
    if (!dev->entry.buffptr) {
        mutex_unlock(&dev->lock);
        return -ENOMEM;
    }

    if (copy_from_user((char *)dev->entry.buffptr + dev->entry.size, buf, count)) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }
    retval = count;
    dev->entry.size += count;

    if (dev->entry.buffptr[dev->entry.size - 1] == '\n') {
      if (dev->buffer.full) {
            kfree(dev->buffer.entry[dev->buffer.in_offs].buffptr);
        }


        aesd_circular_buffer_add_entry(&dev->buffer, &dev->entry);
        dev->entry.buffptr = NULL;
        dev->entry.size = 0;
    }

    mutex_unlock(&dev->lock);
    
    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
    struct aesd_dev *dev = filp->private_data;
    loff_t retval;
    size_t total_size = 0;
    uint8_t index;
    struct aesd_buffer_entry *entry;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;


    AESD_CIRCULAR_BUFFER_FOREACH(entry, &dev->buffer, index) {
        total_size += entry->size;
    }

    switch (whence) {
      case SEEK_SET:
        retval = off;
        break;
      case SEEK_CUR:
        retval = filp->f_pos + off;
        break;
      case SEEK_END:
        retval = total_size + off;
        break;
      default:
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    if (retval < 0 || retval > total_size) {
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    filp->f_pos = retval;
    mutex_unlock(&dev->lock);
    return retval;
}

long aesd_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct aesd_seekto seekto;
    struct aesd_dev *dev = filp->private_data;
    size_t new_fpos = 0;
    int i;
    uint8_t index;

    if (cmd != AESDCHAR_IOCSEEKTO)
        return -ENOTTY;

    if (copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)))
        return -EFAULT;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    int num_entries = 0;
    if (dev->buffer.full) {
        num_entries = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    } else {
        num_entries = (dev->buffer.in_offs - dev->buffer.out_offs + AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    index = (dev->buffer.out_offs + seekto.write_cmd) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    if (seekto.write_cmd >= num_entries || seekto.write_cmd_offset >= dev->buffer.entry[index].size) {
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    for (i = 0; i < seekto.write_cmd; i++) {
        new_fpos += dev->buffer.entry[(dev->buffer.out_offs + i) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].size;
    }
    
    filp->f_pos = new_fpos + seekto.write_cmd_offset;

    mutex_unlock(&dev->lock);
    return 0;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek = 	aesd_llseek,
    .unlocked_ioctl= aesd_unlocked_ioctl,
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
    aesd_circular_buffer_init(&aesd_device.buffer);

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
    uint8_t index;
    struct aesd_buffer_entry *entry;
     AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, index) {
        if (entry->buffptr) {
            kfree(entry->buffptr);
        }
    }
    if (aesd_device.entry.buffptr) {
        kfree(aesd_device.entry.buffptr);
    }
    

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
