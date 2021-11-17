/**
 * Copyright (C) 2021 Bektur Umarbaev <hrafnulf13@gmail.com>
 *
 * This file is part of the User Mode thread Scheduling (UMS) library.
 *
 * UMS library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * UMS library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with UMS library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * @brief Contains implementations of the essential UMS library functions 
 *
 * @file ums_lib.c
 * @author Bektur Umarbaev <hrafnulf13@gmail.com>
 * @date 
 */

#define _GNU_SOURCE
#include "ums_lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <sched.h>
#include <unistd.h>

/*
 * Global variables
 */
int ums_dev = -UMS_ERROR;                                 
pthread_mutex_t ums_mutex = PTHREAD_MUTEX_INITIALIZER;

ums_completion_list_t completion_lists = {
    .list = LIST_HEAD_INIT(completion_lists.list),
    .count = 0
};
ums_worker_list_t workers = {
    .list = LIST_HEAD_INIT(workers.list),
    .count = 0
};
ums_scheduler_list_t schedulers = {
    .list = LIST_HEAD_INIT(schedulers.list),
    .count = 0
};
__thread ums_clid_t completion_list_id;

/** @brief Opens UMS device
 *.
 * Uses mutex to protect a shared resource from simultaneous access by multiple threads
 *
 *  @return returns @c UMS_SUCCESS when succesful or @c UMS_ERROR if there are any errors 
 */
int open_device()
{
    pthread_mutex_lock(&ums_mutex);
    if(ums_dev < 0)
    {
        ums_dev = open(UMS_DEVICE, O_RDONLY);
        if(ums_dev < 0) 
        {
            printf("Error: open_device() => Error# = %d\n", errno);
            return -UMS_ERROR;
	    }
    }
    pthread_mutex_unlock(&ums_mutex);
    return UMS_SUCCESS;
}

/** @brief Closes UMS device
 *.
 * Uses mutex to protect a shared resource from simultaneous access by multiple threads
 *
 *  @return returns @c UMS_SUCCESS when succesful or @c UMS_ERROR if there are any errors 
 */
int close_device()
{
    pthread_mutex_lock(&ums_mutex);
    ums_dev = close(ums_dev);
    if(ums_dev < 0) 
    {
        printf("Error: close_device() => Error# = %d\n", errno);
        pthread_mutex_unlock(&ums_mutex);
        return -UMS_ERROR;
    }
    pthread_mutex_unlock(&ums_mutex);
    return UMS_SUCCESS;
}

/** @brief Requests UMS kernel module to manage current process
 *.
 * 
 *
 *  @return returns @c UMS_SUCCESS when succesful or @c UMS_ERROR if there are any errors 
 */
int ums_enter()
{
    completion_list_id = -1;

    int ret = open_device();
    if(ret < 0)
    {
        printf("Error: ums_enter() => UMS_DEVICE => Error#  = %d\n", errno);
        return -UMS_ERROR;
    }
    ret = ioctl(ums_dev, UMS_ENTER);
    if(ret < 0)
    {
        printf("Error: ums_enter()  => IOCTL => Error# = %d\n", errno);
        return -UMS_ERROR;
    }
    return ret;
}

/** @brief Requests UMS kernel module to finish management of the current process
 *.
 *
 *  
 *  @return returns @c UMS_SUCCESS when succesful or @c UMS_ERROR if there are any errors 
 */
int ums_exit()
{
    printf("UMS_LIB: ums_exit() invoked.\n");
    if(!list_empty(&schedulers.list))
    {
        ums_scheduler_t *temp = NULL;
        ums_scheduler_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &schedulers.list, list) 
        {
            int ret = pthread_join(temp->tid, NULL);
            if(ret < 0)
            {
                printf("Error: ums_create_scheduler() => Error# = %d\n", errno);
                goto out;
            }
        }
    }

    int ret = open_device();
    if(ret < 0)
    {
        printf("Error: ums_exit() => UMS_DEVICE => Error# = %d\n", errno);
        goto out;
    }

    ret = ioctl(ums_dev, UMS_EXIT);
    printf("UMS_LIB: ums_exit() => ioctl_ret = %d.\n", ret);
    if(ret < 0)
    {
        printf("Error: ums_exit() => IOCTL => Error# = %d, ret_val = %d\n", errno, ret);
        goto out;
    }

    out:
    close_device(ums_dev);
    cleanup();
    return ret;
}

