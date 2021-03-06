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
#include <linux/slab.h>	/* for kmalloc() */

MODULE_LICENSE("GPL");

#define DEVICE_NAME	"tictactoe"
#define MAX_MINOR	1

/* Prototypes for device functions */
static ssize_t d_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t d_write(struct file *, const char __user *, size_t, loff_t *);
static int d_open(struct inode *, struct file *);
static int d_release(struct inode *, struct file *);

/* Helper function prototypes */


/* This structure holds the addresses of functions
*  that perform device operations.*/
static const struct file_operations fops = {
	.owner	= THIS_MODULE,
	.read	= d_read,
	.write	= d_write,
	.open	= d_open,
	.release = d_release
};

struct d_data {
	struct cdev cdev;
	/* Game state */
	int game_on;	/* Is a game in progress? */
	char board[9];	/* Array to store the state of the game board */
	char turn;	/* X/O */
	char player_token;
	char computer_token;
};

/* Global variables */
static int major = 0;
static struct d_data cdev_data[MAX_MINOR];
static struct class *cdev_class = NULL;

static int cdev_uevent(struct device *dev, struct kobj_uevent_env *env) {
	add_uevent_var(env, "DEVMODE=%#o", 0666);
	return 0;
}

/* Function definitions */
static ssize_t d_read(struct file *filp,
		char __user *buf, size_t len, loff_t *offset) {
	char *msg = "Welcome to kernel Tic-Tac-Toe!\n";
	size_t msg_len = strlen(msg);

	printk("Reading device: %d\n", MINOR(filp->f_path.dentry->d_inode->i_rdev));

	if (len > msg_len) {
		len = msg_len;
	}
	if (__copy_to_user(buf, msg, len)) {
		return -EFAULT;
	}
	return len;
}

static ssize_t d_write(struct file *filp, const char __user *buf,
		size_t len, loff_t *offset) {
	int d_num;
	d_num = MINOR(filp->f_path.dentry->d_inode->i_rdev);
	/*printk("Writing to device: %d\n", MINOR(filp->f_path.dentry->d_inode->i_rdev));*/
	printk("Writing to device: %d\n", d_num);

	char *msg;
	msg = NULL;
	msg = kmalloc(len * sizeof(*msg), GFP_KERNEL);
	size_t num_failed = __copy_from_user(msg, buf, len);

	if (num_failed == 0) {
		printk("Copied %zd bytes from user\n", len);
	}
	else {
		printk("Couldn't copy %zd bytes from user\n", num_failed);
	}

	/* Parse user input */
	/* Check if a newline character is present */
	int i;
	int n_ct;
	n_ct = 0;
	for (i = 0; i < len; ++i) {
		if (msg[i] == '\n') {
			n_ct = i;
		}
	}
	if (n_ct == 0) {
		printk("INVFMT\n"); /* Command not terminated by a newline */
		return len;
	}
	printk("Newline at %d\n", n_ct);
	msg[n_ct] = '\0';

	/* ADD A REALLOC */

	printk("Data from the user: %s\n", msg);

	/* Next, split string into tokens
	* (assuming at most 1 command and at most 2 arguments --
	* anything longer will be considered invalid) */
	char *cmd, *arg1, *arg2, *token;
	cmd = NULL, arg1 = NULL, arg2 = NULL;
	int count;
	count = 0;
	while ((token = strsep(&msg, " ")) != NULL) {
		if (count == 0) {
			cmd = token;
		}
		else if (count == 1) {
			arg1 = token;
		}
		else if (count == 2) {
			arg2 = token;
		}
		else {
			printk("INVFMT\n"); /* Too many arguments */
			return len;
		}
		++count;
	}

	printk("cmd: %s, arg1: %s, arg2: %s\n", cmd, arg1, arg2);

	/* 00 - begin a new game
	* takes 1 argument: user's choice of token (X/O) */
	if (strcmp(cmd, "00") == 0) {
		if (arg1 == NULL || arg2 != NULL) {
			printk("INVFMT\n"); /* Not enough arguments */
			return len;
		}
		if (strcmp(arg1, "X") == 0) {
			cdev_data[d_num].game_on = 1;
			cdev_data[d_num].player_token = 'X';
			cdev_data[d_num].computer_token = 'O';
			printk("User goes first -- OK\n");
			/* Set turn member variable and tokens*/
		}
		else if (strcmp(arg1, "O") == 0) {
			cdev_data[d_num].game_on = 1;
			cdev_data[d_num].player_token = 'O';
			cdev_data[d_num].computer_token = 'X';
			printk("Computer goes first -- OK\n");
			/* Set vars */
		}
		else {
			printk("INVFMT\n"); /* Invalid argument */
			return len;
		}
	}
	/* 01 - View current state of the board
	 * no arguments */
	else if (strcmp(cmd, "01") == 0) {
		if (arg1 != NULL || arg2 != NULL) {
			printk("INVFMT\n"); /* Too many arguments */
			return len;
		}
		/* Reply with the current state of the board */
		printk("Current state of the board: ");
		int k;
		for (k = 0; k < 9; ++k) {
			printk("%c", cdev_data[d_num].board[k]);
		}
		printk("\n");
	}
	/* 02 - User makes a move
	 * takes 2 parameters: column and row, both should be between 0 and 2 */
	else if (strcmp(cmd, "02") == 0) {
		if (arg1 == NULL || arg2 == NULL) {
			printk("INVFMT\n"); /* Not enough arguments */
			return len;
		}
		/* Verify that the game is started / not over */
		if (cdev_data[d_num].game_on == 0) {
			printk("NOGAME\n");
			return len;
		}
		/* Verify that it is player's turn */
		if (cdev_data[d_num].turn != cdev_data[d_num].player_token) {
			printk("OOT\n");
			return len;
		}
		/* Check if the move is valid */
		int column, row;
		column = arg1[0] - 48;
		row = arg2[0] - 48;
		if (column < 0 || column > 2 || row < 0 || row > 2) {
			printk("ILLMOVE\n");
			return len;
		}
		if (cdev_data[d_num].board[column + row * 3] == '*') {
			cdev_data[d_num].board[column + row * 3] = cdev_data[d_num].player_token;
			printk("User made a move at %d%d -- OK\n", column, row);
			cdev_data[d_num].turn = cdev_data[d_num].computer_token;
		}
		else {
			printk("ILLMOVE\n");
			return len;
		}
		/* Check for player win or tie */
		/* Return OK, WIN, TIE, OOT, or NOGAME */
	}
	/* 03 - Ask computer to make a move
	 * doesn't take any arguments */
	else if (strcmp(cmd, "03") == 0) {
		if (arg1 == NULL && arg2 == NULL) {
			if (cdev_data[d_num].game_on != 1) {
				printk("NOGAME\n");
				return len;
			}
			if (cdev_data[d_num].turn != cdev_data[d_num].computer_token) {
				printk("OOT\n");
				return len;
			}
			printk("Computer making a move -- OK\n");
			cdev_data[d_num].turn = cdev_data[d_num].player_token;
			/* Choose a move, check for win */
		}
		else {
			printk("INVFMT\n"); /* Too many arguments */
			return len;
		}
	}

	kfree(msg);
	return len;
}

