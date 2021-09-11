#pragma once

#include "const.h"
#include "list.h"

#define UMS_DEVICE "/dev/ums"


int ums_enter();
int ums_exit();
ums_clid_t ums_create_completion_list();
ums_wid_t ums_create_worker_thread(ums_clid_t list_id, unsigned long stack_size, void (*entry_point)(void *), void *args);

int open_device();
int close_device();
int cleanup();

typedef struct ums_comletion_list ums_comletion_list_t;
typedef struct ums_completion_list_node ums_completion_list_node_t;
typedef struct ums_worker ums_worker_t;
typedef struct ums_worker_list ums_worker_list_t;

typedef struct ums_comletion_list {
    struct list_head list;
    unsigned int count;
} ums_comletion_list_t;

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


#define init(type) (type*)malloc(sizeof(type))
#define delete(val) free(val)
