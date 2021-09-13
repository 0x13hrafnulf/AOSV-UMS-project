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

    worker_list_t *work_list;
    work_list = kmalloc(sizeof(worker_list_t), GFP_KERNEL);
    proc->worker_list = work_list;
    INIT_LIST_HEAD(&work_list->list);
    work_list->worker_count = 0;

    //TBA scheduler list

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
    
    worker_list_t *idle_list;
    idle_list = kmalloc(sizeof(worker_list_t), GFP_KERNEL);
    comp_list->idle_list = idle_list;
    INIT_LIST_HEAD(&comp_list->idle_list->list);
    idle_list->worker_count = 0;

    worker_list_t *busy_list;
    busy_list = kmalloc(sizeof(worker_list_t), GFP_KERNEL);
    comp_list->busy_list = busy_list;
    INIT_LIST_HEAD(&comp_list->busy_list->list);
    busy_list->worker_count = 0;

    list_id = comp_list->clid;
    return list_id;
}

ums_wid_t create_worker_thread(worker_params_t *params)
{
    printk(KERN_INFO UMS_MODULE_NAME_LOG "Called create_worker_thread()\n");

    ums_wid_t worker_id;
    worker_t *worker;
    process_t *process;
    completion_list_node_t *comp_list;
    worker_params_t kern_params;

    process = check_if_process_exists(current->pid);
    if(process == NULL)
    {
        return -UMS_ERROR_PROCESS_NOT_FOUND;
    }

    int err = copy_from_user(&kern_params, params, sizeof(worker_params_t));
    if(err != 0)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG "Error: create_worker_thread() => copy_from_user failed to copy %d bytes\n", err);
        return err;
    }

    comp_list = check_if_completion_list_exists(process, kern_params.clid);
    if(comp_list == NULL)
    {
        return -UMS_ERROR_COMPLETION_LIST_NOT_FOUND;
    }

    worker = kmalloc(sizeof(worker_t), GFP_KERNEL);
    list_add_tail(&(worker->global_list), &process->worker_list->list);

    worker->wid = process->worker_list->worker_count;
    worker->pid = -1;
    worker->tid = current->tgid;
    worker->sid = -1;
    worker->state = IDLE;
    worker->entry_point = kern_params.entry_point;
    worker->stack_addr = kern_params.stack_addr;

    memcpy(&worker->regs, task_pt_regs(current), sizeof(struct pt_regs));
    memset(&worker->fpu_regs, 0, sizeof(struct fpu));
    copy_fxregs_to_kernel(&worker->fpu_regs);

    worker->regs.ip = kern_params.entry_point;
    worker->regs.di = kern_params.function_args;
    worker->regs.sp = kern_params.stack_addr;
    worker->regs.bp = kern_params.stack_addr;

    list_add_tail(&(worker->local_list), &comp_list->idle_list->list);
    comp_list->idle_list->worker_count++;
    comp_list->worker_count++;
    process->worker_list->worker_count++;

    worker_id = worker->wid;
    return worker_id;
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

completion_list_node_t *check_if_completion_list_exists(process_t *proc, ums_clid_t clid)
{
    completion_list_node_t *comp_list;

    if(!list_empty(&proc->completion_lists->list))
    {
        completion_list_node_t *temp = NULL;
        completion_list_node_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &proc->completion_lists->list, list) 
        {
            if(temp->clid == clid)
            {
                comp_list = temp;
                break;
            }
        }
    }
  
    return comp_list;
}

int delete_process(process_t *proc)
{
    delete_completion_lists_and_worker_threads(proc);
    //delete_schedulers(proc);
    list_del(&proc->list);
    proc_list.process_count--;
    kfree(proc);
 
    return UMS_SUCCESS;
}

int delete_completion_lists_and_worker_threads(process_t *proc)
{
    if(!list_empty(&proc->completion_lists->list))
    {
        completion_list_node_t *temp = NULL;
        completion_list_node_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &proc->completion_lists->list, list) 
        {
            printk(KERN_INFO UMS_MODULE_NAME_LOG " %d completion list was deleted.\n", temp->clid);

            if(temp->idle_list->worker_count > 0) delete_workers_from_completion_list(temp->idle_list);
            if(temp->busy_list->worker_count > 0) delete_workers_from_completion_list(temp->busy_list);
            kfree(temp->idle_list);
            kfree(temp->busy_list);
            list_del(&temp->list);
            kfree(temp);
        }
    }
    list_del(&proc->completion_lists->list);
    kfree(proc->completion_lists);
    delete_workers_from_process_list(proc->worker_list);
    kfree(proc->worker_list);
    return UMS_SUCCESS;
}

int delete_workers_from_completion_list(worker_list_t *worker_list)
{
    if(!list_empty(&worker_list->list))
    {
        worker_t *temp = NULL;
        worker_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &worker_list->list, local_list) 
        {
            printk(KERN_INFO UMS_MODULE_NAME_LOG " %d worker thread was deleted.\n", temp->wid);

            list_del(&temp->local_list);
            list_del(&temp->global_list);
            kfree(temp);
        }
    }
    return UMS_SUCCESS;
}

//Might delete this function in future, have it just in case
int delete_workers_from_process_list(worker_list_t *worker_list)
{
    if(!list_empty(&worker_list->list))
    {
        worker_t *temp = NULL;
        worker_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &worker_list->list, global_list) 
        {
            printk(KERN_INFO UMS_MODULE_NAME_LOG " %d worker thread was deleted.\n", temp->wid);

            list_del(&temp->global_list);
            kfree(temp);
        }
    }
    return UMS_SUCCESS;
}