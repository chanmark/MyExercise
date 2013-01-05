#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <asm/io.h>
#include <linux/ioctl.h>
#include "cdata_ioctl.h"
#include <asm/uaccess.h>

#ifdef CONFIG_SMP
#define __SMP__
#endif

#define	CDATA_MAJOR 121 
#define BUFSIZE	1024

#define LCD_WIDTH (240)
#define LCD_HEIGHT (320)
#define LCD_BPP (4)
#define LCD_SIZE (LCD_WIDTH*LCD_HEIGHT*LCD_BPP)

struct cdata_t {
	char data[BUFSIZE];
	int  index;
	int  offset;
	char *iomem;
	struct timer_list timer;
	wait_queue_head_t wait;
};

static DECLARE_MUTEX(cdata_sem); // Think why here use global variable
				 // Don't use private variable

static void flush_lcd(unsigned long priv)
{
	struct cdata_t *cdata = (struct cdata_t *)priv;
	char *fb = cdata->iomem;
	int index = cdata->index;
	int offset = cdata->offset;
	int i;

	fb += offset;
	for( i=0;i<index;i++){
		writeb(cdata->data[i], fb++);
		offset++;
		
		if(offset >= LCD_SIZE)
			offset = 0;
	}
	cdata->index = 0;
	cdata->offset = offset;

	//Wake up process
	// current->state = TASK_RUNNING;
	wake_up(&cdata->wait);
}

static int cdata_open(struct inode *inode, struct file *filp)
{
	int minor;
	struct cdata_t *cdata;
	minor = MINOR(inode->i_rdev);
	printk(KERN_ALERT "filp Address = %p\n", filp);
	
	cdata->index = 0;
	
	cdata = (struct cdata_t *)kmalloc(sizeof(struct cdata_t),GFP_KERNEL);
	init_waitqueue_head(&cdata->wait);
	filp->private_data = (void *)cdata; 
	cdata->iomem =  ioremap(0x33f00000, LCD_WIDTH*LCD_HEIGHT*LCD_BPP);
	init_timer(&cdata->timer);

	return 0;
}

static int cdata_ioctl(struct inode *inode, struct file *filp, 
			unsigned int cmd, unsigned long arg)
{
	struct cdata_t *cdata = (struct cdata_t *)filp->private_data;

	switch(cmd)
	{
		case CDATA_EMPTY:
			printk(KERN_ALERT "in ioctl: IOCTL_EMPTY");
			cdata->data[0] = '\0';
			cdata->index = 0;
			break;
		case CDATA_SYNC:
			printk(KERN_ALERT "Stream = %s",cdata->data);
			break;
		default:
			return EINVAL;
	}
			
	printk(KERN_ALERT "cdata: in cdata_ioctl()\n");
	return 0;
}

static ssize_t cdata_read(struct file *filp, char *buf, 
				size_t size, loff_t *off)
{
	printk(KERN_ALERT "cdata: in cdata_read()\n");
}

static ssize_t cdata_write(struct file *filp, const char *buf, 
				size_t count, loff_t *off)
{
	struct cdata_t *cdata = (struct cdata_t *)filp->private_data; // => container_of 2013/1/8
	DECLARE_WAITQUEUE(wait, current);
	int i;
	down(&cdata_sem);
	for( i=0; i < count; i++){
		if( cdata->index >= BUFSIZE){
					
			
			cdata->timer.expires = jiffies + 500;
			cdata->timer.data = (void *)cdata;
			cdata->timer.function = flush_lcd;
			add_timer(&cdata->timer);
			// perpare_to_wait 2013/1/8
			add_wait_queue(&cdata->wait, &wait);
			set_current_state(TASK_INTERRUPTIBLE); //TASK_UNINTERRUPTIBLE
			
			up(&cdata_sem);			
			schedule();
			down(&cdata_sem);
			current->state = TASK_RUNNING;
			remove_wait_queue(&cdata->wait, &wait);
		}

		if( copy_from_user(&cdata->data[cdata->index++], &buf[i], 1) )
			return -EFAULT;
	}
	// mutex unlock
	up(&cdata_sem);
	return 0;
}

static int cdata_release(struct inode *inode, struct file *filp)
{
	printk(KERN_ALERT "cdata: in cdata_release()\n");
	kfree(filp->private_data);
	return 0;
}

static int cdata_mmap(struct file *filp, struct vm_area_struct *vma)
{	
	unsigned long start = vma->vm_start;
	unsigned long end = vma->vm_end;
	unsigned long size;

	size = end - start;
	remap_page_range(start, 0x33f00000, size, PAGE_SHARED);
	
	return 0;
}

struct file_operations cdata_fops = {	
	owner:		THIS_MODULE,
	open:		cdata_open,
	release:	cdata_release,
	ioctl:		cdata_ioctl,
	read:		cdata_read,
	write:		cdata_write,
	mmap:		cdata_mmap,
};

struct miscdevice cdata_fops = {
	minor:	12,
	name:	"cdata",
	fops:	&cdata_fops,
};


int my_init_module(void)
{
	if( misc_register(&cdata_fops)){
		printk(KERN_ALERT "cdata: register failed\n");
		return -1;
	}

	printk(KERN_ALERT "cdata driver: registering...\n");

	return 0;
}

void my_cleanup_module(void)
{
	unregister_chrdev(CDATA_MAJOR, "cdata");
	printk(KERN_ALERT "cdata module: unregisterd.\n");
}

module_init(my_init_module);
module_exit(my_cleanup_module);

MODULE_LICENSE("GPL");
