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
 * @brief Contains implementations of the UMS kernel module API
 *
 * @file ums_api.c
 * @author Bektur Umarbaev <hrafnulf13@gmail.com>
 * @date 
 */
#include "ums_api.h"

/*
 * Global variables
 */
static struct proc_dir_entry *proc_ums;
process_list_t process_list = {
    .list = LIST_HEAD_INIT(process_list.list),
};

/*
 * Static functions
 */
static int scheduler_proc_open(struct inode *inode, struct file *file);
static int worker_proc_open(struct inode *inode, struct file *file);
static int scheduler_proc_show(struct seq_file *m, void *p);
static int worker_proc_show(struct seq_file *m, void *p);

/** @brief Called by a process to request a scheduling management
 *.
 *  Checks if the process is already managed or not, if not:
 *   - Creates a @ref process data structure by calling @ref create_process_node()
 *   - Creates the proc entries by calling @ref create_process_proc_entry()
 *   
 *  @return returns @c UMS_SUCCESS when succesful or error constant if there are any errors  
 */
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

/** @brief Called by a process to request a completion of the scheduling management
 *.
 *  Checks if the process is already managed or not, if not:
 *   - Sets the @ref state of the process to @c FINISHED, but does not delete related data structures of the process (which are deleted when UMS kernel module exits)
 *   
 *  @return returns @c UMS_SUCCESS when succesful or error constant if there are any errors  
 */
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

/** @brief Creates a @ref process data structure to handle the specified process
 *.
 *  To create a @ref process data structure, UMS kernel module:
 *   - Allocates and initializes @ref process:
 *      - process::pid is set to @p pid
 *      - process::state is set to RUNNING
 *      - Allocates and initializes @ref completion_list member of the @ref process to track completion lists created by the process
 *      - Allocates and initializes @ref worker_list  member of the @ref process to track worker threads created by the process
 *      - Allocates and initializes @ref scheduler_list  member of the @ref process to track schedulers created by the process
 *.
 *
 *  @param pid pid of the process
 *  @return returns a pointer to @ref process data structure of the specified process
 */
