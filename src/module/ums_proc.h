#pragma once

#include <linux/proc_fs.h>
#include "ums_api.h"

int init_proc(void);
int delete_proc(void);


extern process_list_t process_list;

typedef struct process_proc_entry_list {
    struct list_head list;
    unsigned int process_count;
} process_proc_entry_list_t;

typedef struct process_proc_entry {
    pid_t pid;
	struct proc_dir_entry *pde;
	struct proc_dir_entry *parent;
	struct proc_dir_entry *child;
    struct list_head list;
	scheduler_proc_entry_list_t *scheduler_list;
} process_proc_entry_t;

typedef struct scheduler_proc_entry_list {
    struct list_head list;
    unsigned int scheduler_count;
} scheduler_proc_entry_list_t;

typedef struct scheduler_proc_entry {
    ums_sid_t sid;
	struct proc_dir_entry *pde;
	struct proc_dir_entry *parent;
	struct proc_dir_entry *child;
	struct proc_dir_entry *info;
    struct list_head list;
	worker_proc_entry_list_t *worker_list;
} scheduler_proc_entry_t;

typedef struct worker_proc_entry_list {
    struct list_head list;
    unsigned int worker_count;
} worker_proc_entry_list_t;

typedef struct worker_proc_entry {
    ums_wid_t wid;
	struct proc_dir_entry *pde;
	struct proc_dir_entry *parent;
    struct list_head list;
} worker_proc_entry_t;
