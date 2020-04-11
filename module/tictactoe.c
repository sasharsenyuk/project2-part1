/* Tic-Tac-Toe Loadable Kernel Module
File:		tictactoe.c
Author:		Sasha Arsenyuk
Course:		CMSC 421, Section 2
Term:		Spring 2020
Instructor:	Lawrence Sebald
*/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>

MODULE_LICENSE("GPL");

#define DEVICE_NAME "tictactoe"

/* Prototypes for device functions */
static ssize_t d_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t d_write(struct file *, const char __user *, size_t, loff_t *);
static int d_open(struct inode *, struct file *);
static int d_release(struct inode *, struct file *);

/* Helper function prototypes */

/* Global variables */
static int major;

/* This structure holds the addresses of functions
*  that perform device operations.*/
static const struct file_operations fops = {
	.read = d_read,
	.write = d_write,
	.open = d_open,
	.release = d_release
};

static ssize_t d_read(struct file *filp,
		char __user *buf, size_t len, loff_t *offset) {
	char *msg = "Welcome to kernel Tic-Tac-Toe!\n";
	size_t msg_len = strlen(msg);

	if (len > msg_len) {
		len = msg_len;
	}
	if (copy_to_user(buf, msg, len)) {
		return -EFAULT;
	}
	return len;
}

static ssize_t d_write(struct file *filp, const char __user *buf,
		size_t len, loff_t *offset) {
	size_t max_len = 8;

	if (len < max_len) {
		max_len = len;
	}

	char msg[max_len];
	size_t num_failed = copy_from_user(msg, buf, max_len);
	if (num_failed == 0) {
		printk("Copied %zd bytes from user\n", max_len);
	}
	else {
		printk("Couldn't copy %zd bytes from user\n", num_failed);
	}

	msg[max_len] = 0;
	printk("Data from the user: %s\n", msg);

	return max_len;
}

static int d_open(struct inode *inode, struct file *file) {
	return 0;
}

static int d_release(struct inode *inode, struct file *file) {
	return 0;
}

static int __init ttt_init(void) {
	/* Register the device */
	major = register_chrdev(0, DEVICE_NAME, &fops);
	if (major < 0) {
		printk(KERN_ALERT "Could not register device: %d\n", major);
		return major;
	}
	printk(KERN_INFO "tictactoe module loaded: %d\n", major);
	return 0;
}

static void __exit ttt_exit(void) {
	/* Clean up by unregistering the device */
	unregister_chrdev(major, DEVICE_NAME);
	printk(KERN_INFO "tictactoe unloaded\n");
}

module_init(ttt_init);
module_exit(ttt_exit);