process_t *create_process_node(pid_t pid)
{
    process_t *process;

    process = kmalloc(sizeof(process_t), GFP_KERNEL);
    list_add_tail(&process->list, &process_list.list);

    process_list.process_count++;
    process->pid = pid;
    process->state = RUNNING;

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

/** @brief Creates a @ref completion_list_node for the process
 *.
 *  To create a @ref completion_list_node, UMS kernel module:
 *   - Checks if the process is already managed, if not returns @c UMS_ERROR_PROCESS_NOT_FOUND
 *   - Allocates and initializes @ref completion_list_node:
 *      - Adds the completion list to the list of completion lists created by the process
 *      - completion_list_node::clid is set to process::completion_lists::list_count value (which is incremented after)
 *      - completion_list_node::worker_count is set to 0
 *      - completion_list_node::finished_count is set to 0
 *      - completion_list_node::state is set to IDLE
 *      - Allocates and initializes @ref idle_list member of the @ref process to track idle worker threads created by the process
 *      - Allocates and initializes @ref busy_list  member of the @ref process to track finished and running worker threads created by the process
 *  
 *  @return returns completion list ID
 */
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
    comp_list->state = IDLE;
    
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

/** @brief Creates a @ref worker thread for the process
 *.
 *  To create a @ref worker, UMS kernel module:
 *   - Checks if the process is already managed, if not returns @c UMS_ERROR_PROCESS_NOT_FOUND
 *   - Checks if completion list exists based on the passed parameters @p params, if not returns @c UMS_ERROR_COMPLETION_LIST_NOT_FOUND
 *   - Checks if completion list is used currently, thus cannot be modified and returns @c UMS_ERROR_COMPLETION_LIST_IS_USED_AND_CANNOT_BE_MODIFIED
 *   - Allocates and initializes @ref worker:
 *      - Adds the worker to the list of workers created by the process
 *      - worker::wid is set to process::worker_list::worker_count value (which is incremented after)
 *      - worker::pid is set to -1
 *      - worker::tid is set to @c current->tgid
 *      - worker::clid is set to worker_params::clid
 *      - worker::state is set to IDLE
 *      - worker::entry_point is set to worker_params::entry_point
 *      - worker::stack_addr is set to worker_params::stack_addr
 *      - worker::switch_count is set to 0;
 *      - worker::total_exec_time is set to 0;
 *      - worker::regs is a @c pt_regs data structure and set to a snapshot of current CPU registers of the process
 *          - regs::ip is set to worker_params::entry_point
 *          - regs::di is set to worker_params::function_args
 *          - regs::sp is set to worker_params::stack_addr
 *          - regs::bp is set to worker_params::stack_addr
 *      - worker::fpu_regs is a @c fpu data structure and set to a snapshot of current FPU registers of the process
 *      - Adds worker to the idle list of the completion list
 * 
 *  @param params pointer to @ref worker_params
 *  @return returns worker ID
 */
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

    if(comp_list->state == RUNNING)
    {
        return -UMS_ERROR_COMPLETION_LIST_IS_USED_AND_CANNOT_BE_MODIFIED;
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
    worker->switch_count = 0;
    worker->total_exec_time = 0;

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

/** @brief Converts a pthread to the @ref scheduler
 *
 *  To create a @ref scheduler, UMS kernel module:
 *   - Checks if the process is already managed, if not returns @c UMS_ERROR_PROCESS_NOT_FOUND
 *   - Checks if completion list exists based on the passed parameters @p params, if not returns @c UMS_ERROR_COMPLETION_LIST_NOT_FOUND
 *   - Allocates and initializes @ref scheduler:
 *      - Adds the scheduler to the list of schedulers created by the process
 *      - scheduler::sid is set to process::scheduler_list::scheduler_count; value (which is incremented after)
 *      - scheduler::pid is set to @c current->pid
 *      - scheduler::tid is set to @c current->tgid
 *      - scheduler::wid is set to -1
 *      - scheduler::state is set to IDLE
 *      - scheduler::entry_point is set to scheduler_params::entry_point
 *      - scheduler::avg_switch_time is set to 0;
 *      - scheduler::avg_switch_time_full is set to 0;
 *      - scheduler::time_needed_for_the_last_switch is set to 0;
 *      - scheduler::time_needed_for_the_last_switch_full is set to 0;
 *      - scheduler::comp_list is set to the pointer of the completion list retrieved using @ref check_if_completion_list_exists by passing scheduler_params::clid
 *      - scheduler::regs is a @c pt_regs data structure and set to a snapshot of current CPU registers of the pthread
 *          - scheduler::return_addr is set to regs::ip
 *          - scheduler::stack_ptr is set to regs::sp
 *          - scheduler::base_ptr is set to regs::bp
 *          - regs::ip is set to scheduler_params::entry_point
 *      - scheduler::fpu_regs is a @c fpu data structure and set to a snapshot of current FPU registers of the pthread
 *      - Sets the state of the completion list assigned to that scheduler to RUNNING, since the scheduling starts after the completion of ioctl call
 *      - Creates @ref scheduler_proc_entry for the scheduler by calling @ref create_scheduler_proc_entry()
 *      - Performs a context switch by copying previosly saved and modified scheduler::regs data structure to @c task_pt_regs(current)
 *      
 *  @param params pointer to @ref scheduler_params
 *  @return returns scheduler ID
 */
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
    scheduler->switch_count = 0;
    scheduler->avg_switch_time = 0;
    scheduler->avg_switch_time_full = 0;
    scheduler->time_needed_for_the_last_switch = 0;
    scheduler->time_needed_for_the_last_switch_full = 0;
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
    comp_list->state = RUNNING;

    ret = create_scheduler_proc_entry(process, scheduler);
    if(ret != 0)
    {
        printk(KERN_INFO UMS_MODULE_NAME_LOG "--- Error: enter_scheduling_mode(): %d\n", ret);
        return ret;
    }

    memcpy(task_pt_regs(current), &scheduler->regs, sizeof(struct pt_regs));
    ktime_get_real_ts64(&scheduler->time_of_the_last_switch_full);
        
    return scheduler_id;
}

/** @brief Converts @ref scheduler back to the pthread
 *
 *  To create a @ref scheduler, UMS kernel module:
 *   - Checks if the process is already managed, if not returns @c UMS_ERROR_PROCESS_NOT_FOUND
 *   - Checks if scheduler associated by the pthread already exists, if not returns @c UMS_ERROR_SCHEDULER_NOT_FOUND
 *   - Modifies @ref scheduler:
 *      - scheduler::wid is set to -1
 *      - scheduler::state is set to FINISHED
 *      - scheduler::regs is modified:
 *          - regs::ip is set to scheduler::return_addr 
 *          - regs::sp is set to scheduler::stack_ptr
 *          - regs::bp is set to scheduler::base_ptr
 *          - regs::ip is set to scheduler_params::entry_point
 *      - Performs a context switch by copying previosly saved and modified scheduler::regs data structure to @c task_pt_regs(current)
 *      - Changes current FPU registers to previously saved scheduller::fpu_regs via @c copy_kernel_to_fxregs()
 *      
 *  @return returns @c UMS_SUCCESS when succesful or error constant if there are any errors  
 */
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

    if(scheduler->wid != -1)
    {
        return -UMS_ERROR_CMD_IS_NOT_ISSUED_BY_SCHEDULER;
    }
    scheduler->state = FINISHED;


    scheduler->regs.ip = scheduler->return_addr;
    scheduler->regs.sp = scheduler->stack_ptr;
    scheduler->regs.bp = scheduler->base_ptr;

    memcpy(task_pt_regs(current), &scheduler->regs, sizeof(struct pt_regs));
    copy_kernel_to_fxregs(&scheduler->fpu_regs.state.fxsave);

    return UMS_SUCCESS;
}