/** @brief Requests UMS kernel module to create a completion lists
 *.
 *
 *  
 *  @return returns Completion list ID
 */
ums_clid_t ums_create_completion_list() 
{
    int ret = open_device();
    if(ret < 0)
    {
        printf("Error: ums_create_completion_list() => UMS_DEVICE => Error# = %d\n", errno);
        return -UMS_ERROR;
    }
    ret = ioctl(ums_dev, UMS_CREATE_LIST);
    if(ret < 0)
    {
        printf("Error: ums_create_completion_list()  => IOCTL => Error# = %d\n", errno);
        return -UMS_ERROR;
    }
    ums_completion_list_node_t *comp_list;
    comp_list = init(ums_completion_list_node_t);
    
    comp_list->clid = (ums_clid_t)ret;
    comp_list->worker_count = 0;
    comp_list->state = IDLE;
    list_add_tail(&(comp_list->list), &completion_lists.list);
    completion_lists.count++;

    return ret;
}

/** @brief Requests UMS kernel module to create a worker thread assigned to specific comletion list
 *.
 *  Library requests UMS kernel module to create a worker thread by passing @ref worker_params
 *
 *  @param clid ID of the completion list where worker thread is assigned to
 *  @param stack_size Stack size of the worker thread set by a user
 *  @param entry_point Function pointer and an entry point set by a user, that serves as a starting point of the worker thread
 *  @param args Pointer of the function arguments that are passed to the entry point/function 
 *  @return returns Worker ID
 */
ums_wid_t ums_create_worker_thread(ums_clid_t clid, unsigned long stack_size, void (*entry_point)(void *), void *args)
{
    ums_worker_t *worker;
    ums_completion_list_node_t *comp_list;
    comp_list = check_if_completion_list_exists(clid);
    if(comp_list == NULL)
    {
        printf("Error: ums_create_worker_thread() => Completion list:%d does not exist.\n", (int)clid);
        return -UMS_ERROR;
    }

    worker_params_t *params;
    params = init(worker_params_t);

    params->entry_point = (unsigned long)entry_point;
    params->function_args = (unsigned long)args;
    params->stack_size = stack_size < UMS_MIN_STACK_SIZE ? UMS_MIN_STACK_SIZE : stack_size;
    params->clid = clid;

    void *stack;
    int ret = posix_memalign(&stack, 16, stack_size);
    if(ret < 0)
    {
        printf("Error: ums_create_worker_thread() => Error# = %d\n", errno);
        delete(params);
        return -UMS_ERROR;
    }

    params->stack_addr = (unsigned long)stack + stack_size;
    ((unsigned long *)params->stack_addr)[0] = (unsigned long)&ums_thread_exit;
    params->stack_addr -= 8;

    ret = open_device();
    if(ret < 0)
    {
        printf("Error: ums_create_worker_thread() => UMS_DEVICE => Error# = %d\n", errno);
        delete((void*)(params->stack_addr - stack_size));
        delete(params);
        return -UMS_ERROR;
    }

    ret = ioctl(ums_dev, UMS_CREATE_WORKER, (unsigned long)params);
    if(ret < 0)
    {
        printf("Error: ums_create_worker_thread() => IOCTL => Error# = %d\n", errno);
        delete((void*)(params->stack_addr - stack_size));
        delete(params);
        return -UMS_ERROR;
    }

    worker = init(ums_worker_t);
    worker->wid = (ums_wid_t)ret;
    worker->state = IDLE;
    worker->worker_params = params;
    list_add_tail(&(worker->list), &workers.list);

    workers.count++;
    comp_list->worker_count++;

    return ret;
}

