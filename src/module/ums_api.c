#include "ums_api.h"

static struct proc_dir_entry *proc_ums;
process_list_t process_list = {
    .list = LIST_HEAD_INIT(process_list.list),
};

int enter_ums(void)
{
    printk(KERN_INFO UMS_MODULE_NAME_LOG "-- Invocation of enter_ums()\n");
    
    process_t *process;

    process = check_if_process_exists(current->pid);
    if(process != NULL)
    {
        return -UMS_ERROR_PROCESS_ALREADY_EXISTS;
    }

    process = create_process_node(current->pid);
    
    int ret = create_process_proc_entry(process);
    if(ret != 0)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG "--- Error: enter_ums() => %d\n", ret);
        return ret;
    }

    return UMS_SUCCESS;
}

int exit_ums(void)
{
    printk(KERN_INFO UMS_MODULE_NAME_LOG "-- Invocation of exit_ums()\n");

    process_t *process;

    process = check_if_process_exists(current->pid);
    if(process == NULL)
    {
        return -UMS_ERROR_CMD_IS_NOT_ISSUED_BY_MAIN_THREAD;
    }

    process->state = FINISHED;
    //delete_process(process);
    
    return UMS_SUCCESS;
}

process_t *create_process_node(pid_t pid)
{
    process_t *process;

    process = kmalloc(sizeof(process_t), GFP_KERNEL);
    list_add_tail(&process->list, &process_list.list);

    process_list.process_count++;
    process->pid = current->pid;

    completion_list_t *comp_lists;
    comp_lists = kmalloc(sizeof(completion_list_t), GFP_KERNEL);
    process->completion_lists = comp_lists;
    INIT_LIST_HEAD(&comp_lists->list);
    comp_lists->list_count = 0;

    worker_list_t *work_list;
    work_list = kmalloc(sizeof(worker_list_t), GFP_KERNEL);
    process->worker_list = work_list;
    INIT_LIST_HEAD(&work_list->list);
    work_list->worker_count = 0;

    scheduler_list_t *sched_list;
    sched_list = kmalloc(sizeof(sched_list), GFP_KERNEL);
    process->scheduler_list = sched_list;
    INIT_LIST_HEAD(&sched_list->list);
    sched_list->scheduler_count = 0;

    

    return process;
}

ums_clid_t create_completion_list()
{
    printk(KERN_INFO UMS_MODULE_NAME_LOG "-- Invocation of create_completion_list()\n");

    process_t *process;
    completion_list_node_t *comp_list;
    ums_clid_t list_id;

    process = check_if_process_exists(current->pid);
    if(process == NULL)
    {
        return -UMS_ERROR_PROCESS_NOT_FOUND;
    }
    
    comp_list = kmalloc(sizeof(completion_list_node_t), GFP_KERNEL);
    list_add_tail(&(comp_list->list), &process->completion_lists->list);

    comp_list->clid = process->completion_lists->list_count;
    process->completion_lists->list_count++;
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
    printk(KERN_INFO UMS_MODULE_NAME_LOG "-- Invocation of create_worker_thread()\n");

    process_t *process;
    worker_t *worker;
    completion_list_node_t *comp_list;
    ums_wid_t worker_id;
    worker_params_t kern_params;

    process = check_if_process_exists(current->pid);
    if(process == NULL)
    {
        return -UMS_ERROR_PROCESS_NOT_FOUND;
    }

    int ret = copy_from_user(&kern_params, params, sizeof(worker_params_t));
    if(ret != 0)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG "--- Error: create_worker_thread() => copy_from_user failed to copy %d bytes\n", ret);
        return ret;
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
    worker->clid = comp_list->clid;
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

