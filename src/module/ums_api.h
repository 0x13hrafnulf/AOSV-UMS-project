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

int enter_ums(void);
int exit_ums(void);


int delete_process(process_t *process);
process_t *create_process_node(pid_t pid);
process_t *check_if_process_exists(pid_t pid);

typedef struct process_list {
    struct list_head list;
    unsigned int process_count;
} process_list_t;

typedef struct process {
    pid_t pid;
    struct list_head list;
    //TBD
} process_t;