/** @brief Executes a worker thread with a @p worker_id
 *.
 *  To execute the worker thread: 
 *   - Checks if the process is already managed, if not returns @c UMS_ERROR_PROCESS_NOT_FOUND
 *   - Checks if scheduler associated by the pthread already exists, if not returns @c UMS_ERROR_SCHEDULER_NOT_FOUND
 *   - Checks if the worker thread exists, currently running, completed its' work
 *   - Updates the worker and scheduler data structures
 *   - Records the statistics related to the scheduler and worker, such as number of switches and the time the switch happened
 *   - Saves the register values of the scheduler
 *   - Performs a context switch
 *   
 *
 *  @param worker_id Worker thread ID
 *  @return returns @c UMS_SUCCESS when succesful or error constant if there are any errors  
 */
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
    scheduler->state = RUNNING;

    list_move_tail(&(worker->local_list), &comp_list->busy_list->list);
    comp_list->idle_list->worker_count--;
    comp_list->busy_list->worker_count++;

    memcpy(&scheduler->regs, task_pt_regs(current), sizeof(struct pt_regs));
    copy_fxregs_to_kernel(&scheduler->fpu_regs);

    memcpy(task_pt_regs(current), &worker->regs, sizeof(struct pt_regs));
    copy_kernel_to_fxregs(&worker->fpu_regs.state.fxsave);

    scheduler->time_needed_for_the_last_switch += get_exec_time(&scheduler->time_of_the_last_switch);
    scheduler->time_needed_for_the_last_switch_full += get_exec_time(&scheduler->time_of_the_last_switch_full);

    scheduler->avg_switch_time = scheduler->time_needed_for_the_last_switch / scheduler->switch_count;
    scheduler->avg_switch_time_full = scheduler->time_needed_for_the_last_switch_full / scheduler->switch_count;

    return UMS_SUCCESS;
}

