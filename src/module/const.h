#pragma once

#include <linux/ioctl.h>

#define UMS_DEVICE          "/dev/ums"
#define UMS_IOC_MAGIC       'R'
#define UMS_IOC_DEVICE_NAME "ums_sched"
#define UMS_MODULE_NAME_LOG "ums_sched: "
#define UMS_MINOR MISC_DYNAMIC_MINOR

#define UMS_ENTER                           _IO(UMS_IOC_MAGIC, 1)
#define UMS_EXIT                            _IO(UMS_IOC_MAGIC, 2)
#define UMS_CREATE_LIST                     _IO(UMS_IOC_MAGIC, 3)

#define UMS_SUCCESS                         0
#define UMS_ERROR                           1
#define UMS_ERROR_PROCESS_NOT_FOUND         1000
#define UMS_ERROR_PROCESS_ALREADY_EXISTS    1001


typedef enum state {
    IDLE,
    RUNNING,
    FINISHED
} state_t;

typedef enum worker_status {
    PAUSE,
    FINISH
} worker_status_t;

typedef unsigned int ums_sid_t;
typedef unsigned int ums_wid_t;
typedef unsigned int ums_clid_t;

typedef struct list_params {
    unsigned int size;
    unsigned int worker_count;
    state_t state;
    ums_wid_t workers[];
} list_params_t;