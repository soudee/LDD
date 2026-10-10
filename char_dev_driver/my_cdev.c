#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>

static const char *my_device = "my_cdev";
static int major;

static struct file_operations fops = {

};

static int __init my_init(void)
{
	major = register_chrdev(0, my_device, &fops);
	if (major < 0) {
		pr_err("%s: character device registration failed\n", my_device);
		return major;
	}

	pr_info("%s: character device registered successfully with major number %d\n", my_device, major);

	return 0;
}

static void __exit my_exit(void)
{
	unregister_chrdev(major, my_device);
	pr_info("%s: character device unregistered. Goodbye Kernel!\n", my_device);
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SOUD");
MODULE_DESCRIPTION("my first character device");