/** @brief Pauses or completes the execution of the worker thread
 *.
 *  To pause or complete the execution of the worker thread: 
 *   - Checks if the process is already managed, if not returns @c UMS_ERROR_PROCESS_NOT_FOUND
 *   - Checks if scheduler associated by the pthread already exists, if not returns @c UMS_ERROR_SCHEDULER_NOT_FOUND
 *   - Checks if the worker thread exists, currently running, completed its' work
 *   - Updates the worker, scheduler and completion list data structures:
 *      - if @p status is set to PAUSE:
 *          - worker::state is set to IDLE, so that it can be scheduled later
 *          - worker is added back to the completion list 
 *      - if @p status is set to FINISH:
 *          - worker::state is set to FINISHED
 *          - completion list increments the value of finished workers
 *   - Records the statistics related to the scheduler and worker, such as number of switches and the time the switch happened
 *   - Saves the register values of the worker thread
 *   - Performs a context switch
 *   
 *
 *  @param status value of @ref worker_status, which is the status of the worker thread
 *  @return returns @c UMS_SUCCESS when succesful or error constant if there are any errors  
 */
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
    ktime_get_real_ts64(&scheduler->time_of_the_last_switch_full);

    worker->state = (status == PAUSE) ? IDLE : FINISHED;
    scheduler->wid = -1;
    scheduler->state = IDLE;
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

/** @brief Provides a list of available worker threads of the completion list that can be scheduled
 *.
 *   To retrieve the list of available worker threads: 
 *   - Checks if the process is already managed, if not returns @c UMS_ERROR_PROCESS_NOT_FOUND
 *   - Checks if scheduler associated by the pthread already exists, if not returns @c UMS_ERROR_SCHEDULER_NOT_FOUND
 *   - Checks if there are any available workers, if not modifes @p params state value to FINISHED to indicate the completion of the work
 *   - Retrieves the list of idle worker threads from the completion list and copies them back to the @p params
 *
 *
 *  @param params pointer to @ref list_params
 *  @return returns @c UMS_SUCCESS when succesful or error constant if there are any errors  
 */
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

/** @brief Checks if @p process with @p pid is managed by the UMS kernel module
 *.
 * 
 *  @param pid pid of the process
 *  @return returns pointer to @ref process, or @c NULL if no process was found
 */
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

/** @brief Checks if completion list with @p clid was created by a @p process
 *.
 * 
 *  @param process pointer to @ref process
 *  @param clid Completion list ID
 *  @return returns pointer to @ref completion_list_node, or @c NULL if no completion list was found
 */
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

/** @brief Checks if scheduler with @p sid was created by a @p process
 *.
 * 
 *  @param process pointer to @ref process
 *  @param sid Scheduler ID
 *  @return returns pointer to @ref scheduler, or @c NULL if no scheduler was found
 */
scheduler_t *check_if_scheduler_exists(process_t *process, ums_sid_t sid)
{
    scheduler_t *scheduler;

    if(!list_empty(&process->scheduler_list->list))
    {
        scheduler_t *temp = NULL;
        scheduler_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &process->scheduler_list->list, list) 
        {
            if(temp->sid == sid)
            {
                scheduler = temp;
                break;
            }
        }
    }
  
    return scheduler;
}

/** @brief Checks if scheduler with @p pid was created by a @p process
 *.
 * 
 *  @param process pointer to @ref process
 *  @param pid pid of the pthread which is associated with scheduler
 *  @return returns pointer to @ref scheduler, or @c NULL if no scheduler was found
 */
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

/** @brief Checks if worker thread with @p wid exists in @p worker_list of the completion list
 *.
 * Search is performed using local_list member of @ref worker 
 * 
 *  @param worker_list pointer to @ref worker_list
 *  @param wid Worker ID
 *  @return returns pointer to @ref worker, or @c NULL if no worker was found
 */
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

/** @brief Checks if worker thread with @p wid exists in @p worker_list of the process
 *.
 * Search is performed using global_list member of @ref worker
 *  
 *  @param worker_list pointer to @ref worker_list
 *  @param wid Worker ID
 *  @return returns pointer to @ref worker, or @c NULL if no worker was found
 */
