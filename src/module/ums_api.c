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
process_t *create_process_node(pid_t pid)
{
    process_t *proc;

    proc = kmalloc(sizeof(process_t), GFP_KERNEL);
    list_add_tail(&(proc->list), &proc_list.list);

    proc_list.process_count++;
    proc->pid = current->pid;

    completion_list_t *comp_lists;
    comp_lists = kmalloc(sizeof(completion_list_t), GFP_KERNEL);
    proc->completion_lists = comp_lists;
    INIT_LIST_HEAD(&comp_lists->list);
    comp_lists->list_count = 0;


    return proc;
}

ums_clid_t create_completion_list()
{
    printk(KERN_INFO UMS_MODULE_NAME_LOG "Called create_completion_list()\n");

    ums_clid_t list_id;
    process_t *proc;
    completion_list_node_t *comp_list;

    proc = check_if_process_exists(current->pid);
    if(proc == NULL)
    {
        return -UMS_ERROR_PROCESS_NOT_FOUND;
    }
    
    comp_list = kmalloc(sizeof(completion_list_node_t), GFP_KERNEL);
    list_add_tail(&(comp_list->list), &proc->completion_lists->list);

    comp_list->clid = proc->completion_lists->list_count;
    proc->completion_lists->list_count++;
    comp_list->worker_count = 0;
    comp_list->finished_count = 0;
    //TBA worker lists

    list_id = comp_list->clid;
    return list_id;
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

int delete_process(process_t *proc)
{
    delete_completion_lists(proc);
    list_del(&proc->list);
    proc_list.process_count--;
    kfree(proc);
 
    return UMS_SUCCESS;
}

int delete_completion_lists(process_t *proc)
{
    if(!list_empty(&proc->completion_lists->list))
    {
        completion_list_node_t *temp = NULL;
        completion_list_node_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &proc->completion_lists->list, list) 
        {
            //NEED TO DELETE WORKERS FROM LIST ALSO IN FUTURE
            printk(KERN_INFO UMS_MODULE_NAME_LOG " %d completion list was deleted.\n", temp->clid);
            list_del(&temp->list);
            kfree(temp);
        }
    }
    list_del(&proc->completion_lists->list);
    kfree(proc->completion_lists);
    return UMS_SUCCESS;
}