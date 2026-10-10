#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>

static const char *my_device = "my_cdev";
static dev_t dev_num;
static struct cdev my_cdev;
static struct class *my_class;
static struct file_operations fops = {

};

static int __init my_init(void)
{
	int status;
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