worker_t *check_if_worker_exists_global(worker_list_t *worker_list, ums_wid_t wid)
{
    worker_t *worker;

    if(!list_empty(&worker_list->list))
    {
        worker_t *temp = NULL;
        worker_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &worker_list->list, global_list) 
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

/** @brief Checks if schedulers of the process have finished their work
 *.
 *
 *  @param process pointer to @ref process 
 *  @return returns @ref state
 */
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

/** @brief Deletes @ref process from the global @ref process_list 
 *.
 *  The function was used during early phases of development and later was left unused,
 *  since proc entries of the process are linked to @ref process (in case user wants to see statistics, the data should be available for read), it was decided that only kernel module can delete data structures allocated for the process
 *  Therefore @ref delete_process_safe() is used to delete @ref process 
 *  Still the function performs a check to see if all schedulers have finished their job and then performs series of deletions of all data structures used by the process
 *
 *  @param process pointer to @ref process 
 *  @return returns @c UMS_SUCCESS if succesful 
 */
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
    //delete_workers_from_process_list(process->worker_list);
    delete_schedulers(process);
    list_del(&process->list);
    process_list.process_count--;
    kfree(process->proc_entry);
    kfree(process);
    ret = UMS_SUCCESS;

 out:
    return ret;
}

/** @brief Called by UMS kernel module when module exits to delete @ref process from the global @ref process_list 
 *.
 *  The function does not perform a check to see if all schedulers have finished their job to delete data structures used by the process
 *  It is assumed that all work has been done, therefore user issued a command to UMS kernel module to exit
 *
 *  @param process pointer to @ref process 
 *  @return returns @c UMS_SUCCESS if succesful 
 */
int delete_process_safe(process_t *process)
{
    int ret;

    delete_completion_lists_and_worker_threads(process);
    //delete_workers_from_process_list(process->worker_list);
    delete_schedulers(process);
    list_del(&process->list);
    process_list.process_count--;
    kfree(process->proc_entry);
    kfree(process);
    ret = UMS_SUCCESS;

 out:
    return ret;
}

/** @brief Deletes completion lists and worker threads created by the process 
 *.
 *  Calls @ref delete_workers_from_completion_list() and frees allocated memory that was used by the completion lists
 *
 *  @param process pointer to @ref process 
 *  @return returns @c UMS_SUCCESS if succesful 
 */
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

/** @brief Deletes worker threads assigned to the completion list
 *.
 *
 *  @param worker_list pointer to @ref worker_list of the @ref completion_list_node 
 *  @return returns @c UMS_SUCCESS if succesful 
 */
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

/** @brief Deletes worker threads created by the process
 *.
 *  Frees allocated memory that was used by the worker threads
 *
 *  @param worker_list pointer to @ref worker_list of the @ref process 
 *  @return returns @c UMS_SUCCESS if succesful 
 */
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

/** @brief Deletes schedulers created by the process
 *.
 *  Frees allocated memory that was used by the schedulers
 *  
 *  @param process pointer to @ref process 
 *  @return returns @c UMS_SUCCESS if succesful 
 */
int delete_schedulers(process_t *process)
{

    if(!list_empty(&process->scheduler_list->list))
    {
        scheduler_t *temp = NULL;
        scheduler_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &process->scheduler_list->list, list) 
        {
            printk(KERN_INFO UMS_MODULE_NAME_LOG "--- Scheduler:%d was deleted.\n", temp->sid);
            list_del(&temp->list);
            kfree(temp->proc_entry);
            kfree(temp);
        }
    }
    list_del(&process->scheduler_list->list);
    kfree(process->scheduler_list);
    return UMS_SUCCESS;
}

/** @brief Performs a cleanup by deleting all the allocated data structures for all processes that were managed by the UMS kernel module
 *.
 *  
 * 
 *  @return returns @c UMS_SUCCESS if succesful 
 */
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

/** @brief Computes time difference between passed @p prev_time and current time, which is used in this case as an indicator of execution time for @ref worker and @ref scheduler
 *.
 *
 *  @param prev_time member of @ref worker and @ref scheduler
 *  @return returns unsigned long
 */
unsigned long get_exec_time(struct timespec64 *prev_time)
{
    struct timespec64 current_time;
    unsigned long cur, prev;

    ktime_get_real_ts64(&current_time);

    cur = current_time.tv_sec * 1000000000 + current_time.tv_nsec;
    prev = prev_time->tv_sec * 1000000000 + prev_time->tv_nsec;

    return cur - prev;
}

/** @brief Initializes the core proc directory for UMS kernel module
 *.
 *
 *  
 *  @return returns @c UMS_SUCCESS when succesful or error constant if there are any errors  
 */