/** @brief Wrapper function that creates pthreds which eventually request UMS kernel module to create a scheduler
 *.
 *  UMS library uses pthread library to create process threads that will become scheduler threads.
 *  Each pthread jumps to @ref ums_enter_scheduling_mode() function and requests UMS kernel module to create a scheduler by passing @ref scheduler_params.
 *  After succesful creation of the scheduler by the UMS kernel module, created pthread becomes scheduler.
 *  It starts scheduler work by jumping to the entry point assigned by a user and stays there until @ref ums_exit_scheduling_mode() is called.
 *  Here @ref list_params is also created for the future calls of @ref ums_dequeue_completion_list_items() by a scheduler (since in this stage the completion list has been fully populated and cannot be modified later). 
 *  
 *  @param clid ID of the completion list that is assigned to the scheduler
 *  @param entry_point Function pointer and an entry point set by a user, that serves as a starting point of the scheduler. It is a scheduling function that determines the next thread to be scheduled
 *  @return returns Scheduler ID
 */
ums_sid_t ums_create_scheduler(ums_clid_t clid, void (*entry_point)())
{
    list_params_t *list;
    ums_completion_list_node_t *comp_list;
    
    comp_list = check_if_completion_list_exists(clid);
    if(comp_list == NULL)
    {
        printf("Error: ums_create_scheduler() => Completion list:%d does not exist.\n", (int)clid);
        return -UMS_ERROR;
    }
    scheduler_params_t *params;
    params = init(scheduler_params_t);
    params->entry_point = (unsigned long)entry_point;
    params->clid = clid;
    params->core_id = schedulers.count;

    ums_scheduler_t *scheduler;
    scheduler = init(ums_scheduler_t);
    scheduler->sched_params = params;
    list_add_tail(&(scheduler->list), &schedulers.list);
    schedulers.count++;

    list = create_list_params(comp_list->worker_count);
    scheduler->list_params = list;
    list->size = comp_list->worker_count;


    int ret = pthread_create(&scheduler->tid, NULL, ums_enter_scheduling_mode, (void *)scheduler->sched_params);
    if(ret < 0)
    {
        printf("Error: ums_create_scheduler() => pthread_create() => Error# = %d\n", errno);
        delete(params);
        return -UMS_ERROR;
    }

    return ret;
}

/** @brief Actual function that is called by a pthread to request the UMS kernel module in order create a scheduler and assign a completion list to it
 *.
 *  Additionally assigns a CPU core on which the scheduler will operate based on available cores
 *  
 *  @param args Pointer to @ref scheduler_params that is passed in order to create a scheduler
 *  @return 
 */
void *ums_enter_scheduling_mode(void *args)
{
    scheduler_params_t *params = (scheduler_params_t *)args;
    completion_list_id = params->clid;

    cpu_set_t set;
    long number_of_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    printf("number_of_cpus = %d\n", number_of_cpus);
    int cpu = params->core_id % number_of_cpus;
    printf("UMS_SCHEDULER_CORE_ID = %d, CPU = %d\n", params->core_id, cpu);
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    
    int ret = sched_setaffinity(getpid(), sizeof(cpu_set_t), &set);
    if(ret < 0)
    {
        printf("Error: ums_create_worker_thread() => Schedule_Affinity => Error# = %d\n", errno);
        pthread_exit(NULL);
    }
    
    ret = open_device();
    if(ret < 0)
    {
        printf("Error: ums_create_worker_thread() => UMS_DEVICE => Error# = %d\n", errno);
        pthread_exit(NULL);
    }

    ret = ioctl(ums_dev, UMS_ENTER_SCHEDULING_MODE, (unsigned long)params);
    if(ret < 0)
    {
        printf("Error: ums_enter_scheduling_mode() => IOCTL => Error# = %d\n", errno);
        pthread_exit(NULL);
    }

    pthread_exit(NULL);
}

/** @brief Called by a scheduler to signal the UMS kernel module about the completion of scheduling mode
 *.
 *  Restores instruction, stack and base pointers to return back to @ref ums_enter_scheduling_mode() function to perform pthread_exit()
 * 
 *  @return 
 */
