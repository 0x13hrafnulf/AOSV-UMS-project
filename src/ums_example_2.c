#include "ums_lib.h"

#define STACK_SIZE 4096

void loop1()
{
    printf("- UMS_EXAMPLE_%s\n", __FUNCTION__);
    

    list_params_t *ready_list = ums_dequeue_completion_list_items();
    ums_wid_t worker_id = ums_get_next_worker_thread(ready_list);
    while(ready_list->state != FINISHED)
    {
        printf("-- UMS_EXAMPLE_LOOP: Worker = %d\n", (int)worker_id);
        ums_execute_thread(worker_id);
        ready_list = ums_dequeue_completion_list_items();
        worker_id = ums_get_next_worker_thread(ready_list);
    }

    ums_exit_scheduling_mode();
}

void loop2()
{
    printf("- UMS_EXAMPLE_%s\n", __FUNCTION__);
    list_params_t *ready_list = ums_dequeue_completion_list_items();
    ums_wid_t worker_id = ums_get_next_worker_thread(ready_list);
    printf("-- UMS_EXAMPLE_LOOP: Worker = %d\n", (int)worker_id);
    ums_execute_thread(worker_id);
    ums_exit_scheduling_mode();
}

void function1(void *args)
{
    printf("- UMS_EXAMPLE_%s\n", __FUNCTION__);
    printf("-- UMS_EXAMPLE: args = %d\n", *(int*)args);
    printf("-- pthread_id = %ld\n", pthread_self());
    int check = 5;
    for(int i = 0; i < 5; ++i)
    {
        getpid();
    }
    ums_thread_pause();
    printf("- UMS_EXAMPLE_Continue_%s\n", __FUNCTION__);
    printf("-- UMS_EXAMPLE: args = %d\n", *(int*)args);
    printf("-- pthread_id = %ld\n", pthread_self());
    printf("-- Check = %d\n", check);
    for(int i = 0; i < 5; ++i)
    {
        getpid();
    }
    ums_thread_exit();
}

void function2(void *args)
{
    printf("- UMS_EXAMPLE_%s\n", __FUNCTION__);
    printf("-- UMS_EXAMPLE: args = %d\n", *(int*)args);
    printf("-- pthread_id = %ld\n", pthread_self());
    for(int i = 0; i < 5; ++i)
    {
       getpid();
    }
    ums_thread_exit();
}

int main()
{
    ums_enter();
    ums_clid_t comp_list1 =  ums_create_completion_list();
    ums_clid_t comp_list2 =  ums_create_completion_list();
    
    int arg1 = 1;
    int arg2 = 2;
    ums_wid_t worker1 = ums_create_worker_thread(comp_list1, STACK_SIZE, function1, &arg1);
    ums_wid_t worker2 = ums_create_worker_thread(comp_list1, STACK_SIZE, function2, &arg2);
    ums_wid_t worker3 = ums_create_worker_thread(comp_list1, STACK_SIZE, function2, &arg2);
    ums_wid_t worker4 = ums_create_worker_thread(comp_list1, STACK_SIZE, function2, &arg2);
    ums_wid_t worker5 = ums_create_worker_thread(comp_list1, STACK_SIZE, function2, &arg2);
    ums_wid_t worker6 = ums_create_worker_thread(comp_list2, STACK_SIZE, function1, &arg1);
    ums_wid_t worker7 = ums_create_worker_thread(comp_list2, STACK_SIZE, function2, &arg2);
    ums_wid_t worker8 = ums_create_worker_thread(comp_list2, STACK_SIZE, function2, &arg2);
    ums_wid_t worker9 = ums_create_worker_thread(comp_list2, STACK_SIZE, function2, &arg2);
    ums_wid_t worker10 = ums_create_worker_thread(comp_list2, STACK_SIZE, function2, &arg2);
    
    ums_sid_t scheduler1 = ums_create_scheduler(comp_list1, loop1);
    ums_sid_t scheduler2 = ums_create_scheduler(comp_list2, loop1);
    
    ums_exit();

    
    return 0;
}