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
 * @brief Set of data structures and other constant variables used by UMS library
 *
 * @file const.h
 * @author Bektur Umarbaev <hrafnulf13@gmail.com>
 * @date 
 */

#pragma once

#include <linux/ioctl.h>

/*
 * Definitions
 */

#define UMS_NAME            "ums"
#define UMS_DEVICE          "/dev/ums"
#define UMS_IOC_MAGIC       'R'

/*
 * IOCTL definitions
 */
#define UMS_ENTER                           _IO(UMS_IOC_MAGIC, 1)
#define UMS_EXIT                            _IO(UMS_IOC_MAGIC, 2)
#define UMS_CREATE_LIST                     _IO(UMS_IOC_MAGIC, 3)
#define UMS_CREATE_WORKER                   _IOW(UMS_IOC_MAGIC, 4, unsigned long)
#define UMS_ENTER_SCHEDULING_MODE           _IOWR(UMS_IOC_MAGIC, 5, unsigned long)
#define UMS_EXIT_SCHEDULING_MODE            _IO(UMS_IOC_MAGIC, 6)
#define UMS_EXECUTE_THREAD                  _IOW(UMS_IOC_MAGIC, 7, unsigned long)
#define UMS_THREAD_YIELD                    _IOW(UMS_IOC_MAGIC, 8, unsigned long)
#define UMS_DEQUEUE_COMPLETION_LIST_ITEMS   _IOWR(UMS_IOC_MAGIC, 9, unsigned long)

/*
 * Errors and return values
 */
#define UMS_SUCCESS                                                     0                                           ///< 
#define UMS_ERROR                                                       1                                           ///<
#define UMS_ERROR_PROCESS_NOT_FOUND                                     1000                                        ///<
#define UMS_ERROR_PROCESS_ALREADY_EXISTS                                1001                                        ///<
#define UMS_ERROR_COMPLETION_LIST_NOT_FOUND                             1002                                        ///<
#define UMS_ERROR_SCHEDULER_NOT_FOUND                                   1003                                        ///<
#define UMS_ERROR_WORKER_NOT_FOUND                                      1004                                        ///<
#define UMS_ERROR_STATE_RUNNING                                         1005                                        ///<
#define UMS_ERROR_CMD_IS_NOT_ISSUED_BY_MAIN_THREAD                      1006 || UMS_ERROR_PROCESS_NOT_FOUND         ///<
#define UMS_ERROR_WORKER_ALREADY_RUNNING                                1007                                        ///<
#define UMS_ERROR_WRONG_INPUT                                           1008                                        ///<
#define UMS_ERROR_CMD_IS_NOT_ISSUED_BY_SCHEDULER                        1009                                        ///<
#define UMS_ERROR_CMD_IS_NOT_ISSUED_BY_WORKER                           1010                                        ///<
#define UMS_ERROR_WORKER_ALREADY_FINISHED                               1011                                        ///<
#define UMS_ERROR_NO_AVAILABLE_WORKERS                                  1012                                        ///<
#define UMS_ERROR_COMPLETION_LIST_ALREADY_FINISHED                      1013                                        ///<
#define UMS_ERROR_FAILED_TO_CREATE_PROC_ENTRY                           1014                                        ///<
#define UMS_ERROR_FAILED_TO_PROC_OPEN                                   1015                                        ///<
#define UMS_ERROR_COMPLETION_LIST_IS_USED_AND_CANNOT_BE_MODIFIED        1016

/** @brief The minimum stack size of the worker thread
 *.
 *
 */
#define UMS_MIN_STACK_SIZE                                      4096

/** @brief States of processes, completion lists and threads (schedulers, worker threads)
 *.
 *  
 */
typedef enum state {
    IDLE,                           /**< Represents the state when worker thread is waiting to be scheduled; When scheduler waits or searches for available worker threads to run; Completion list has available worker threads to be scheduled */
    RUNNING,                        /**< Represents the state when worker thread is scheduled and ran by the scheduler; When scheduler handles worker thread; Completion list is currently used and can't be modified */
    FINISHED                        /**< Represents the state when worker thread has been completed; When scheduler has completed all scheduling work with a completion list; All completion list's worker threads has been completed */
} state_t;

/** @brief Status of the worker thread
 *.
 *  Used as a parameter that is passed for pausing or completing the worker thread
 */
typedef enum worker_status {
    PAUSE,                          /**< Used for pausing a worker thread: ums_thread_pause() == ums_thread_yield(PAUSE) */
    FINISH                          /**< Used for completing a worker thread: ums_thread_exit() == ums_thread_yield(FINISH) */
} worker_status_t;

/** @brief Scheduler ID
 *.
 *
 */
typedef unsigned int ums_sid_t;

/** @brief Worker thread ID
 *.
 *
 */
typedef unsigned int ums_wid_t;

/** @brief Completion list ID
 *.
 *
 */
typedef unsigned int ums_clid_t;

/** @brief Parameters that are created by the scheduler and passed to dequeue the completion list items
 *.
 *
 */
typedef struct list_params {
    unsigned int size;              /**< Size of the worker thread array */
    unsigned int worker_count;      /**< Tracks the quantity of the available workers and used as state indicator for scheduler to perform a new dequeue call */
    state_t state;                  /**< Tracks the state of the completion list which is set by the kernel module after a dequeue call */
    ums_wid_t workers[];            /**< Array of worker threads. Stores ID of worker threads in case they are available to be scheduled (when worker thread is finished, scheduler replaces ID with -1 value) */
} list_params_t;

/** @brief Parameters that are passed in order to create a worker thread
 *.
 *
 */
typedef struct worker_params {
    unsigned long entry_point;      /**< Function pointer and an entry point set by a user, that serves as a starting point of the worker thread  */
    unsigned long function_args;    /**< Pointer of the function arguments that are passed to the entry point/function */
    unsigned long stack_size;       /**< Stack size of the worker thread set by a user */
    unsigned long stack_addr;       /**< Address of the stack allocated by the UMS library */
    ums_clid_t clid;                /**< ID of the completion list where worker thread is assigned to */
} worker_params_t;

/** @brief Parameters that are passed in order to create a scheduler
 *.
 *
 */
typedef struct scheduler_params {
    unsigned long entry_point;      /**< Function pointer and an entry point set by a user, that serves as a starting point of the scheduler. It is a scheduling function that determines the next thread to be scheduled */
    ums_clid_t clid;                /**< ID of the completion list that is assigned to the scheduler */
    ums_sid_t sid;                  /**< ID of the scheduler which is set by the kernel module */
    int core_id;                    /**< ID of the CPU core that is assigned to the scheduler (It is handled automatically by the library, no user input required) */
} scheduler_params_t;