ums_sid_t enter_scheduling_mode(scheduler_params_t *params)
{
    printk(KERN_INFO UMS_MODULE_NAME_LOG "-- Invocation of enter_scheduling_mode()\n");
    
    process_t *process;
    scheduler_t *scheduler;
    completion_list_node_t *comp_list;
    ums_sid_t scheduler_id;
    scheduler_params_t kern_params;

    process = check_if_process_exists(current->tgid);
    if(process == NULL)
    {
        return -UMS_ERROR_PROCESS_NOT_FOUND;
    }

    int ret = copy_from_user(&kern_params, params, sizeof(scheduler_params_t));
    if(ret != 0)
    {
        printk(KERN_INFO UMS_MODULE_NAME_LOG "--- Error: enter_scheduling_mode(): copy_from_user failed to copy %d bytes\n", ret);
        return ret;
    }

    comp_list = check_if_completion_list_exists(process, kern_params.clid);
    if(comp_list == NULL)
    {
        return -UMS_ERROR_COMPLETION_LIST_NOT_FOUND;
    }

    scheduler = kmalloc(sizeof(scheduler_t), GFP_KERNEL);
    list_add_tail(&(scheduler->list), &process->scheduler_list->list);

    scheduler->sid = process->scheduler_list->scheduler_count;
    scheduler->pid = current->pid;
    scheduler->tid = current->tgid;
    scheduler->wid = -1;
    scheduler->state = IDLE;
    scheduler->entry_point = kern_params.entry_point;
    scheduler->comp_list = comp_list;
    scheduler_id = scheduler->sid;

    kern_params.sid = scheduler_id;
    ret = copy_to_user(params, &kern_params, sizeof(scheduler_params_t));
    if(ret != 0)
    {
        printk(KERN_INFO UMS_MODULE_NAME_LOG "--- Error: enter_scheduling_mode(): copy_to_user failed to copy %d bytes\n", ret);
        return ret;
    }

    memcpy(&scheduler->regs, task_pt_regs(current), sizeof(struct pt_regs));
    memset(&scheduler->fpu_regs, 0, sizeof(struct fpu));
    copy_fxregs_to_kernel(&scheduler->fpu_regs);

    scheduler->return_addr = scheduler->regs.ip;
    scheduler->stack_ptr = scheduler->regs.sp;
    scheduler->base_ptr = scheduler->regs.bp;
    scheduler->regs.ip = kern_params.entry_point;
    process->scheduler_list->scheduler_count++;

    ret = create_scheduler_proc_entry(process, scheduler);
    if(ret != 0)
    {
        printk(KERN_INFO UMS_MODULE_NAME_LOG "--- Error: enter_scheduling_mode(): %d\n", ret);
        return ret;
    }

    memcpy(task_pt_regs(current), &scheduler->regs, sizeof(struct pt_regs));
        
    return scheduler_id;
}

int exit_scheduling_mode(void)
{
    printk(KERN_INFO UMS_MODULE_NAME_LOG "-- Invocation of exit_scheduling_mode()\n");
    
    process_t *process;
    scheduler_t *scheduler;
    
    process = check_if_process_exists(current->tgid);
    if(process == NULL)
    {
        return -UMS_ERROR_PROCESS_NOT_FOUND;
    }

    scheduler = check_if_scheduler_exists_run_by(process, current->pid);
    if(scheduler == NULL)
    {
        return -UMS_ERROR_SCHEDULER_NOT_FOUND;
    }
    scheduler->wid = -1;
    scheduler->state = FINISHED;


    scheduler->regs.ip = scheduler->return_addr;
    scheduler->regs.sp = scheduler->stack_ptr;
    scheduler->regs.bp = scheduler->base_ptr;

    memcpy(task_pt_regs(current), &scheduler->regs, sizeof(struct pt_regs));
    copy_kernel_to_fxregs(&scheduler->fpu_regs.state.fxsave);

    return UMS_SUCCESS;
}

