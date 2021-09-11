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
ums_worker_list_t workers;

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

ums_wid_t ums_create_worker_thread(ums_clid_t list_id, unsigned long stack_size, void (*entry_point)(void *), void *args)
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
            printf("UMS_EXAMPLE: #:%d completion list was deleted.\n", temp->clid);
            //delete(list_params);
            delete(temp);
        }
    }
    return UMS_SUCCESS;
}

__attribute__((constructor)) void start(void)
{

}

__attribute__((destructor)) void end(void)
{
    close_device();
    cleanup();
}