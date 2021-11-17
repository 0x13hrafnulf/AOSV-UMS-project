/**
 * Copyright (C) 2021 Bektur Umarbaev <hrafnulf13@gmail.com>
 *
 * This file is part of the User Mode thread Scheduling (UMS) library.
 *
 * UMS library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * UMS library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with UMS library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * @brief The header that contains essential UMS library functions and has to be included by the user in order to use the UMS library
 *
 * @file ums_lib.h
 * @author Bektur Umarbaev <hrafnulf13@gmail.com>
 * @date 
 */

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
ums_worker_t check_if_worker_exists(ums_wid_t wid);
ums_scheduler_t *check_if_scheduler_exists();

/** @brief The list of the completion lists created by the process
 *.
 *
 */
typedef struct ums_completion_list {
    struct list_head list;                          
    unsigned int count;                             /**< Number of completion lists created */
} ums_completion_list_t;

/** @brief  Represents a node in the @ref ums_completion_list 
 *.
 *
 */
typedef struct ums_completion_list_node {
    ums_clid_t clid;                                /**< Completion list ID */
    pthread_t writer;                               /**< pthread that is responsible for the list updates */
    int update_required;                           /**< Update status */
    unsigned int worker_count;                      /**< Number of worker threads assigned to the completion list */
    unsigned int usage;                             /**< Number of schedulers currently using the completion list (required for shared resource update) */
    pthread_cond_t update;                          /**< Conditional variable that is signaled when the completion list is updated, while schedulers wait for an update */
    pthread_mutex_t mutex;                          /**< Mutex for the completion list, since it can be shared by the multiple schedulers */
    struct list_head list;                          
    list_params_t *list_params;                     /**< Parameters that are created by the scheduler and passed to dequeue the completion list items @ref list_params */
} ums_completion_list_node_t;

/** @brief The list of the worker threads created by the process
 *.
 *
 */
typedef struct ums_worker_list {
    struct list_head list;                          
    unsigned int count;                             /**< Number of worker threads created */
} ums_worker_list_t;

/** @brief Represents a node in the @ref ums_worker_list 
 *.
 *
 */
typedef struct ums_worker {
    ums_wid_t wid;                                  /**< Worker thread ID */
    state_t state;                                  /**< State of worker thread's progress */
    struct list_head list;                          
    worker_params_t *worker_params;                 /**< Parameters that are passed in order to create a worker thread @ref worker_params */
} ums_worker_t;

/** @brief The list of the schedulers created by the process
 *.
 *
 */
typedef struct ums_scheduler_list {
    struct list_head list;                          
    unsigned int count;                             /**< Number of scheduler created */
} ums_scheduler_list_t;

/** @brief Represents a node in the @ref ums_scheduler_list 
 *.
 *
 */
typedef struct ums_scheduler {
    struct list_head list;                          
    pthread_t tid;                                  /**< Pthread ID */
    scheduler_params_t *sched_params;               /**< Parameters that are passed in order to create a scheduler @ref scheduler_params */
    list_params_t *list_params;                     /**< Parameters that are created by the scheduler and passed to dequeue the completion list items @ref list_params */
} ums_scheduler_t;

#define init(type) (type*)malloc(sizeof(type))
#define delete(val) free(val)
#define create_list_params(size) (list_params_t*)malloc(sizeof(list_params_t) + size * sizeof(ums_wid_t))