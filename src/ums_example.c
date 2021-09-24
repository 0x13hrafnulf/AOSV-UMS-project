#include "ums_lib.h"

#define STACK_SIZE 4096 * 2

void loop()
{
    printf("UMS_EXAMPLE: %s\n", __FUNCTION__);
    ums_exit_scheduling_mode();
}

void function1(void *args)
{

}
void function2(void *args)
{

}

int main()
{
    ums_enter();
    ums_clid_t comp_list1 =  ums_create_completion_list();
    printf("UMS_EXAMPLE: #:%d completion list was created.\n", comp_list1);
    ums_clid_t comp_list2 =  ums_create_completion_list();
    printf("UMS_EXAMPLE: #:%d completion list was created.\n", comp_list2);

    int arg1 = 1;
    int arg2 = 2;
    ums_wid_t worker1 = ums_create_worker_thread(comp_list1, STACK_SIZE, function1, &arg1);
    ums_wid_t worker2 = ums_create_worker_thread(comp_list2, STACK_SIZE, function2, &arg2);

    
    ums_sid_t scheduler1 = ums_create_scheduler(comp_list1, loop);
    ums_sid_t scheduler2 = ums_create_scheduler(comp_list2, loop);
    
    ums_exit();

    
    return 0;
}