int ums_exit_scheduling_mode()
{
    int ret = open_device();
    if(ret < 0)
    {
        printf("Error: ums_exit_scheduling_mode() => UMS_DEVICE => Error# = %d\n", errno);
        return -UMS_ERROR;
    }

    ret = ioctl(ums_dev, UMS_EXIT_SCHEDULING_MODE);
    if(ret < 0)
    {
        printf("Error: ums_exit_scheduling_mode() => IOCTL => Error# = %d\n", errno);
        return -UMS_ERROR;
    }   
    return ret;
}

/** @brief Called by a scheduler to request UMS kernel module to execute a worker thread with specific ID
 *.
 *  
 *  
 *  @param wid ID of the worker thread that to be executed
 *  @return 
 */
int ums_execute_thread(ums_wid_t wid) 
{
    list_params_t *list;
    ums_scheduler_t *scheduler;
    ums_worker_t *worker;
    printf("EXECUTING %ld\n", pthread_self());

    worker = check_if_worker_exists(wid);
    if(worker == NULL)
    {
        printf("Error: ums_execute_thread() => Worker thread:%d was not found!\n", (int)wid);
        return -UMS_ERROR;
    }

    scheduler = check_if_scheduler_exists();
    if(scheduler == NULL)
    {
        printf("Error: ums_execute_thread() => Scheduler for pthread: %ld does not exist.\n", pthread_self());
        return -UMS_ERROR;
    }

    list = scheduler->list_params;
    if(list != NULL)
    {
        int index = 0;
        while(index < list->size && list->workers[index] != wid)
        {
            ++index;
        }
        list->workers[index] = -1;
        list->worker_count--;
    }

    int ret;
    if(worker->state != IDLE)
    {
        ret = -(UMS_ERROR_WORKER_ALREADY_RUNNING | UMS_ERROR_WORKER_ALREADY_FINISHED);
        goto out;
    }

    ret = open_device();
    if(ret < 0)
    {
        printf("Error: ums_execute_thread() => UMS_DEVICE => Error# = %d\n", errno);
        return -UMS_ERROR;
    }

    scheduler->wid = wid;
    ret = ioctl(ums_dev, UMS_EXECUTE_THREAD, (unsigned long)wid);
    if(ret < 0)
    {
        printf("Error: ums_execute_thread() => IOCTL => Error# = %d\n", errno);
        return -UMS_ERROR;
    }   


    out:
    return ret;
}

/** @brief Called by a worker thread to pause or complete the execution
 *.
 *  Depending on the value of the argument, the function will:
 *   - Remove the worker thread from the list of worker threads that can be scheduled, thus completes the execution; 
 *   - Push it back to the list of available worker thread, thus pauses its' execution and can be rescheduled later.
 *  
 *  @param status defines the status of the execution flow of the worker thread (passing @c PAUSE will pause the execution, when @c FINISH will complete it)
 *  @return 
 */
int ums_thread_yield(worker_status_t status) 
{  
    ums_scheduler_t *scheduler;
    ums_worker_t *worker;

    scheduler = check_if_scheduler_exists();
    if(scheduler == NULL)
    {
        printf("Error: ums_thread_yield() => Scheduler for pthread: %ld does not exist.\n", pthread_self());
        return -UMS_ERROR;
    }

    worker = check_if_worker_exists(scheduler->wid);
    if(worker == NULL)
    {
        printf("Error: ums_thread_yield() => Worker thread:%d was not found!\n", (int)scheduler->wid);
        return -UMS_ERROR;
    }

    worker->state = (status == PAUSE) ? IDLE : FINISHED;

    int ret = open_device();
    if(ret < 0)
    {
        printf("Error: ums_thread_yield() => UMS_DEVICE => Error# = %d\n", errno);
        return -UMS_ERROR;
    }

    ret = ioctl(ums_dev, UMS_THREAD_YIELD, (unsigned long)status);
    if(ret < 0)
    {
        printf("Error: ums_thread_yield() => IOCTL => Error# = %d\n", errno);
        return -UMS_ERROR;
    }   

    return ret;
}

