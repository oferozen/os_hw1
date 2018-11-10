
#include <linux/sched.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>
#include <linux/types.h>
#include <linux/errno.h>



struct forbidden_activity_info{
	int syscall_req_level;
	int proc_level;
	int time;
};


struct log_array{
    int indexWrite;
    int indexRead;
    int size;
    int full;
    struct forbidden_activity_info* array;
};


/* system call number 243 */
int enable_policy(pid_t pid , int size, int password){
	struct task_struct* p = find_task_by_pid(pid);
	if(!p){
		return -ESRCH;
	}
	if(password != 234123){
		return -EINVAL;
	}
	if(p->enable_policy == 1){
		return -EINVAL;
	}
	if(size < 0){
		return -EINVAL;
	}
	p->logArray = (struct log_array*)kmalloc(sizeof(struct log_array),GFP_KERNEL);
	if(!p->logArray){
		return -ENOMEM
	}
	if(size!=0){
        p->logArray->array=(struct forbidden_activity_info*)kmalloc(size* sizeof(struct forbidden_activity_info), GFP_KERNEL);
        if(!p->logArray->array){
            kfree(p->logArray);
            return -ENOMEM;
        }
    }
    p->logArray->indexRead=0;
    p->logArray->indexWrite=0;
    p->logArray->size=size;
    p->policy_level=2;
    p->enable_policy=1;
    p->logArray->full=0;
	
	return 0;
}


/* system call number 244 */
int disable_policy(pid_t pid, int password){
	struct task_struct* p = find_task_by_pid(pid);
	if(!p){
		return -ESRCH;
	}
	if(p->enable_policy == 0){
		return -EINVAL;
	}
	if(password != 234123){
		return -EINVAL;
	}

	p->enable_policy=0;
	p->policy_level=2;
	if(p->logArray->size!=0){
        kfree(p->logArray->array);
    }
	kfree(p->logArray);
	
	return 0;
}


/* system call number 245 */
int set_process_capabilities(pid_t pid, int new_level, int password){
	struct task_struct* p = find_task_by_pid(pid);
	if(!p){
		return -ESRCH;
	}
	if(new_level > 2 || new_level < 0){
        return -EINVAL;
    }
	if(password != 234123){
		return -EINVAL;
	}
	if(p->enable_policy == 0){
		return -EINVAL;
	}
	 p->policy_level=new_level;
	
	return 0;
}


/* system call number 246 */
int get_process_log(pid_t pid, int size, struct forbidden_activity_info* user_mem){
	struct task_struct* p = find_task_by_pid(pid);
	if(!p){
		return -ESRCH;
	}
	if(size > p->logArray->size || size < 0){
		return -EINVAL;
	}
	if(p->enable_policy == 0){
		return -EINVAL;
	}
	
	// TODO: copy the memory with "copy_to_user" 
	
	return 0;
}