int execute_thread(ums_wid_t worker_id)
{
    printk(KERN_INFO UMS_MODULE_NAME_LOG "-- Invocation of execute_thread()\n");

    process_t *process;
    worker_t *worker;
    scheduler_t *scheduler;
    completion_list_node_t *comp_list;

    process = check_if_process_exists(current->tgid);
    if(process == NULL)
    {
        return -UMS_ERROR_PROCESS_NOT_FOUND;
    }

    scheduler = check_if_scheduler_exists_run_by(process, current->pid);
    if(scheduler == NULL)
    {
        return -UMS_ERROR_SCHEDULER_NOT_FOUND;
    }

    comp_list = scheduler->comp_list;
    worker = check_if_worker_exists(comp_list->idle_list, worker_id);
    if(worker == NULL)
    {
        return -UMS_ERROR_WORKER_NOT_FOUND;
    }

    if(worker->state == RUNNING)
    {
        return -UMS_ERROR_WORKER_ALREADY_RUNNING;

    }
    if(worker->state == FINISHED)
    {
        return -UMS_ERROR_WORKER_ALREADY_FINISHED;

    }


    scheduler->switch_count++;
    worker->switch_count++;

    ktime_get_real_ts64(&scheduler->time_of_the_last_switch);
    ktime_get_real_ts64(&worker->time_of_the_last_switch);

    worker->state = RUNNING;
    worker->sid = scheduler->sid;
    worker->pid = current->pid;
    scheduler->wid = worker->wid;

    list_move_tail(&(worker->local_list), &comp_list->busy_list->list);
    comp_list->idle_list->worker_count--;
    comp_list->busy_list->worker_count++;

    memcpy(&scheduler->regs, task_pt_regs(current), sizeof(struct pt_regs));
    copy_fxregs_to_kernel(&scheduler->fpu_regs);

    memcpy(task_pt_regs(current), &worker->regs, sizeof(struct pt_regs));
    copy_kernel_to_fxregs(&worker->fpu_regs.state.fxsave);

    scheduler->time_needed_for_the_last_switch += get_exec_time(&scheduler->time_of_the_last_switch);

    return UMS_SUCCESS;
}

int thread_yield(worker_status_t status)
{
    printk(KERN_INFO UMS_MODULE_NAME_LOG "-- Invocation of thread_yield()\n");

    process_t *process;
    worker_t *worker;
    scheduler_t *scheduler;
    completion_list_node_t *comp_list;

    if(status != FINISH && status != PAUSE) 
    {
        return -UMS_ERROR_WRONG_INPUT;
    }

    process = check_if_process_exists(current->tgid);
    if(process == NULL)
    {
        return -UMS_ERROR_PROCESS_NOT_FOUND;
    }

    scheduler = check_if_scheduler_exists_run_by(process, current->pid);
    if(scheduler == NULL)
    {
        return -UMS_ERROR_SCHEDULER_NOT_FOUND;
    }

    comp_list = scheduler->comp_list;
    worker = check_if_worker_exists(comp_list->busy_list, scheduler->wid);
    if(worker == NULL)
    {
        return -UMS_ERROR_WORKER_NOT_FOUND;
    }

    worker->total_exec_time += get_exec_time(&worker->time_of_the_last_switch);

    worker->state = (status == PAUSE) ? IDLE : FINISHED;
    scheduler->wid = -1;
    comp_list->finished_count = (status == PAUSE) ? comp_list->finished_count : comp_list->finished_count + 1;

    if(status == PAUSE)
    {
        list_move_tail(&(worker->local_list), &comp_list->idle_list->list);
        comp_list->busy_list->worker_count--;
        comp_list->idle_list->worker_count++;
    }

    memcpy(&worker->regs, task_pt_regs(current), sizeof(struct pt_regs));
    copy_fxregs_to_kernel(&worker->fpu_regs);

    memcpy(task_pt_regs(current), &scheduler->regs, sizeof(struct pt_regs));
    copy_kernel_to_fxregs(&scheduler->fpu_regs.state.fxsave);

    return UMS_SUCCESS;
}