int init_proc(void)
{
    printk(KERN_INFO UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG "> Initialization.\n");

    proc_ums = proc_mkdir(UMS_NAME, NULL);
    if(!proc_ums)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG "--- Error: create_process_proc_entry() => proc_mkdir() failed for " UMS_NAME "\n");
        return -UMS_ERROR_FAILED_TO_CREATE_PROC_ENTRY;
    }

    return UMS_SUCCESS;
}

/** @brief Deletes all proc files of the UMS kernel module
 *.
 *
 *
 *  @return returns @c UMS_SUCCESS when succesful or error constant if there are any errors  
 */
int delete_proc(void)
{
    printk(KERN_INFO UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG "> Deleted.\n");
    proc_remove(proc_ums);
    return UMS_SUCCESS;
}

/*
 * Static declarations of file operations for proc filesystem
 */
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

/** @brief Dinamically creates essential proc entries for the process
 *.
 *  Allocates a memory for @ref process_proc_entry and initializes it:
 *      - Creates a folder to represent the process
 *      - Creates schedulers folder
 *
 *  @param process pointer to @ref process 
 *  @return returns @c UMS_SUCCESS when succesful or error constant if there are any errors  
 */
int create_process_proc_entry(process_t *process)
{
    process_proc_entry_t *process_pe;
	char buf[UMS_BUFFER_LEN];

    
    int ret = snprintf(buf, UMS_BUFFER_LEN, "%d", process->pid);
	if(ret < 0)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG "--- Error: create_process_proc_entry() => snprintf() failed to copy %d bytes\n", ret);
        return ret;
    }

    process_pe = kmalloc(sizeof(process_proc_entry_t), GFP_KERNEL);
    process->proc_entry = process_pe;
    process_pe->parent = proc_ums;
	process_pe->pde = proc_mkdir(buf, proc_ums);
	if (!process_pe->pde) {
		printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG "--- Error: create_process_proc_entry() => proc_mkdir() failed for Process:%d\n", process->pid);
        return -UMS_ERROR_FAILED_TO_CREATE_PROC_ENTRY;
	}

    process_pe->child = proc_mkdir("schedulers", process_pe->pde);
    if (!process_pe->child) {
		printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG "--- Error: create_process_proc_entry() => proc_mkdir() failed for Process:%d\n", process->pid);
        return -UMS_ERROR_FAILED_TO_CREATE_PROC_ENTRY;
	}

    return UMS_SUCCESS;
}

/** @brief Dinamically creates essential proc entries for the scheduler of the process
 *.
 *  Allocates a memory for @ref scheduler_proc_entry and initializes it:
 *      - Creates a folder to represent the scheduler
 *      - Creates info file that provides statistics about the scheduler
 *      - Creates workers folder of the completion list that is assigned to the scheduler
 *      - Creates proc entries for each worker thread by calling @ref create_worker_proc_entry()
 *
 *  @param process pointer to @ref process
 *  @param scheduler pointer to @ref scheduler
 *  @return returns @c UMS_SUCCESS when succesful or error constant if there are any errors  
 */
