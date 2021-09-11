#pragma once

#include "const.h"

#include <asm/current.h>
#include <asm/fpu/internal.h>
#include <asm/fpu/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/slab.h>	
#include <linux/types.h>

extern spinlock_t spinlock_ums;
extern unsigned long spinlock_flags_ums;

typedef struct process_list process_list_t;
typedef struct process process_t;
typedef struct completion_list completion_list_t;
typedef struct completion_list_node completion_list_node_t;

int enter_ums(void);
int exit_ums(void);
ums_clid_t create_completion_list(void);
ums_wid_t create_worker_thread(worker_params_t *params);

int delete_process(process_t *process);
int delete_completion_lists(process_t *proc);
process_t *create_process_node(pid_t pid);
process_t *check_if_process_exists(pid_t pid);

typedef struct process_list {
    struct list_head list;
    unsigned int process_count;
} process_list_t;

typedef struct process {
    pid_t pid;
    struct list_head list;
    completion_list_t *completion_lists;
    //TBA scheduler and worker list to track them 
} process_t;

typedef struct completion_list {
    struct list_head list;
    unsigned int list_count;
} completion_list_t;

typedef struct completion_list_node {
    ums_clid_t clid;
    struct list_head list;
    unsigned int worker_count;
    unsigned int finished_count;
    //TBA worker lists
} completion_list_node_t;