/** @brief Called by a worker thread to pause the execution
 *.
 *  Wrapper that calls @ref ums_thread_yield() with an argument @c PAUSE
 *
 *  @return 
 */
int ums_thread_pause()
{
    return ums_thread_yield(PAUSE);
}

/** @brief Called by a worker thread to complete the execution
 *.
 *  Wrapper that calls @ref ums_thread_yield() with an argument @c FINISH
 * 
 *  @return 
 */
int ums_thread_exit()
{
    return ums_thread_yield(FINISH);
}

/** @brief Called by a scheduler to request UMS kernel module to provide a list of available worker threads that can be scheduled
 *.
 *  The function passes a global @ref list_params from the @ref ums_completion_list_node structure to the UMS kernel module
 *  The kernel module populates the structure with the list of available workers and sets the number of those available workers.
 *  The function utilizes pthreads conditional and mutex variables to protect the shared @ref list_params.
 *  Therefore only one scheduler (usually the scheduler that is running the last available worker thread) can update the structure from the kernel module, while others wait in the blocked state for the signal from the conditional variable. 
 * 
 *  @return returns the pointer to a shared @ref list_params structure which contains an array of available workers that can be scheduled
 */
list_params_t *ums_dequeue_completion_list_items()
{
    list_params_t *list;
    ums_scheduler_t *scheduler;
    ums_completion_list_node_t *comp_list;

    scheduler = check_if_scheduler_exists();
    if(scheduler == NULL)
    {
        printf("Error: ums_dequeue_completion_list_items() => Scheduler for pthread: %ld does not exist.\n", pthread_self());
        return -UMS_ERROR;
    }

    comp_list = check_if_completion_list_exists(completion_list_id);
    if(comp_list == NULL)
    {
        printf("Error: ums_dequeue_completion_list_items() => Completion list:%d does not exist.\n", (int)completion_list_id);
        return -UMS_ERROR;
    }
            
    int ret;
    list = scheduler->list_params;
    
    printf("Dequeue: %ld\n", pthread_self());
    printf("Dequeue: count %d, state: %d\n", list->worker_count, list->state);
    if(list->worker_count == 0 && comp_list->state != FINISHED)
    { 
      
        printf("Updating the list %ld\n", pthread_self());
        goto dequeue;
    }
    else
    {
        printf("SKIPPING %ld\n", pthread_self());
        if(comp_list->state == FINISHED)
        {
            list->state = FINISHED;
        }
        goto out;
    }
    
    dequeue: ;
    ret = open_device();
    if(ret < 0)
    {
        printf("Error: ums_dequeue_completion_list_items() => UMS_DEVICE => Error# = %d\n", errno);
        return -UMS_ERROR;
    }

    ret = ioctl(ums_dev, UMS_DEQUEUE_COMPLETION_LIST_ITEMS, (unsigned long)list);
    if(ret < 0)
    {
        printf("Error: ums_dequeue_completion_list_items() => IOCTL => Error# = %d\n", errno);
        return -UMS_ERROR;
    }   
    
    if(list->state == FINISHED)
    {
        comp_list->state = FINISHED;
    }
    
    out:
    return list;
}

/** @brief Called by a scheduler, after performing @ref ums_dequeue_completion_list_items(), to find a next available worker thread from the completion list
 *.
 *  This function always has to be run after calling @ref ums_dequeue_completion_list_items(), since it will populate the list in the correct way to be processed 
 *  Passing a manually created list parameter will result in undefined behaviour
 *  
 *  @param list List parameter that is created after @ref ums_dequeue_completion_list_items() call and contains the list of available workers 
 *  @return returns a next available worker thread that can be scheduled, or error values otherwise
 */
