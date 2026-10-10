#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/mutex.h>

static const char *my_device = "my_cdev";
static dev_t dev_num;
static struct cdev my_cdev;
static struct class *my_class;

/* Size of the device's internal buffer */
#define DEV_SIZE_BUFFER 64

/* Internal device buffer (kernel memory) and it's mutex */
static char *dev_buffer;
static struct mutex dev_mutex;


static int my_open(struct inode *pInode, struct file *pFile){
	pr_info("%s: my_open function called Major num: %d Minor num: %d\n",my_device, imajor(pInode), iminor(pInode));

	pr_info("%s: file->f_pos: %lld\n", my_device,pFile->f_pos);
	pr_info("%s: file->f_mode: 0x%x\n", my_device,pFile->f_mode);
	pr_info("%s: file->f_flags: 0x%x\n", my_device,pFile->f_flags);
	return 0;
}

static int my_release(struct inode *pInode, struct file *pFile){
	pr_info("%s: my_release function called\n",my_device);
	return 0;
}

/* read: copy data from kernel buffer -> user-space buffer*/
static ssize_t my_read(struct file *pFile, char __user *pUser_buffer, size_t length, loff_t *pOffset){
	size_t bytes_to_copy, not_copied, copied;

	if(mutex_lock_interruptible(&dev_mutex)){
		pr_err("%s: could not lock device during read\n", my_device);
		return -ERESTARTSYS;
	}

	pr_info("%s: read called: request=%zu, offset=%lld, \n", my_device, length, *pOffset);

	bytes_to_copy = ((length + *pOffset) > strlen(dev_buffer)) ? (strlen(dev_buffer) - *pOffset) : length;
	pr_info("%s: Read will copy=%zu bytes\n", my_device, bytes_to_copy);

	not_copied = copy_to_user(pUser_buffer, dev_buffer + *pOffset, bytes_to_copy);
	copied = bytes_to_copy - not_copied;
	*pOffset += copied;

	if(not_copied)
        pr_warn("%s: copy_to_user only copied %zu/%zu\n", my_device, copied, bytes_to_copy);

    pr_info("%s: Read done: return=%zu, new offset=%lld\n", my_device, copied, *pOffset);

	mutex_unlock(&dev_mutex);
	return (ssize_t)copied;

}

/* write: copy data from user-space buffer -> kernel buffer */
static ssize_t my_write(struct file *pFile, const char __user *pUser_buffer, size_t length, loff_t *pOffset){
	size_t bytes_to_copy, not_copied, copied;

	if(mutex_lock_interruptible(&dev_mutex)){
		pr_err("%s: could not lock device during write\n", my_device);
		return -ERESTARTSYS;
	}

	if(*pOffset >= DEV_SIZE_BUFFER){
		pr_warn("%s: write offset %lld is beyond device buffer size %d\n", my_device, *pOffset, DEV_SIZE_BUFFER);
		mutex_unlock(&dev_mutex);
		return -ENOSPC;
	}else if(length > DEV_SIZE_BUFFER - (size_t)*pOffset){
        bytes_to_copy = DEV_SIZE_BUFFER - (size_t)*pOffset;
        pr_info("%s: only %zu bytes left to write\n", my_device, bytes_to_copy);
    }else{
        bytes_to_copy = length;
        pr_info("%s: writing %zu bytes\n", my_device, bytes_to_copy);
    }

	pr_info("%s: write request=%zu, offset=%lld, will copy=%zu\n", my_device, length, *pOffset, bytes_to_copy);

	not_copied = copy_from_user(dev_buffer + *pOffset, pUser_buffer, bytes_to_copy);
	copied = bytes_to_copy - not_copied;
	if(not_copied){
        pr_warn("%s: copy_from_user: only copied %zu/%zu\n", my_device, copied, bytes_to_copy);
    }

	*pOffset += copied;
	
	pr_info("%s: write done: copied=%zu, new offset=%lld\n", my_device, copied, *pOffset);

    mutex_unlock(&dev_mutex);
	return (ssize_t)copied;
}

static struct file_operations fops = {

	.open = my_open,
	.release = my_release,
	.read = my_read,
	.write = my_write,

};

static int __init my_init(void)
{
	int status;

    /* allocate kernel buffer */
	dev_buffer = kmalloc(DEV_SIZE_BUFFER, GFP_KERNEL);
	if(!dev_buffer){
		pr_err("%s: Failed to allocate device buffer\n", my_device);
		return -ENOMEM;
	}

	/* initialize mutex */
	mutex_init(&dev_mutex);

	/* clear dev_buffer */
	memset(dev_buffer, 0, DEV_SIZE_BUFFER);


	status = alloc_chrdev_region(&dev_num, 0, MINORMASK+1, my_device);
	if (status < 0) {
		pr_err("%s: character device registration failed\n", my_device);
		return status;
	}

	cdev_init(&my_cdev, &fops);
	my_cdev.owner = THIS_MODULE;

	status = cdev_add(&my_cdev, dev_num, MINORMASK+1);
	if(status){
		pr_err("%s: error adding cdev to kernel device list\n", my_device);
		goto free_dev_num;
	}

	my_class = class_create("my_class");
	if(!my_class){
		pr_err("%s: could not create class my_class\n",my_device);
		status = ENOMEM;
		goto delete_cdev;

	}
	if(!device_create(my_class, NULL, dev_num, NULL, "my_cdev%d", 0)){
		pr_err("%s: could not create device my_cdev0\n", my_device);
		status = ENOMEM;
		goto delete_class;

	}

	pr_info("%s: character device registered successfully with major number %d and minor number %d\n", my_device, MAJOR(dev_num), MINOR(dev_num));
	pr_info("%s: Created device number under /sys/class/my_class\n", my_device);
    pr_info("%s: Created new device node /dev/my_cdev\n", my_device);
	return 0;

delete_class:
	class_destroy(my_class);

delete_cdev:
	cdev_del(&my_cdev);

free_dev_num:
	unregister_chrdev_region(dev_num, MINORMASK+1);
	return status;

}

static void __exit my_exit(void)
{
	device_destroy(my_class, dev_num);
	class_destroy(my_class);
	cdev_del(&my_cdev);
	unregister_chrdev_region(dev_num, MINORMASK+1);
	pr_info("%s: character device unregistered. Goodbye Kernel!\n", my_device);
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SOUD");
MODULE_DESCRIPTION("my first character device");