static int d_open(struct inode *inode, struct file *file) {
	printk("tictactoe: Device open\n");
	return 0;
}

static int d_release(struct inode *inode, struct file *file) {
	printk("tictactoe: Device closed\n");
	return 0;
}

static int __init ttt_init(void) {
	/* Register the device */
	int error;
	dev_t dev;

	error = alloc_chrdev_region(&dev, 0, MAX_MINOR, DEVICE_NAME);
	major = MAJOR(dev);

	cdev_class = class_create(THIS_MODULE, DEVICE_NAME);
	cdev_class->dev_uevent = cdev_uevent;

	int i;
	for (i = 0; i < MAX_MINOR; ++i) {
		cdev_init(&cdev_data[i].cdev, &fops);
		cdev_data[i].cdev.owner = THIS_MODULE;
		cdev_add(&cdev_data[i].cdev, MKDEV(major, i), 1);
		device_create(cdev_class, NULL, MKDEV(major, i), NULL, "tictactoe-%d", i);
		/* Initialize the game board */
		int j;
		for (j = 0; j < 9; ++j) {
			cdev_data[i].board[j] = '*';
		}
		cdev_data[i].turn = 'X';	/* X goes first */
		cdev_data[i].game_on = 0;	/* Game not started yet */
		/* Don't know what the tokens will be yet. */
	}
	/*
	major = register_chrdev(0, DEVICE_NAME, &fops);
	if (major < 0) {
		printk(KERN_ALERT "Could not register device: %d\n", major);
		return major;
	}
	printk(KERN_INFO "tictactoe module loaded: %d\n", major);
	*/
	return 0;
}

static void __exit ttt_exit(void) {
	/* Clean up by unregistering the device */
	int i;
	for(i = 0; i < MAX_MINOR; ++i) {
		device_destroy(cdev_class, MKDEV(major, i));
	}

	class_unregister(cdev_class);
	class_destroy(cdev_class);

	unregister_chrdev_region(MKDEV(major, 0), MINORMASK);

	/*
	unregister_chrdev(major, DEVICE_NAME);
	printk(KERN_INFO "tictactoe unloaded\n");
	*/
}

module_init(ttt_init);
module_exit(ttt_exit);
