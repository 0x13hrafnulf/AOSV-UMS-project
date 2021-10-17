#include "ums_proc.h"



static struct proc_dir_entry *proc_ums;


/*
static struct proc_ops proc_fops = {
        .proc_open = open_proc,
        .proc_read = read_proc,
};
*/

int init_proc(void)
{
    printk(KERN_INFO UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG "> Initialization.\n");
    proc_ums = proc_mkdir(UMS_NAME, NULL);
    return UMS_SUCCESS;
}

int delete_proc(void)
{
    printk(KERN_INFO UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG "> Deleted.\n");
    proc_remove(proc_ums);
    return UMS_SUCCESS;
}