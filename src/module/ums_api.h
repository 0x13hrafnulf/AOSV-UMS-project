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
typedef struct worker_list worker_list_t;
typedef struct worker worker_list_t;

int enter_ums(void);
int exit_ums(void);
ums_clid_t create_completion_list(void);
ums_wid_t create_worker_thread(worker_params_t *params);
completion_list_node_t *check_if_completion_list_exists(process_t *proc, ums_clid_t clid);


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
    worker_list_t *worker_list;
    //TBA scheduler to track them 
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
    worker_list_t *ready_list;  
    worker_list_t *busy_list;
} completion_list_node_t;

typedef struct worker_list {
    struct list_head list;
    unsigned int worker_count;
} worker_list_t;

typedef struct worker {
    ums_wid_t wid;
    pid_t pid;
    tid_t tid;
    ums_sid_t sid;
    unsigned long entry_point;
    unsigned long stack_addr; 
    struct pt_regs regs;
    struct fpu fpu_regs;
    struct list_head global_list;
    struct list_head local_list;
    state_t state;
    unsigned int switch_count;
    unsigned long total_exec_time;
    struct timespec time_of_the_last_switch;
} worker_t;

