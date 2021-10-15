#pragma once

<linux/proc_fs.h>

union proc_op {
    
};

struct pid_entry {
    const char *name;
    unsigned int len;
    umode_t mode;
    const struct inode_operations *iop;
    const struct file_operations *fop;
    union proc_op op;
};

#define NOD(NAME, MODE, IOP, FOP, OP) {   \
	.name = (NAME),					      \
	.len  = sizeof(NAME) - 1,			  \
	.mode = MODE,					      \
	.iop  = IOP,					      \
	.fop  = FOP,					      \
	.op   = OP,					          \
}
