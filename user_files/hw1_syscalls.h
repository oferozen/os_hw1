#ifndef LINUX_2_4_18_14CUSTOM_HW1_SYSCALLS_H
#define LINUX_2_4_18_14CUSTOM_HW1_SYSCALLS_H


#include <errno.h>
#include <stdio.h>
#include <sys/types.h>

struct forbidden_activity_info{
    int syscall_req_level;
    int proc_level;
    int time;
};


/* system call number 243 */
int enable_policy(pid_t pid, int size, int password){
    unsigned int res;
     if(pid<0){
         errno=ESRCH;
         return -1;
     }
     if(size<0){
         errno=EINVAL;
         return -1;
     }

    __asm__(
    "int $0x80;"
    : "=a" (res)
    : "0" (243), "b" (pid), "c" (size), "d" (password)
    : "memory"
    );
    if(res==0){
        return 0;
    }
	
    if(res >= (unsigned long)(-125)){
        errno = -res;
    }
	
    return -1;
}

/* system call number 244 */
int disable_policy(pid_t pid, int password){
	 unsigned int res;
     if(pid<0){
         errno=ESRCH;
         return -1;
     }

	 __asm__(
    "int $0x80;"
    : "=a" (res)
    : "0" (244), "b" (pid), "c" (password)
    : "memory"
    );
	
    if(res==0){
        return 0;
    }
	if(res >= (unsigned long)(-125)){
        errno = -res;
    }
	
    return -1;
}


/* system call number 245 */
int set_process_capabilities(pid_t pid, int new_level, int password){
	 unsigned int res;
     if(pid<0){
         errno=ESRCH;
         return -1;
     }
	
	 __asm__(
    "int $0x80;"
    : "=a" (res)
    : "0" (245), "b" (pid), "c" (new_level), "d" (password)
    : "memory"
    );
	
    if(res==0){
        return 0;
    }
	if(res >= (unsigned long)(-125)){
        errno = -res;
    }
	
    return -1;
}


/* system call number 246 */
int get_process_log(pid_t pid, int size, struct forbidden_activity_info* user_mem){
	 unsigned int res;
     if(pid<0){
         errno=ESRCH;
         return -1;
     }
	 if(size==0){
		return 0;
	}

	 __asm__(
    "int $0x80;"
    : "=a" (res)
    : "0" (246), "b" (pid), "c" (size), "d" (user_mem)
    : "memory"
    );
	
    if(res==0){
        return 0;
    }
	if(res >= (unsigned long)(-125)){
        errno = -res;
    }
	
    return -1;
}


#endif //LINUX_2_4_18_14CUSTOM_HW1_SYSCALLS_H

