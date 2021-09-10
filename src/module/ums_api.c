#include "ums_api.h"

process_list_t proc_list = {
    .list = LIST_HEAD_INIT(proc_list.list),
    .process_count = 0
};

int enter_ums(void)
{
    process_t *proc;
    printk(KERN_INFO UMS_MODULE_NAME_LOG "Invokation of enter_ums()\n");

    proc = check_if_process_exists(current->pid);
    if(proc != NULL)
    {
        return -UMS_ERROR_PROCESS_ALREADY_EXISTS;
    }

    proc = create_process_node(current->pid);

    return UMS_SUCCESS;
}

int exit_ums(void)
{
    process_t *proc;

    printk(KERN_INFO UMS_MODULE_NAME_LOG "Invokation of exit_ums()\n");

    proc = check_if_process_exists(current->pid);
    if(proc == NULL)
    {
        return -UMS_ERROR_PROCESS_NOT_FOUND;
    }

    delete_process(proc);

    return UMS_SUCCESS;
}

int delete_process(process_t *proc)
{
    
    list_del(&proc->list);
    proc_list.process_count--;
    kfree(proc);
 
    return UMS_SUCCESS;
}

process_t *create_process_node(pid_t pid)
{
    process_t *proc;

    proc = kmalloc(sizeof(process_t), GFP_KERNEL);
    list_add_tail(&(proc->list), &proc_list.list);

    proc_list.process_count++;
    proc->pid = current->pid;

    return proc;
}

process_t *check_if_process_exists(pid_t pid)
{
    process_t *proc = NULL;

    if(!list_empty(&proc_list.list))
    {
        process_t *temp = NULL;
        process_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &proc_list.list, list) 
        {
            if(temp->pid == pid)
            {
                proc = temp;
                break;
            }
        }
    }

    return proc;
}