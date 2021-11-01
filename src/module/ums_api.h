/**
 * Copyright (C) 2021 Bektur Umarbaev <hrafnulf13@gmail.com>
 *
 * This file is part of the User Mode thread Scheduling (UMS) kernel module.
 *
 * UMS kernel module is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * UMS kernel module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with UMS kernel module.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * @brief The header that contains essential functions, data structures and proc filesystem of the UMS kernel module 
 *
 * @file ums_api.h
 * @author Bektur Umarbaev <hrafnulf13@gmail.com>
 * @date 
 */
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
#include <linux/time.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

typedef struct process_list process_list_t;
typedef struct process process_t;
typedef struct completion_list completion_list_t;
typedef struct completion_list_node completion_list_node_t;
typedef struct worker_list worker_list_t;
typedef struct worker worker_t;
typedef struct scheduler_list scheduler_list_t;
typedef struct scheduler scheduler_t;

typedef struct process_proc_entry  process_proc_entry_t;
typedef struct scheduler_proc_entry scheduler_proc_entry_t;
typedef struct worker_proc_entry worker_proc_entry_t;

int enter_ums(void);
int exit_ums(void);
ums_clid_t create_completion_list(void);
ums_wid_t create_worker_thread(worker_params_t *params);
ums_sid_t enter_scheduling_mode(scheduler_params_t *params);
int exit_scheduling_mode(void);
int execute_thread(ums_wid_t worker_id);
int thread_yield(worker_status_t status);
int dequeue_completion_list_items(list_params_t *params);
int delete_process(process_t *process);
int delete_completion_lists_and_worker_threads(process_t *process);
int delete_workers_from_completion_list(worker_list_t *worker_list);
int delete_workers_from_process_list(worker_list_t *worker_list);
int delete_schedulers(process_t *process);
process_t *create_process_node(pid_t pid);
process_t *check_if_process_exists(pid_t pid);
completion_list_node_t *check_if_completion_list_exists(process_t *proc, ums_clid_t clid);
scheduler_t *check_if_scheduler_exists(process_t *proc, ums_sid_t sid);
scheduler_t *check_if_scheduler_exists_run_by(process_t *process, pid_t pid);
worker_t *check_if_worker_exists(worker_list_t *worker_list, ums_wid_t wid);
worker_t *check_if_worker_exists_global(worker_list_t *worker_list, ums_wid_t wid);
state_t check_if_schedulers_state(process_t *proc);
unsigned long get_exec_time(struct timespec64 *prev_time);
int cleanup(void);

int init_proc(void);
int delete_proc(void);
int create_process_proc_entry(process_t *process);
int create_scheduler_proc_entry(process_t *process, scheduler_t *scheduler);
int create_worker_proc_entry(process_t *process, scheduler_t *scheduler, worker_t *worker);
int delete_process_proc_entry(process_t *process);

/** @brief 
 *.
 *
 */
typedef struct process_list {
    struct list_head list;          /**<  */
    unsigned int process_count;     /**<  */
} process_list_t;

/** @brief 
 *.
 *
 */
typedef struct process {
    pid_t pid;                              /**<  */
    struct list_head list;                  /**<  */
    state_t state;                          /**<  */
    completion_list_t *completion_lists;    /**<  */
    worker_list_t *worker_list;             /**<  */
    scheduler_list_t *scheduler_list;       /**<  */
    process_proc_entry_t *proc_entry;       /**<  */
} process_t;

/** @brief 
 *.
 *
 */
typedef struct completion_list {
    struct list_head list;          /**<  */
    unsigned int list_count;        /**<  */
} completion_list_t;

/** @brief 
 *.
 *
 */
typedef struct completion_list_node {
    ums_clid_t clid;                /**<  */
    struct list_head list;          /**<  */
    unsigned int worker_count;      /**<  */
    unsigned int finished_count;    /**<  */
    worker_list_t *idle_list;       /**<  */
    worker_list_t *busy_list;       /**<  */
} completion_list_node_t;

/** @brief 
 *.
 *
 */
typedef struct worker_list {
    struct list_head list;          /**<  */
    unsigned int worker_count;      /**<  */
} worker_list_t;

/** @brief 
 *.
 *
 */
typedef struct worker {
    ums_wid_t wid;                                      /**<  */
    pid_t pid;                                          /**<  */
    pid_t tid;                                          /**<  */
    ums_sid_t sid;                                      /**<  */
    ums_clid_t clid;                                    /**<  */
    unsigned long entry_point;                          /**<  */
    unsigned long stack_addr;                           /**<  */
    struct pt_regs regs;                                /**<  */
    struct fpu fpu_regs;                                /**<  */
    struct list_head global_list;                       /**<  */
    struct list_head local_list;                        /**<  */
    state_t state;                                      /**<  */
    worker_proc_entry_t *proc_entry;                    /**<  */
    unsigned int switch_count;                          /**<  */
    unsigned long total_exec_time;                      /**<  */
    struct timespec64 time_of_the_last_switch;          /**<  */
} worker_t;

/** @brief 
 *.
 *
 */
typedef struct scheduler_list {
    struct list_head list;              /**<  */
    unsigned int scheduler_count;       /**<  */
} scheduler_list_t;

/** @brief 
 *.
 *
 */
typedef struct scheduler {
    ums_sid_t sid;                                              /**<  */
    pid_t pid;                                                  /**<  */
    pid_t tid;                                                  /**<  */
    ums_wid_t wid;                                              /**<  */
	unsigned long entry_point;                                  /**<  */
    unsigned long return_addr;                                  /**<  */
    unsigned long stack_ptr;                                    /**<  */
    unsigned long base_ptr;                                     /**<  */
    state_t state;                                              /**<  */
    struct pt_regs regs;                                        /**<  */
    struct fpu fpu_regs;                                        /**<  */
    completion_list_node_t *comp_list;                          /**<  */
    struct list_head list;                                      /**<  */
    scheduler_proc_entry_t *proc_entry;                         /**<  */
    unsigned int switch_count;                                  /**<  */
    unsigned long avg_switch_time;                              /**<  */
    unsigned long avg_switch_time_full;                         /**<  */
    unsigned long time_needed_for_the_last_switch;              /**<  */
    unsigned long time_needed_for_the_last_switch_full;         /**<  */
    struct timespec64 time_of_the_last_switch;                  /**<  */
    struct timespec64 time_of_the_last_switch_full;             /**<  */
} scheduler_t;

/** @brief 
 *.
 *
 */
typedef struct process_proc_entry {
	struct proc_dir_entry *pde;         /**<  */
	struct proc_dir_entry *parent;      /**<  */
	struct proc_dir_entry *child;       /**<  */
} process_proc_entry_t;

/** @brief 
 *.
 *
 */
typedef struct scheduler_proc_entry {
	struct proc_dir_entry *pde;     /**<  */                             
	struct proc_dir_entry *parent;  /**<  */
	struct proc_dir_entry *child;   /**<  */
	struct proc_dir_entry *info;    /**<  */
} scheduler_proc_entry_t;

/** @brief 
 *.
 *
 */
typedef struct worker_proc_entry {
	struct proc_dir_entry *pde;     /**<  */
	struct proc_dir_entry *parent;  /**<  */
} worker_proc_entry_t;