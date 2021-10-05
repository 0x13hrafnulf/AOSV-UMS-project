#pragma once

#include "const.h"
#include "list.h"
#include <pthread.h>

#define UMS_DEVICE "/dev/ums"

typedef struct ums_completion_list ums_completion_list_t;
typedef struct ums_completion_list_node ums_completion_list_node_t;
typedef struct ums_worker ums_worker_t;
typedef struct ums_worker_list ums_worker_list_t;
typedef struct ums_scheduler_list ums_scheduler_list_t;
typedef struct ums_scheduler ums_scheduler_t;

int ums_enter();
int ums_exit();

ums_clid_t ums_create_completion_list();
ums_wid_t ums_create_worker_thread(ums_clid_t clid, unsigned long stack_size, void (*entry_point)(void *), void *args);
ums_sid_t ums_create_scheduler(ums_clid_t clid, void (*entry_point)(void *));
void *ums_enter_scheduling_mode(void *args);
int ums_exit_scheduling_mode();
int ums_execute_thread(ums_wid_t wid);
int ums_thread_yield();
int ums_thread_pause();
int ums_thread_exit();
list_params_t *ums_dequeue_completion_list_items();
ums_wid_t ums_get_next_worker_thread(list_params_t *list);

int open_device();
int close_device();
int cleanup();
ums_completion_list_node_t *check_if_completion_list_exists(ums_clid_t clid);

typedef struct ums_completion_list {
    struct list_head list;
    unsigned int count;
} ums_completion_list_t;

typedef struct ums_completion_list_node {
    ums_clid_t clid;
    unsigned int worker_count;
    struct list_head list;
    list_params_t *list_params;
} ums_completion_list_node_t;

typedef struct ums_worker_list {
    struct list_head list;
    unsigned int count;
} ums_worker_list_t;

typedef struct ums_worker {
    ums_wid_t wid;
    struct list_head list;
    worker_params_t *worker_params;
} ums_worker_t;

typedef struct ums_scheduler_list {
    struct list_head list;
    unsigned int count;
} ums_scheduler_list_t;

typedef struct ums_scheduler {
    struct list_head list;
    pthread_t tid;
    scheduler_params_t *sched_params;
} ums_scheduler_t;

#define init(type) (type*)malloc(sizeof(type))
#define delete(val) free(val)
#define create_list_params(size) (list_params_t*)malloc(sizeof(list_params_t) + size * sizeof(ums_wid_t))