#include "ums_dev.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/fs.h>

MODULE_AUTHOR("Bektur Umarbaev");
MODULE_DESCRIPTION("User Mode thread Scheduling (UMS)");
MODULE_LICENSE("GPL");

DEFINE_SPINLOCK(spinlock_ums);
unsigned long spinlock_flags_ums;

static long ioctl_ums(struct file *file, unsigned int cmd, unsigned long arg);

static const struct file_operations fops_ums = {
	.owner		    = THIS_MODULE,
	.unlocked_ioctl	= ioctl_ums,
    .compat_ioctl   = ioctl_ums
};

static struct miscdevice dev_ums = {
	.minor		= UMS_MINOR,
	.name		= "ums",
    .mode       = S_IALLUGO,
	.fops		= &fops_ums,
};

static long ioctl_ums(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;

    printk(KERN_INFO UMS_MODULE_NAME_LOG "> IOCTL_START: pid: %d, tgid: %d, IOCTL:%d\n", current->pid, current->tgid, cmd);
    spin_lock_irqsave(&spinlock_ums, spinlock_flags_ums);

    switch (cmd) {
        case UMS_ENTER:
            ret = enter_ums();
            goto out;
        case UMS_EXIT:
            ret = exit_ums();
            goto out;
        case UMS_CREATE_LIST:
            ret = create_completion_list();
            goto out;
        case UMS_CREATE_WORKER:
            ret = create_worker_thread((worker_params_t*)arg);
            goto out;
        case UMS_ENTER_SCHEDULING_MODE:
            ret = enter_scheduling_mode((scheduler_params_t*)arg);
            goto out;
        case UMS_EXIT_SCHEDULING_MODE:
            ret = exit_scheduling_mode();
            goto out;
        default:
            goto out;
	}

    out:
    printk(KERN_INFO UMS_MODULE_NAME_LOG "> IOCTL_END: pid: %d, tgid: %d, IOCTL:%d => return_value: %d\n",  current->pid, current->tgid, cmd, ret);
    spin_unlock_irqrestore(&spinlock_ums, spinlock_flags_ums);
	return ret;
}

static int __init init_dev(void)
{
    int ret;
    
    printk(KERN_INFO UMS_MODULE_NAME_LOG "> Initialization:\n");

    ret = misc_register(&dev_ums);
    if (ret < 0)
    {
        printk(KERN_ERR UMS_MODULE_NAME_LOG "-Registration of device " UMS_DEVICE " has failed.\n");
        return -UMS_ERROR;
    }

    printk(KERN_INFO UMS_MODULE_NAME_LOG "-Device " UMS_DEVICE " has been successfully registered.\n");

    return UMS_SUCCESS;
}

static void __exit exit_dev(void)
{
    misc_deregister(&dev_ums);
    printk(KERN_INFO UMS_MODULE_NAME_LOG "> Shut down.\n");
}


module_init(init_dev);
module_exit(exit_dev);