#pragma once

#include "const.h"

#define UMS_DEVICE "/dev/ums"


int ums_enter();
int ums_exit();

int open_device();
int close_device();
