#include "ums_lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>

int ums_dev = -1;
pthread_mutex_t ums_mutex = PTHREAD_MUTEX_INITIALIZER;
ums_comletion_list_t completion_lists = {
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
        printf("Error: ums_enter() => Error# = %d\n", errno);
        return -UMS_ERROR;
    }
    ret = ioctl(ums_dev, UMS_ENTER);
    if(ret < 0)
    {
        printf("Error: ums_enter() => Error# = %d\n", errno);
        return -UMS_ERROR;
    }
    return ret;
}

int ums_exit()
{
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
                return -1;
            }
        }
    }

    int ret;
    do
    {
       ret = ums_exit_helper();
    } while (ret != UMS_SUCCESS);

    return ret;
}

int ums_exit_helper()
{
    int ret = open_device();
    if(ret < 0)
    {
        printf("Error: ums_exit() => Error# = %d\n", errno);
        return -UMS_ERROR;
    }

    ret = ioctl(ums_dev, UMS_EXIT);
    if(ret < 0)
    {
        printf("Error: ums_exit() => Error# = %d\n", errno);
        return -UMS_ERROR;
    }   
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
        printf("Error: ums_create_completion_list() => Error# = %d\n", errno);
        return -1;
    }
    ret = ioctl(ums_dev, UMS_CREATE_LIST);
    if(ret < 0)
    {
        printf("Error: ums_create_completion_list() => Error# = %d\n", errno);
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
    params->stack_addr = (unsigned long)stack;

    ret = open_device();
    if(ret < 0)
    {
        printf("Error: ums_create_worker_thread() => Error# = %d\n", errno);
        delete((void*)params->stack_addr);
        delete(params);
        return -1;
    }

    ret = ioctl(ums_dev, UMS_CREATE_WORKER, (unsigned long)params);
    if(ret < 0)
    {
        printf("Error: ums_create_worker_thread() => Error# = %d\n", errno);
        delete((void*)params->stack_addr);
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
        printf("Error: ums_create_scheduler() => Error# = %d\n", errno);
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
        printf("Error: ums_create_worker_thread() => Error# = %d\n", errno);
        pthread_exit(NULL);
    }

    ret = ioctl(ums_dev, UMS_ENTER_SCHEDULING_MODE, (unsigned long)params);
    if(ret < 0)
    {
        printf("Error: ums_enter_scheduling_mode() => Error# = %d\n", errno);
        pthread_exit(NULL);
    }

    pthread_exit(NULL);
}

int ums_exit_scheduling_mode()
{
    int ret = open_device();
    if(ret < 0)
    {
        printf("Error: ums_exit_scheduling_mode() => Error# = %d\n", errno);
        return -1;
    }

    ret = ioctl(ums_dev, UMS_EXIT_SCHEDULING_MODE);
    if(ret < 0)
    {
        printf("Error: ums_exit_scheduling_mode() => Error# = %d\n", errno);
        return -1;
    }   
    return ret;
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
            delete((void*)temp->worker_params->stack_addr);
            delete(temp->worker_params);
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
            delete(temp->sched_params);
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

__attribute__((constructor)) void start(void)
{

}

__attribute__((destructor)) void end(void)
{
    close_device();
    cleanup();
}