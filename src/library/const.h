#pragma once

#include <linux/ioctl.h>

#define UMS_NAME            "ums"
#define UMS_DEVICE          "/dev/ums"
#define UMS_IOC_MAGIC       'R'

#define UMS_ENTER                           _IO(UMS_IOC_MAGIC, 1)
#define UMS_EXIT                            _IO(UMS_IOC_MAGIC, 2)
#define UMS_CREATE_LIST                     _IO(UMS_IOC_MAGIC, 3)
#define UMS_CREATE_WORKER                   _IOW(UMS_IOC_MAGIC, 4, unsigned long)
#define UMS_ENTER_SCHEDULING_MODE           _IOWR(UMS_IOC_MAGIC, 5, unsigned long)
#define UMS_EXIT_SCHEDULING_MODE            _IO(UMS_IOC_MAGIC, 6)
#define UMS_EXECUTE_THREAD                  _IOW(UMS_IOC_MAGIC, 7, unsigned long)
#define UMS_THREAD_YIELD                    _IOW(UMS_IOC_MAGIC, 8, unsigned long)
#define UMS_DEQUEUE_COMPLETION_LIST_ITEMS   _IOWR(UMS_IOC_MAGIC, 9, unsigned long)

#define UMS_SUCCESS                                         0
#define UMS_ERROR                                           1
#define UMS_ERROR_PROCESS_NOT_FOUND                         1000
#define UMS_ERROR_PROCESS_ALREADY_EXISTS                    1001
#define UMS_ERROR_COMPLETION_LIST_NOT_FOUND                 1002
#define UMS_ERROR_SCHEDULER_NOT_FOUND                       1003
#define UMS_ERROR_WORKER_NOT_FOUND                          1004
#define UMS_ERROR_STATE_RUNNING                             1005
#define UMS_ERROR_CMD_IS_NOT_ISSUED_BY_MAIN_THREAD          1006 || UMS_ERROR_PROCESS_NOT_FOUND
#define UMS_ERROR_WORKER_ALREADY_RUNNING                    1007
#define UMS_ERROR_WRONG_INPUT                               1008
#define UMS_ERROR_CMD_IS_NOT_ISSUED_BY_SCHEDULER            1009
#define UMS_ERROR_CMD_IS_NOT_ISSUED_BY_WORKER               1010
#define UMS_ERROR_WORKER_ALREADY_FINISHED                   1011
#define UMS_ERROR_NO_AVAILABLE_WORKERS                      1012
#define UMS_ERROR_COMPLETION_LIST_ALREADY_FINISHED          1013

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

typedef struct worker_params {
    unsigned long entry_point;
    unsigned long function_args; 
    unsigned long stack_size; 
    unsigned long stack_addr;
    ums_clid_t clid;
} worker_params_t;

typedef struct scheduler_params {
    unsigned long entry_point; 
    ums_clid_t clid;
    ums_sid_t sid;
    int core_id;
} scheduler_params_t;