int dequeue_completion_list_items(list_params_t *params)
{
    printk(KERN_INFO UMS_MODULE_NAME_LOG "-- Invocation of dequeue_completion_list_items()\n");

    process_t *process;
    worker_t *worker;
    scheduler_t *scheduler;
    completion_list_node_t *comp_list;
    list_params_t *kern_params;
    unsigned int size;

    process = check_if_process_exists(current->tgid);
    if(process == NULL)
    {
        return -UMS_ERROR_PROCESS_NOT_FOUND;
    }
    
    scheduler = check_if_scheduler_exists_run_by(process, current->pid);
    if(scheduler == NULL)
    {
        kfree(kern_params);
        return -UMS_ERROR_SCHEDULER_NOT_FOUND;
    }

    comp_list = scheduler->comp_list;

    kern_params = kmalloc(sizeof(list_params_t) + comp_list->worker_count * sizeof(ums_wid_t), GFP_KERNEL);
    int ret = copy_from_user(kern_params, params, sizeof(list_params_t) + comp_list->worker_count * sizeof(ums_wid_t));
    if(ret != 0)
    {
        printk(KERN_INFO UMS_MODULE_NAME_LOG "--- Error: dequeue_completion_list_items(): copy_from_user failed to copy %d bytes\n", ret);
        kfree(kern_params);
        return ret;
    }
    
    kern_params->state = comp_list->finished_count == comp_list->worker_count ? FINISHED : IDLE;

    unsigned int count = 0;
    
    if(!list_empty(&comp_list->idle_list->list))
    {
        worker_t *temp = NULL;
        worker_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &comp_list->idle_list->list, local_list) 
        {
            kern_params->workers[count] = temp->wid;
            count++;
            if (count > comp_list->worker_count) break;
        }
    }

    kern_params->worker_count = count;

    ret = copy_to_user(params, kern_params, sizeof(list_params_t) + comp_list->worker_count * sizeof(ums_wid_t));
    if(ret != 0)
    {
        printk(KERN_INFO UMS_MODULE_NAME_LOG "--- Error: dequeue_completion_list_items(): copy_to_user failed to copy %d bytes\n", ret);
        kfree(kern_params);
        return ret;
    }

    kfree(kern_params);
    return UMS_SUCCESS;
}

process_t *check_if_process_exists(pid_t pid)
{
    process_t *process = NULL;

    if(!list_empty(&process_list.list))
    {
        process_t *temp = NULL;
        process_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &process_list.list, list) 
        {
            if(temp->pid == pid)
            {
                process = temp;
                break;
            }
        }
    }

    return process;
}

completion_list_node_t *check_if_completion_list_exists(process_t *process, ums_clid_t clid)
{
    completion_list_node_t *comp_list;

    if(!list_empty(&process->completion_lists->list))
    {
        completion_list_node_t *temp = NULL;
        completion_list_node_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &process->completion_lists->list, list) 
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

scheduler_t *check_if_scheduler_exists(process_t *process, ums_sid_t sid)
{
    scheduler_t *scheduler;

    if(!list_empty(&process->scheduler_list->list))
    {
        scheduler_t *temp = NULL;
        scheduler_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &process->scheduler_list->list, list) 
        {
            if(temp->sid == pid)
            {
                scheduler = temp;
                break;
            }
        }
    }
  
    return scheduler;
}

scheduler_t *check_if_scheduler_exists_run_by(process_t *process, pid_t pid)
{
    scheduler_t *scheduler;

    if(!list_empty(&process->scheduler_list->list))
    {
        scheduler_t *temp = NULL;
        scheduler_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &process->scheduler_list->list, list) 
        {
            if(temp->pid == pid)
            {
                scheduler = temp;
                break;
            }
        }
    }
  
    return scheduler;
}

worker_t *check_if_worker_exists(worker_list_t *worker_list, ums_wid_t wid)
{
    worker_t *worker;

    if(!list_empty(&worker_list->list))
    {
        worker_t *temp = NULL;
        worker_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &worker_list->list, local_list) 
        {
            if(temp->wid == wid)
            {
                worker = temp;
                break;
            }
        }
    }
  
    return worker;
}

state_t check_schedulers_state(process_t *process)
{
    state_t progress = FINISHED;

    if(!list_empty(&process->scheduler_list->list))
    {
        scheduler_t *temp = NULL;
        scheduler_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &process->scheduler_list->list, list) 
        {
            
            if(temp->state == RUNNING || temp->state == IDLE) 
            {
                progress = temp->state;
                break;
            }

        }
    }
    return progress;
}

int delete_process(process_t *process)
{
    int ret;
    state_t progress = check_schedulers_state(process);

    if(progress != FINISHED)
    {
        ret = -UMS_ERROR_STATE_RUNNING;
        printk(KERN_INFO UMS_MODULE_NAME_LOG "--- Error: delete_process() => Schedulers are still running.\n");
        goto out;
    }

    delete_completion_lists_and_worker_threads(process);
    delete_schedulers(process);
    list_del(&process->list);
    process_list.process_count--;
    kfree(process->proc_entry);
    kfree(process);
    ret = UMS_SUCCESS;

 out:
    return ret;
}

