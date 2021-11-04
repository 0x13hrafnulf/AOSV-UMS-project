/**
 * Copyright (C) 2021 Bektur Umarbaev <hrafnulf13@gmail.com>
 *
 * This file is part of the User Mode thread Scheduling (UMS) kernel module.
 *
 * UMS kernel module is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * UMS kernel module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with UMS kernel module.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * @brief Contains implementations of the UMS miscdevice
 *
 * @file ums_dev.c
 * @author Bektur Umarbaev <hrafnulf13@gmail.com>
 * @date 
 */
#include "ums_dev.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/fs.h>

MODULE_AUTHOR("Bektur Umarbaev");
MODULE_DESCRIPTION("User Mode thread Scheduling (UMS)");
MODULE_LICENSE("GPL");

/*
 * Global variables
 */
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

/** @brief The function that is responsible for ioctl calls
 *.
 *
 *  @param file
 *  @param cmd command number
 *  @param arg pointer to arguments, if any
 *  @return can return @ref UMS_SUCCESS, IDs of the completion lists, worker threads, schedulers depending on the syscall or error value in case of the failure 
 */
static long ioctl_ums(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;

    spin_lock_irqsave(&spinlock_ums, spinlock_flags_ums);

    printk(KERN_INFO UMS_MODULE_NAME_LOG "> IOCTL_START: pid: %d, tgid: %d, IOCTL:%d\n", current->pid, current->tgid, cmd);
    
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
        case UMS_EXECUTE_THREAD:
			ret = execute_thread((ums_wid_t)arg);
            goto out;
        case UMS_THREAD_YIELD:
            ret = thread_yield((worker_status_t)arg);
            goto out;
        case UMS_DEQUEUE_COMPLETION_LIST_ITEMS:
            ret = dequeue_completion_list_items((list_params_t*)arg);
            goto out;
        default:
            goto out;
	}

    out:
    printk(KERN_INFO UMS_MODULE_NAME_LOG "> IOCTL_END: pid: %d, tgid: %d, IOCTL:%d => return_value: %d\n",  current->pid, current->tgid, cmd, ret);
    printk(KERN_INFO UMS_MODULE_NAME_LOG ">-----------------------------------------------------------\n",  current->pid, current->tgid, cmd, ret);
    spin_unlock_irqrestore(&spinlock_ums, spinlock_flags_ums);

	return ret;
}

/** @brief The function is responsible for initialization of the kernel module
 *.
 * 
 * @return returns @ref UMS_SUCCESS when succesful or @ref UMS_ERROR if there are any errors  
 */
static int __init init_dev(void)
{
    int ret;
    
    printk(KERN_INFO UMS_MODULE_NAME_LOG "> Initialization:\n");

    ret = misc_register(&dev_ums);
    if (ret < 0)
    {
        printk(KERN_ERR UMS_MODULE_NAME_LOG "- Registration of device " UMS_DEVICE " has failed.\n");
        return -UMS_ERROR;
    }

    printk(KERN_INFO UMS_MODULE_NAME_LOG "- Device " UMS_DEVICE " has been successfully registered.\n");

    ret = init_proc();
    if (ret < 0)
    {
        printk(KERN_ERR UMS_MODULE_NAME_LOG "- Registration of proc filesystem of /proc/" UMS_NAME " has failed.\n");
        return -UMS_ERROR;
    }

    return UMS_SUCCESS;
}

/** @brief The function is manaages kernel module used resources before exit
 *.
 *  The function performs a cleanup of the memory and deregister devices and filesystems used (miscdevice and proc filesystem)
 *
 */
static void __exit exit_dev(void)
{
    delete_proc();
    misc_deregister(&dev_ums);
    cleanup();
    printk(KERN_INFO UMS_MODULE_NAME_LOG "> Shut down.\n");
}


module_init(init_dev);
module_exit(exit_dev);