int create_scheduler_proc_entry(process_t *process, scheduler_t *scheduler)
{
    scheduler_proc_entry_t *scheduler_pe;
    char buf[UMS_BUFFER_LEN];

    int ret = snprintf(buf, UMS_BUFFER_LEN, "%d", scheduler->sid);
	if(ret < 0)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG "--- Error: create_scheduler_proc_entry() => snprintf() failed to copy %d bytes\n", ret);
        return ret;
    }

    scheduler_pe = kmalloc(sizeof(scheduler_proc_entry_t), GFP_KERNEL);
    scheduler->proc_entry = scheduler_pe;
    scheduler_pe->parent = process->proc_entry->child;
    scheduler_pe->pde = proc_mkdir(buf, scheduler_pe->parent);
    if (!scheduler_pe->pde) {
		printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG "--- Error: create_scheduler_proc_entry() => proc_mkdir() failed for Scheduler:%d\n", scheduler->sid);
        return -UMS_ERROR_FAILED_TO_CREATE_PROC_ENTRY;
	}

    scheduler_pe->child = proc_mkdir("workers", scheduler_pe->pde);
    if (!scheduler_pe->child) {
		printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG "--- Error: create_scheduler_proc_entry() => proc_mkdir() failed for Scheduler:%d\n", scheduler->sid);
        return -UMS_ERROR_FAILED_TO_CREATE_PROC_ENTRY;
	}

    scheduler_pe->info = proc_create("info", S_IALLUGO, scheduler_pe->pde, &scheduler_proc_file_ops);
    if (!scheduler_pe->info) {
		printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG "--- Error: create_scheduler_proc_entry() => proc_create() failed for Scheduler:%d\n", scheduler->sid);
        return -UMS_ERROR_FAILED_TO_CREATE_PROC_ENTRY;
	}

    if(!list_empty(&scheduler->comp_list->idle_list->list))
    {
        worker_t *temp = NULL;
        worker_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &scheduler->comp_list->idle_list->list, local_list) 
        {

            create_worker_proc_entry(process, scheduler, temp);
 
        }   
    }

    if(!list_empty(&scheduler->comp_list->busy_list->list))
    {
        worker_t *temp = NULL;
        worker_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &scheduler->comp_list->busy_list->list, local_list) 
        {

            create_worker_proc_entry(process, scheduler, temp);

        }
    }

    return UMS_SUCCESS;
}

/** @brief Dinamically creates essential proc entries for the worker of the process
 *.
 *  Allocates a memory for @ref worker_proc_entry and initializes it:
 *      - Creates a file that provides statistics about the worker
 *
 *  @param process pointer to @ref process
 *  @param scheduler pointer to @ref scheduler
 *  @param worker pointer to @ref worker
 *  @return returns @c UMS_SUCCESS when succesful or error constant if there are any errors  
 */
int create_worker_proc_entry(process_t *process, scheduler_t *scheduler, worker_t *worker)
{
    worker_proc_entry_t *worker_pe;
    char buf[UMS_BUFFER_LEN];

    int ret = snprintf(buf, UMS_BUFFER_LEN, "%d", worker->wid);
    if(ret < 0)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG "--- Error: create_worker_proc_entry() => snprintf() failed to copy %d bytes\n", ret);
        return ret;
    }

    worker_pe = kmalloc(sizeof(worker_proc_entry_t), GFP_KERNEL);
    worker->proc_entry = worker_pe;
    worker_pe->parent = scheduler->proc_entry->child;

    worker_pe->pde = proc_create(buf, S_IALLUGO, worker_pe->parent, &worker_proc_file_ops);
    if (!worker_pe->pde) {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG "--- Error: create_worker_proc_entry() => proc_create() failed for Worker:%d\n", worker->wid);
        return -UMS_ERROR_FAILED_TO_CREATE_PROC_ENTRY;
    }

    return UMS_SUCCESS;
}

/** @brief Function that is used when opening info file inside the scheduler folder, it will eventually calls @c single_open()
 *.
 *
 *  @param inode
 *  @param file
 *  @return returns @c UMS_SUCCESS when succesful or error constant if there are any errors  
 */
static int scheduler_proc_open(struct inode *inode, struct file *file)
{
    process_t *process;
    scheduler_t *scheduler;
    int pid, sid;

    if (kstrtoint(file->f_path.dentry->d_parent->d_name.name, 10, &sid) != 0)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG "--- Error: scheduler_proc_open() => kstrtoul() failed for Scheduler:%d\n", sid);
        return -UMS_ERROR_FAILED_TO_PROC_OPEN;
    }
    if (kstrtoint(file->f_path.dentry->d_parent->d_parent->d_parent->d_name.name, 10, &pid) != 0)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG "--- Error: scheduler_proc_open() => kstrtoul() failed for Process:%d\n", pid);
        return -UMS_ERROR_FAILED_TO_PROC_OPEN;
    }

    process = check_if_process_exists(pid);
    scheduler = check_if_scheduler_exists(process, sid);

    int ret = single_open(file, scheduler_proc_show, scheduler);

    return UMS_SUCCESS;
}

/** @brief Function that is used when opening worker file inside the workers folder of the scheduler, it will eventually calls @c single_open()
 *.
 *
 *  @param inode
 *  @param file
 *  @return returns @c UMS_SUCCESS when succesful or error constant if there are any errors  
 */