int delete_process_safe(process_t *process)
{
    int ret;

    delete_completion_lists_and_worker_threads(process);
    delete_schedulers(process);
    list_del(&process->list);
    process_list.process_count--;
    kfree(process->proc_entry);
    kfree(process);
    ret = UMS_SUCCESS;

 out:
    return ret;
}

int delete_completion_lists_and_worker_threads(process_t *process)
{
    if(!list_empty(&process->completion_lists->list))
    {
        completion_list_node_t *temp = NULL;
        completion_list_node_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &process->completion_lists->list, list) 
        {
            printk(KERN_INFO UMS_MODULE_NAME_LOG "--- Completion list:%d was deleted.\n", temp->clid);

            if(temp->idle_list->worker_count > 0) delete_workers_from_completion_list(temp->idle_list);
            if(temp->busy_list->worker_count > 0) delete_workers_from_completion_list(temp->busy_list);
            kfree(temp->idle_list);
            kfree(temp->busy_list);
            list_del(&temp->list);
            kfree(temp);
        }
    }
    list_del(&process->completion_lists->list);
    kfree(process->completion_lists);
    delete_workers_from_process_list(process->worker_list);
    kfree(process->worker_list);
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
            printk(KERN_INFO UMS_MODULE_NAME_LOG "--- Worker thread:%d was deleted.\n", temp->wid);
            list_del(&temp->local_list);
            list_del(&temp->global_list);
            kfree(temp->proc_entry);
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
            printk(KERN_INFO UMS_MODULE_NAME_LOG "--- Worker thread:%d was deleted.\n", temp->wid);
            list_del(&temp->global_list);
            kfree(temp->proc_entry);
            kfree(temp);
        }
    }
    return UMS_SUCCESS;
}

int delete_schedulers(process_t *process)
{

    if(!list_empty(&process->scheduler_list->list))
    {
        scheduler_t *temp = NULL;
        scheduler_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &process->scheduler_list->list, list) 
        {
            printk(KERN_INFO UMS_MODULE_NAME_LOG "--- Scheduler:%d was deleted.\n", temp->sid);
            //Proc entries
            list_del(&temp->list);
            kfree(temp->proc_entry);
            kfree(temp);
        }
    }
    list_del(&process->scheduler_list->list);
    kfree(process->scheduler_list);
    return UMS_SUCCESS;
}

int cleanup()
{

    if(!list_empty(&process_list.list))
    {
        process_t *temp = NULL;
        process_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &process_list.list, list) 
        {
            delete_process_safe(temp);
        }
    }

    return UMS_SUCCESS;
}

unsigned long get_exec_time(struct timespec64 *prev_time)
{
    struct timespec64 current_time;
    unsigned long cur, prev;

    ktime_get_real_ts64(&current_time);

    cur = current_time.tv_sec * 1000 + current_time.tv_nsec / 1000000;
    prev = prev_time->tv_sec * 1000 + prev_time->tv_nsec / 1000000;

    return cur - prev;
}

int init_proc(void)
{
    printk(KERN_INFO UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG "> Initialization.\n");

    proc_ums = proc_mkdir(UMS_NAME, NULL);
    if(!proc_ums)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG"--- Error: create_process_proc_entry() => proc_mkdir() failed for " UMS_NAME "\n");
        return -UMS_ERROR_FAILED_TO_CREATE_PROC_ENTRY;
    }

    return UMS_SUCCESS;
}

int delete_proc(void)
{
    printk(KERN_INFO UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG "> Deleted.\n");
    proc_remove(proc_ums);
    return UMS_SUCCESS;
}

static struct proc_ops scheduler_proc_file_ops = {
    .proc_open = scheduler_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release
};

static struct proc_ops worker_proc_file_ops = {
    .proc_open = worker_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release
};