ums_wid_t ums_get_next_worker_thread(list_params_t *list)
{
    ums_scheduler_t *scheduler;

    scheduler = check_if_scheduler_exists();
    if(scheduler == NULL)
    {
        printf("Error: ums_get_next_worker_thread() => Scheduler for pthread: %ld does not exist.\n", pthread_self());
        return -UMS_ERROR;
    }

    if(list->state == FINISHED)
    {
        printf("Error: get_next_worker_thread() => Completion list is finished!\n");
        return -UMS_ERROR_COMPLETION_LIST_ALREADY_FINISHED;
    }
    else if(list->worker_count == 0)
    {
        printf("Error: get_next_worker_thread() => No available worker threads to run!\n");
        return -UMS_ERROR_NO_AVAILABLE_WORKERS;
    }

    int index = -1;
    while(++index < list->size && list->workers[index] == -1);

    for(int i = 0; i < list->size; ++i)
    {
        printf("%d ", list->workers[i]);
    }
    printf("\n");
    return list->workers[index];
}

/** @brief Performs a cleanup by deleting all the data structures allocated by the library
 *.
 *  @return returns @c UMS_SUCCESS when succesful or @c UMS_ERROR if there are any errors 
 */
int cleanup()
{
    if(!list_empty(&completion_lists.list))
    {
        ums_completion_list_node_t *temp = NULL;
        ums_completion_list_node_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &completion_lists.list, list) 
        {
            list_del(&temp->list);
            printf("UMS_LIB: Completion list:%d was deleted.\n", temp->clid);
            delete(temp);
        }
    }
    if(!list_empty(&workers.list))
    {
        ums_worker_t *temp = NULL;
        ums_worker_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &workers.list, list) 
        {
            list_del(&temp->list);
            printf("UMS_LIB: Worker thread:%d  was deleted.\n", temp->wid);
            temp->worker_params->stack_addr += 8;
            if(temp->worker_params->stack_addr != NULL) delete((void*)(temp->worker_params->stack_addr - temp->worker_params->stack_size));
            if(temp->worker_params != NULL) delete(temp->worker_params);
            delete(temp);
        }
    }
    if(!list_empty(&schedulers.list))
    {
        ums_scheduler_t *temp = NULL;
        ums_scheduler_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &schedulers.list, list) 
        {
            list_del(&temp->list);
            printf("UMS_LIB: Scheduler:%d  was deleted.\n", temp->sched_params->sid);
            if(temp->sched_params != NULL) delete(temp->sched_params);
            if(temp->list_params != NULL) delete(temp->list_params);
            delete(temp);
        }
    }
    return UMS_SUCCESS;
}

/** @brief Checks if the completion list with a passed ID exists or not
 *.
 *
 *  @param clid Completion list ID
 *  @return returns a pointer to the existing completion list structure if it exists, NULL otherwise
 */
ums_completion_list_node_t *check_if_completion_list_exists(ums_clid_t clid)
{
    ums_completion_list_node_t *comp_list;

    if(!list_empty(&completion_lists.list))
    {
        ums_completion_list_node_t *temp = NULL;
        ums_completion_list_node_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &completion_lists.list, list) 
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

/** @brief Checks if the worker thread with a passed ID exists or not
 *.
 *
 *  @param wid Worker thread ID
 *  @return returns a @c UMS_SUCCESS if worker thread exists, @c UMS_ERROR_WORKER_NOT_FOUND otherwise
 */
ums_worker_t *check_if_worker_exists(ums_wid_t wid)
{
    ums_worker_t *worker = NULL;

    if(!list_empty(&workers.list))
    {
        ums_worker_t *temp = NULL;
        ums_worker_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &workers.list, list) 
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

/** @brief Checks if the scheduler for the current pthread exists or not
 *.
 *
 *  @return returns a pointer to the existing scheduler structure if it exists, NULL otherwise
 */
ums_scheduler_t *check_if_scheduler_exists()
{
    ums_scheduler_t *scheduler;

    if(!list_empty(&completion_lists.list))
    {
        ums_scheduler_t *temp = NULL;
        ums_scheduler_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &schedulers.list, list) 
        {
            if(pthread_equal(temp->tid, pthread_self()))
            {
                scheduler = temp;
                break;
            }
        }
    }
  
    return scheduler;
}

/** @brief 
 *.
 */
__attribute__((constructor)) void start(void)
{

}

/** @brief 
 *.
 */
__attribute__((destructor)) void end(void)
{
    cleanup();
    close_device();
}