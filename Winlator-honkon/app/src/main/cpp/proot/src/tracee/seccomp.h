#include "tracee/tracee.h"
#include "sys/vfs.h"

struct compat_statfs {
	int f_type;
	int f_bsize;
	int f_blocks;
	int f_bfree;
	int f_bavail;
	int f_files;
	int f_ffree;
	fsid_t f_fsid;
	int f_namelen;
	int f_frsize;
	int f_flags;
	int f_spare[4];
};

void restart_syscall_after_seccomp(Tracee* tracee);
void set_result_after_seccomp(Tracee *tracee, word_t result);
int handle_seccomp_event(Tracee* tracee);
void fix_and_restart_enosys_syscall(Tracee* tracee);