int create_process_proc_entry(process_t *process)
{
    process_proc_entry_t *process_pe;
	char buf[UMS_BUFFER_LEN];

    
    int ret = snprintf(buf, UMS_BUFFER_LEN, "%d", process->pid);
	if(ret != 0)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG"--- Error: create_process_proc_entry() => snprintf() failed to copy %d bytes\n", ret);
        return ret;
    }

    process_pe = kmalloc(sizeof(process_proc_entry_t), GFP_KERNEL);
    process->proc_entry = process_pe;
    process_pe->parent = proc_ums;
	process_pe->pde = proc_mkdir(buf, proc_ums);
	if (!process_pe->pde) {
		printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG"--- Error: create_process_proc_entry() => proc_mkdir() failed for Process:%d\n", pid);
        return -UMS_ERROR_FAILED_TO_CREATE_PROC_ENTRY;
	}

    process_pe->child = proc_mkdir("schedulers", process_pe->pde);
    if (!process_pe->child) {
		printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG"--- Error: create_process_proc_entry() => proc_mkdir() failed for Process:%d\n", pid);
        return -UMS_ERROR_FAILED_TO_CREATE_PROC_ENTRY;
	}

    return UMS_SUCCESS;
}

int create_scheduler_proc_entry(process_t *process, scheduler_t *scheduler)
{
    scheduler_proc_entry_t *scheduler_pe;
    char buf[UMS_BUFFER_LEN];

    int ret = snprintf(buf, UMS_BUFFER_LEN, "%d", scheduler->sid);
	if(ret != 0)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG"--- Error: create_scheduler_proc_entry() => snprintf() failed to copy %d bytes\n", ret);
        return ret;
    }

    scheduler_pe = kmalloc(sizeof(scheduler_proc_entry_t), GFP_KERNEL);
    scheduler->proc_entry = scheduler_pe;
    scheduler_pe->parent = process->proc_entry->child;
    scheduler_pe->pde = proc_mkdir(buf, scheduler_pe->parent);
    if (!scheduler_pe->pde) {
		printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG"--- Error: create_scheduler_proc_entry() => proc_mkdir() failed for Scheduler:%d\n", scheduler->sid);
        return -UMS_ERROR_FAILED_TO_CREATE_PROC_ENTRY;
	}

    scheduler_pe->child = proc_mkdir("workers", scheduler_pe->pde);
    if (!scheduler_pe->child) {
		printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG"--- Error: create_scheduler_proc_entry() => proc_mkdir() failed for Scheduler:%d\n", scheduler->sid);
        return -UMS_ERROR_FAILED_TO_CREATE_PROC_ENTRY;
	}

    scheduler_pe->info = proc_create("info", S_IALLUGO, scheduler_pe->pde, &scheduler_proc_file_ops);
    if (!scheduler_pe->info) {
		printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG"--- Error: create_scheduler_proc_entry() => proc_create() failed for Scheduler:%d\n", scheduler->sid);
        return -UMS_ERROR_FAILED_TO_CREATE_PROC_ENTRY;
	}

    if(!list_empty(&scheduler->comp_list->idle_list))
    {
        worker_t *temp = NULL;
        worker_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &scheduler->comp_list->idle_list, local_list) 
        {
            create_worker_proc_entry(process, scheduler, temp);
        }   
    }

    if(!list_empty(&scheduler->comp_list->busy_list))
    {
        worker_t *temp = NULL;
        worker_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &scheduler->comp_list->busy_list, local_list) 
        {
            create_worker_proc_entry(process, scheduler, temp);
        }
    }

    return UMS_SUCCESS;
}

int create_worker_proc_entry(process_t *process, scheduler_t *scheduler, worker_t *worker)
{
    worker_proc_entry_t *worker_pe;
    char buf[UMS_BUFFER_LEN];

    int ret = snprintf(buf, UMS_BUFFER_LEN, "%d", worker->wid);
    if(ret != 0)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG"--- Error: create_worker_proc_entry() => snprintf() failed to copy %d bytes\n", ret);
        return ret;
    }

    worker_pe = kmalloc(sizeof(worker_proc_entry_t), GFP_KERNEL);
    worker->proc_entry = worker_pe;
    worker_pe->parent = scheduler_pe->child;

    worker_pe->pde = proc_create(buf, S_IALLUGO, worker_pe->parent, &worker_proc_file_ops);
    if (!worker_pe->pde) {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG"--- Error: create_worker_proc_entry() => proc_create() failed for Worker:%d\n", worker->wid);
        return -UMS_ERROR_FAILED_TO_CREATE_PROC_ENTRY;
    }

    return UMS_SUCCESS;
}

