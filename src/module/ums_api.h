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

/** @brief The list of the processes handled by the UMS kernel module
 *.
 *
 */
typedef struct process_list {
    struct list_head list;          
    unsigned int process_count;     /**< Number of processes handled by the UMS kernel module*/
} process_list_t;

/** @brief Represents a node in the @ref process_list 
 *.
 *
 */
typedef struct process {
    pid_t pid;                              /**< pid of the process or tgid of the process threads */
    struct list_head list;                  
    state_t state;                          /**< State of the process */
    completion_list_t *completion_lists;    /**< List of completions lists created by the process */
    worker_list_t *worker_list;             /**< List of worker threads created by the process  */
    scheduler_list_t *scheduler_list;       /**< List of schedulers created by the process  */
    process_proc_entry_t *proc_entry;       /**< Proc entries of the process */
} process_t;

/** @brief The list of the completion lists created by the specific process
 *.
 *
 */
typedef struct completion_list {
    struct list_head list;          
    unsigned int list_count;        /**< Number of completion lists created by the process */
} completion_list_t;

/** @brief Represents a node in the @ref completion_list
 *.
 *
 */
typedef struct completion_list_node {
    ums_clid_t clid;                /**< Completion list ID */
    struct list_head list;          
    unsigned int worker_count;      /**< Number of worker threads assigned to the completion list */
    unsigned int finished_count;    /**< Number of worker threads that has completed their work */
    state_t state;                  /**< State of the completion list */
    worker_list_t *idle_list;       /**< List of worker threads that are ready and waiting to be scheduled */
    worker_list_t *busy_list;       /**< List of worker threads that has been completed or currently running */
} completion_list_node_t;

/** @brief The list of the worker threads
 *.
 *
 */
typedef struct worker_list {
    struct list_head list;          
    unsigned int worker_count;      /**< Number of worker threads created by the process */
} worker_list_t;

/** @brief Represents a node in the @ref worker_list
 *.
 *
 */
typedef struct worker {
    ums_wid_t wid;                                      /**< Worker thread ID */
    pid_t pid;                                          /**< pid of the process thread that is currently running the worker thread */
    pid_t tid;                                          /**< pid of the process that created the worker thread */
    ums_sid_t sid;                                      /**< ID of the scheduler which manages the current worker thread */
    ums_clid_t clid;                                    /**< ID of the completion list where worker thread is assigned to */
    unsigned long entry_point;                          /**< Function pointer and an entry point set by a user, that serves as a starting point of the worker thread  * */
    unsigned long stack_addr;                           /**< Address of the stack allocated by the UMS library */
    struct pt_regs regs;                                /**< Snapshot of CPU registers */
    struct fpu fpu_regs;                                /**< Snapshot of FPU registers */
    struct list_head global_list;                       /**< List of the worker threads created by the process */
    struct list_head local_list;                        /**< List of the worker threads of the completion list */
    state_t state;                                      /**< State of worker thread's progress */
    worker_proc_entry_t *proc_entry;                    /**< Proc entry of the worker thread */
    unsigned int switch_count;                          /**< Number of context switches */
    unsigned long total_exec_time;                      /**< Total execution time of the worker thread */
    struct timespec64 time_of_the_last_switch;          /**< Time when the last switch occured */
} worker_t;

/** @brief The list of the schedulers created by the specific process
 *.
 *
 */
typedef struct scheduler_list {
    struct list_head list;              
    unsigned int scheduler_count;       /**< Number of schedulers created by the process */
} scheduler_list_t;

/** @brief Represents a node in the @ref scheduler_list
 *.
 *
 */
typedef struct scheduler {
    ums_sid_t sid;                                              /**< Scheduler ID */
    pid_t pid;                                                  /**< pid of the process thread that is currently running the scheduler */
    pid_t tid;                                                  /**< pid of the process that created the scheduler */
    ums_wid_t wid;                                              /**< ID of the worker that is managed by the scheduler */
	unsigned long entry_point;                                  /**< Function pointer and an entry point set by a user, that serves as a starting point of the scheduler. It is a scheduling function that determines the next thread to be scheduled */
    unsigned long return_addr;                                  /**< Snapshot of the instruction pointer that is restored when exiting scheduling mode */
    unsigned long stack_ptr;                                    /**< Snapshot of the stack pointer that is restored when exiting scheduling mode */
    unsigned long base_ptr;                                     /**< Snapshot of the base pointer that is restored when exiting scheduling mode */
    state_t state;                                              /**< State of the scheduler */
    struct pt_regs regs;                                        /**< Snapshot of CPU registers */
    struct fpu fpu_regs;                                        /**< Snapshot of FPU registers */
    completion_list_node_t *comp_list;                          /**< Pointer of the completion list that is associated with the scheduler */
    struct list_head list;                                      
    scheduler_proc_entry_t *proc_entry;                         /**< Proc entry of the scheduler */
    unsigned int switch_count;                                  /**< Number of context switches */
    unsigned long avg_switch_time;                              /**< Average time needed for context switch */
    unsigned long time_needed_for_the_last_switch;              /**< Time needed for the last context switch */
    unsigned long total_time_needed_for_the_switch;         /**< Time needed for the last context switch, including the time for finding available worker thread */
    struct timespec64 time_of_the_last_switch;                  /**< Time when the last switch occured */
} scheduler_t;

/** @brief Responsible for tracking proc_dir_entries of the process
 *.
 *
 */
typedef struct process_proc_entry {
	struct proc_dir_entry *pde;         /**< proc_dir_entry of the process (/proc/ums/<PID>/) */
	struct proc_dir_entry *parent;      /**< Parent folder of the process' folder (/proc/ums/) */
	struct proc_dir_entry *child;       /**< Child folder of the process' folder (/proc/ums/<PID>/schedulers/) */
} process_proc_entry_t;

/** @brief Responsible for tracking proc_dir_entries of the schedulers of the specific process
 *.
 *
 */
typedef struct scheduler_proc_entry {
	struct proc_dir_entry *pde;     /**< proc_dir_entry of the scheduler of the specific process (/proc/ums/<PID>/schedulers/<Scheduler ID>/) */                             
	struct proc_dir_entry *parent;  /**< Parent folder of the scheduler's folder (/proc/ums/<PID>/schedulers/) */
	struct proc_dir_entry *child;   /**< Child folder of the scheduler's folder (/proc/ums/<PID>/schedulers/<Scheduler ID>/workers/) */
	struct proc_dir_entry *info;    /**< File that contains information and statistics of the scheduler performance (/proc/ums/<PID>/schedulers/<Scheduler ID>/info) */
} scheduler_proc_entry_t;

/** @brief Responsible for tracking proc_dir_entries of the worker of the specific process
 *.
 *
 */
typedef struct worker_proc_entry {
	struct proc_dir_entry *pde;     /**< proc_dir_entry of the worker thread of the completion list that is assigned to a specific scheduler and contains information/statistics regarding worker thread's performance (/proc/ums/<PID>/schedulers/<Scheduler ID>/workers/<Worker ID>) */
	struct proc_dir_entry *parent;  /**< Parent folder of the worker thread's file (/proc/ums/<PID>/schedulers/<Scheduler ID>/workers/) */
} worker_proc_entry_t;