static int worker_proc_open(struct inode *inode, struct file *file)
{
    process_t *process;
    worker_t *worker;
    scheduler_t *scheduler;
    int pid, sid, wid;

    if (kstrtoint(file->f_path.dentry->d_name.name, 10, &wid) != 0)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG "--- Error: worker_proc_open() => kstrtoul() failed for Worker:%d\n", wid);
        return -UMS_ERROR_FAILED_TO_PROC_OPEN;
    }
    if (kstrtoint(file->f_path.dentry->d_parent->d_parent->d_name.name, 10, &sid) != 0)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG "--- Error: worker_proc_open() => kstrtoul() failed for Scheduler:%d\n", sid);
        return -UMS_ERROR_FAILED_TO_PROC_OPEN;
    }
    if (kstrtoint(file->f_path.dentry->d_parent->d_parent->d_parent->d_parent->d_name.name, 10, &pid) != 0)
    {
        printk(KERN_ALERT UMS_MODULE_NAME_LOG UMS_PROC_NAME_LOG" --- Error: worker_proc_open() => kstrtoul() failed for Process:%d\n", pid);
        return -UMS_ERROR_FAILED_TO_PROC_OPEN;
    }

    process = check_if_process_exists(pid);
    scheduler = check_if_scheduler_exists(process, sid);
    worker = check_if_worker_exists_global(process->worker_list, wid);

    int ret = single_open(file, worker_proc_show, worker);

    return UMS_SUCCESS;
}

/** @brief Shows the data/statistics of the @ref scheduler and used by @c single_open()
 *.
 *
 * 
 *  @return returns @c UMS_SUCCESS if succesful 
 */
static int scheduler_proc_show(struct seq_file *m, void *p)
{
    scheduler_t *scheduler = (scheduler_t*)m->private;
    seq_printf(m, "Scheduler id: %d\n", scheduler->sid);
    seq_printf(m, "Entry point: %p\n", (void*)scheduler->entry_point);
    seq_printf(m, "Completion list: %d\n", scheduler->comp_list->clid);
	seq_printf(m, "Number of times the scheduler switched to a worker thread: %d\n", scheduler->switch_count);
    seq_printf(m, "Time needed for the last worker thread switch: %lu\n", scheduler->time_needed_for_the_last_switch);
    seq_printf(m, "Time needed for the last full worker thread switch: %lu\n", scheduler->time_needed_for_the_last_switch_full);
    seq_printf(m, "Average time needed for the worker thread switch: %lu\n", scheduler->avg_switch_time);
    seq_printf(m, "Average time needed for the full worker thread switch: %lu\n", scheduler->avg_switch_time_full);
    if(scheduler->state == IDLE) seq_printf(m, "Scheduler status is: IDLE.\n");
    else if(scheduler->state == RUNNING) seq_printf(m, "Scheduler status is: Running.\n");
	else if(scheduler->state == FINISHED) seq_printf(m, "Scheduler status is: Finished.\n");

    return UMS_SUCCESS;
}

/** @brief Shows the data/statistics of the @ref worker and used by @c single_open()
 *.
 *
 *  
 *  @return returns @c UMS_SUCCESS if succesful 
 */
static int worker_proc_show(struct seq_file *m, void *p)
{
    worker_t *worker = (worker_t*)m->private;
    seq_printf(m, "Worker id: %d\n", worker->wid);
    seq_printf(m, "Run by Scheduler#: %d\n", worker->sid);
    seq_printf(m, "Entry point: %p\n", (void*)worker->entry_point);
    seq_printf(m, "Completion list: %d\n", worker->clid);
	seq_printf(m, "Number of switches: %d\n", worker->switch_count);
    seq_printf(m, "Total running time of the thread: %lu\n", worker->total_exec_time);
    if(worker->state == IDLE) seq_printf(m, "Worker status is: IDLE.\n");
    else if(worker->state == RUNNING) seq_printf(m, "Worker status is: Running.\n");
	else if(worker->state == FINISHED) seq_printf(m, "Worker status is: Finished.\n");

    return UMS_SUCCESS;
}
