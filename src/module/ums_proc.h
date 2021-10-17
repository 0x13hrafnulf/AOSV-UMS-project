#pragma once

#include <linux/proc_fs.h>
#include "ums_api.h"

int init_proc(void);
int delete_proc(void);


extern process_list_t process_list;

union proc_op {
	int (*proc_get_link)(struct dentry *, struct path *);
	int (*proc_show)(struct seq_file *m, struct pid_namespace *ns, struct pid *pid, struct task_struct *task);
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