static int scheduler_proc_open(struct inode *inode, struct file *file)
{
    process_t *process;
    scheduler_t *scheduler;
    pid_t pid;
    ums_sid_t sid;

    if (kstrtoul(file->f_path.dentry->d_parent->d_name.name, 10, &sid) != 0)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG"--- Error: scheduler_proc_open() => kstrtoul() failed for Scheduler:%d\n", sid);
        return -UMS_ERROR_FAILED_TO_PROC_OPEN;
    }
    if (kstrtoul(file->f_path.dentry->d_parent->d_parent->d_name.name, 10, &pid) != 0)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG"--- Error: scheduler_proc_open() => kstrtoul() failed for Process:%d\n", pid);
        return -UMS_ERROR_FAILED_TO_PROC_OPEN;
    }

    process = check_if_process_exists(pid);
    scheduler = check_if_scheduler_exists(process, sid);

    int ret = single_open(file, scheduler_proc_show, scheduler);

    return UMS_SUCCESS;
}

static int worker_proc_open(struct inode *inode, struct file *file)
{
    process_t *process;
    worker_t *worker;
    scheduler_t *scheduler;
    pid_t pid;
    ums_sid_t sid;
    ums_wid_t wid;

    if (kstrtoul(file->f_path.dentry->d_name.name, 10, &wid) != 0)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG"--- Error: worker_proc_open() => kstrtoul() failed for Worker:%d\n", wid);
        return -UMS_ERROR_FAILED_TO_PROC_OPEN;
    }
    if (kstrtoul(file->f_path.dentry->d_parent->d_parent->d_name.name, 10, &sid) != 0)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG"--- Error: worker_proc_open() => kstrtoul() failed for Scheduler:%d\n", sid);
        return -UMS_ERROR_FAILED_TO_PROC_OPEN;
    }
    if (kstrtoul(file->f_path.dentry->d_parent->d_parent->d_parent->d_parent->d_name.name, 10, &pid) != 0)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG"--- Error: worker_proc_open() => kstrtoul() failed for Process:%d\n", pid);
        return -UMS_ERROR_FAILED_TO_PROC_OPEN;
    }

    process = check_if_process_exists(pid);
    scheduler = check_if_scheduler_exists(process, sid);
    worker = check_if_worker_exists(process->worker_list, wid);

    int ret = single_open(file, worker_proc_show, worker);

    return UMS_SUCCESS;
}

static int scheduler_proc_show(struct seq_file *m, void *p)
{
    scheduler_t *scheduler = (scheduler_t*)m->private;
    seq_printf(m, "Scheduler id: %d\n", scheduler->sid);
    seq_printf(m, "Entry point: %p\n", (void*)scheduler->entry_point);
    seq_printf(m, "Completion list: %d\n", scheduler->comp_list->clid);
	seq_printf(m, "Number of times the scheduler switched to a worker thread: %d\n", scheduler->switch_count);
    seq_printf(m, "Time needed for the last worker thread switch: %lu\n", scheduler->time_needed_for_the_last_switch);
    if(scheduler->state == IDLE) seq_printf(m, "Scheduler status is: IDLE.\n");
    else if(scheduler->state == RUNNING) seq_printf(m, "Scheduler status is: Running.\n");
	else if(scheduler->state == FINISHED) seq_printf(m, "Scheduler status is: Finished.\n");

    return UMS_SUCCESS;
}

static int worker_proc_show(struct seq_file *m, void *p)
{
    worker_t *worker = (worker_t*)m->private;
    seq_printf(m, "Worker id: %d\n", worker->wid);
    seq_printf(m, "Entry point: %p\n", (void*)worker->entry_point);
    seq_printf(m, "Completion list: %d\n", worker->clid);
	seq_printf(m, "Number of switches: %d\n", worker->switch_count);
    seq_printf(m, "Total running time of the thread: %lu\n", worker->total_exec_time);
    if(worker_t->state == IDLE) seq_printf(m, "Worker status is: IDLE.\n");
    else if(worker_t->state == RUNNING) seq_printf(m, "Worker status is: Running.\n", );
	else if(worker_t->state == FINISHED) seq_printf(m, "Worker status is: Finished.\n");

    return UMS_SUCCESS;
}
