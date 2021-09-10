#pragma once

#include "const.h"
#include "list.h"

#define UMS_DEVICE "/dev/ums"


int ums_enter();
int ums_exit();
ums_clid_t ums_create_completion_list();


int open_device();
int close_device();
int cleanup();


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

#define init(type) (type*)malloc(sizeof(type))
#define delete(val) free(val)
