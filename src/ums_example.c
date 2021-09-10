#include "ums_lib.h"

int main()
{
    ums_enter();
    ums_clid_t comp_list1 =  ums_create_completion_list();
    printf("UMS_EXAMPLE: #:%d completion list was created.\n", comp_list1);
    ums_clid_t comp_list2 =  ums_create_completion_list();
    printf("UMS_EXAMPLE: #:%d completion list was created.\n", comp_list2);
    ums_exit();
    return 0;
}