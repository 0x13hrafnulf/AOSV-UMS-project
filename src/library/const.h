#pragma once

#include <linux/ioctl.h>

#define UMS_DEVICE          "/dev/ums"
#define UMS_IOC_MAGIC       'R'

#define UMS_ENTER                           _IO(UMS_IOC_MAGIC, 1)
#define UMS_EXIT                            _IO(UMS_IOC_MAGIC, 2)

#define UMS_SUCCESS                         0
#define UMS_ERROR                           -1
#define UMS_ERROR_PROCESS_NOT_FOUND         1000
#define UMS_ERROR_PROCESS_ALREADY_EXISTS    1001
