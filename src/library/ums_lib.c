#include "ums_lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>

int ums_dev = -1;
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

int ums_enter()
{
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
        return -UMS_ERROR;
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

int open_device()
{
    pthread_mutex_lock(&ums_mutex);
    if(ums_dev < 0)
    {
        ums_dev = open(UMS_DEVICE, O_RDWR);
        if(ums_dev < 0) 
        {
            printf("Error: open_device() => Error# = %d\n", errno);
            return -UMS_ERROR;
	    }
    }
    pthread_mutex_unlock(&ums_mutex);
    return UMS_SUCCESS;
}

int close_device()
{
    pthread_mutex_lock(&ums_mutex);
    ums_dev = close(ums_dev);
    if(ums_dev < 0) 
    {
        printf("Error: close_device() => Error# = %d\n", errno);
        return -UMS_ERROR;
    }
    pthread_mutex_unlock(&ums_mutex);
    return UMS_SUCCESS;
}

ums_clid_t ums_create_completion_list() 
{
    int ret = open_device();
    if(ret < 0)
    {
        printf("Error: ums_create_completion_list() => UMS_DEVICE => Error# = %d\n", errno);
        return -1;
    }
    ret = ioctl(ums_dev, UMS_CREATE_LIST);
    if(ret < 0)
    {
        printf("Error: ums_create_completion_list()  => IOCTL => Error# = %d\n", errno);
        return -1;
    }
    ums_completion_list_node_t *comp_list;
    comp_list = init(ums_completion_list_node_t);
    
    comp_list->clid = (ums_clid_t)ret;
    comp_list->worker_count = 0;
    comp_list->list_params = NULL;
    list_add_tail(&(comp_list->list), &completion_lists.list);

    return ret;
}

ums_wid_t ums_create_worker_thread(ums_clid_t clid, unsigned long stack_size, void (*entry_point)(void *), void *args)
{
    ums_worker_t *worker;
    ums_completion_list_node_t *comp_list;
    comp_list = check_if_completion_list_exists(clid);
    if(comp_list == NULL)
    {
        printf("Error: ums_create_worker_thread() => Completion list:%d does not exist.\n", (int)clid);
        return -1;
    }

    worker_params_t *params;
    params = init(worker_params_t);

    params->entry_point = (unsigned long)entry_point;
    params->function_args = (unsigned long)args;
    params->stack_size = stack_size;
    params->clid = clid;

    void *stack;
    int ret = posix_memalign(&stack, 16, stack_size);
    if(ret < 0)
    {
        printf("Error: ums_create_worker_thread() => Error# = %d\n", errno);
        delete(params);
        return -1;
    }

    params->stack_addr = (unsigned long)stack + stack_size;
    //((unsigned long *)params->stack_addr)[0] = (unsigned long)&ums_thread_exit;
    //params->stack_addr -= 8;

    ret = open_device();
    if(ret < 0)
    {
        printf("Error: ums_create_worker_thread() => UMS_DEVICE => Error# = %d\n", errno);
        delete((void*)(params->stack_addr - stack_size));
        delete(params);
        return -1;
    }

    ret = ioctl(ums_dev, UMS_CREATE_WORKER, (unsigned long)params);
    if(ret < 0)
    {
        printf("Error: ums_create_worker_thread() => IOCTL => Error# = %d\n", errno);
        delete((void*)(params->stack_addr - stack_size));
        delete(params);
        return -1;
    }

    worker = init(ums_worker_t);
    worker->wid = (ums_wid_t)ret;
    worker->worker_params = params;
    list_add_tail(&(worker->list), &workers.list);

    workers.count++;
    comp_list->worker_count++;

    return ret;
}

ums_sid_t ums_create_scheduler(ums_clid_t clid, void (*entry_point)())
{
    ums_completion_list_node_t *comp_list;
    comp_list = check_if_completion_list_exists(clid);
    if(comp_list == NULL)
    {
        printf("Error: ums_create_scheduler() => Completion list:%d does not exist.\n", (int)clid);
        return -1;
    }
    scheduler_params_t *params;
    params = init(scheduler_params_t);
    params->entry_point = (unsigned long)entry_point;
    params->clid = clid;

    ums_scheduler_t *scheduler;
    scheduler = init(ums_scheduler_t);
    scheduler->sched_params = params;
    list_add_tail(&(scheduler->list), &schedulers.list);

    int ret = pthread_create(&scheduler->tid, NULL, ums_enter_scheduling_mode, (void *)scheduler->sched_params);
    if(ret < 0)
    {
        printf("Error: ums_create_scheduler() => pthread_create() => Error# = %d\n", errno);
        delete(params);
        return -1;
    }

    return ret;
}

void *ums_enter_scheduling_mode(void *args)
{
    scheduler_params_t *params = (scheduler_params_t *)args;
    completion_list_id = params->clid;

    int ret = open_device();
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

int ums_exit_scheduling_mode()
{
    int ret = open_device();
    if(ret < 0)
    {
        printf("Error: ums_exit_scheduling_mode() => UMS_DEVICE => Error# = %d\n", errno);
        return -1;
    }

    ret = ioctl(ums_dev, UMS_EXIT_SCHEDULING_MODE);
    if(ret < 0)
    {
        printf("Error: ums_exit_scheduling_mode() => IOCTL => Error# = %d\n", errno);
        return -1;
    }   
    return ret;
}

int ums_execute_thread(ums_wid_t wid) 
{
    
    if(check_if_worker_exists(wid) == UMS_ERROR_WORKER_NOT_FOUND)
    {
        printf("Error: ums_execute_thread() => Worker thread:%d was not found!\n", (int)wid);
        return -1;
    }

    int ret = open_device();
    if(ret < 0)
    {
        printf("Error: ums_execute_thread() => Error# = %d\n", errno);
        return -1;
    }

    ret = ioctl(ums_dev, UMS_EXECUTE_THREAD, (unsigned long)wid);
    if(ret < 0)
    {
        printf("Error: ums_execute_thread() => Error# = %d\n", errno);
        return -1;
    }   

    return ret;
}

int ums_thread_yield(worker_status_t status) 
{
    int ret = open_device();
    if(ret < 0)
    {
        printf("Error: ums_execute_thread() => Error# = %d\n", errno);
        return -1;
    }

    ret = ioctl(ums_dev, UMS_THREAD_YIELD, (unsigned long)status);
    if(ret < 0)
    {
        printf("Error: ums_execute_thread() => Error# = %d\n", errno);
        return -1;
    }   

    return ret;
}

int ums_thread_pause()
{
    return ums_thread_yield(PAUSE);
}

int ums_thread_exit()
{
    return ums_thread_yield(FINISH);
}

list_params_t *ums_dequeue_completion_list_items()
{

}

ums_wid_t ums_get_next_worker_thread(list_params_t *list)
{
    
}

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
            if(temp->list_params != NULL) delete(temp->list_params);
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
            delete(temp);
        }
    }
    return UMS_SUCCESS;
}

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

int check_if_worker_exists(ums_wid_t wid)
{
    if(!list_empty(&workers.list))
    {
        ums_worker_t *temp = NULL;
        ums_worker_t *safe_temp = NULL;
        list_for_each_entry_safe(temp, safe_temp, &workers.list, list) 
        {
            if(temp->wid == wid)
            {
                return UMS_SUCCESS;
            }
        }
    }
  
    return UMS_ERROR_WORKER_NOT_FOUND;
}

__attribute__((constructor)) void start(void)
{

}

__attribute__((destructor)) void end(void)
{
    cleanup();
    close_device();
}