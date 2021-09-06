#include "ums_lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>

int ums_dev = -1;
pthread_mutex_t ums_mutex = PTHREAD_MUTEX_INITIALIZER;

int ums_enter()
{
    int ret = open_device();
    if(ret < 0)
    {
        printf("Error: ums_enter() => Error# = %d\n", errno);
        return UMS_ERROR;
    }
    ret = ioctl(ums_dev, UMS_ENTER);
    if(ret < 0)
    {
        printf("Error: ums_enter() => Error# = %d\n", errno);
        return UMS_ERROR;
    }
    return ret;
}
int ums_exit()
{
    int ret = open_device();
    if(ret < 0)
    {
        printf("Error: ums_exit() => Error# = %d\n", errno);
        return UMS_ERROR;
    }

    ret = ioctl(ums_dev, UMS_EXIT);
    if(ret < 0)
    {
        printf("Error: ums_exit() => Error# = %d\n", errno);
        return UMS_ERROR;
    }   

    close_device(ums_dev);
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
            return UMS_ERROR;
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
        return UMS_ERROR;
    }
    pthread_mutex_unlock(&ums_mutex);
    return UMS_SUCCESS;
}