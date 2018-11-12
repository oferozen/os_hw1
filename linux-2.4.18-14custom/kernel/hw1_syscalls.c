
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
int sys_enable_policy(pid_t pid , int size, int password){
	struct task_struct* p = find_task_by_pid(pid);
	if(!p){
		return -ESRCH;
	}
	if(password != 234123){
		return -EINVAL;
	}
	if(p->policy_on == 1){
		return -EINVAL;
	}
	if(size < 0){
		return -EINVAL;
	}
	p->logArray = (struct log_array*)kmalloc(sizeof(struct log_array),GFP_KERNEL);
	if(!p->logArray){
		return -ENOMEM;
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
    p->policy_on=1;
    p->logArray->full=0;
	
	return 0;
}


/* system call number 244 */
int sys_disable_policy(pid_t pid, int password){
	struct task_struct* p = find_task_by_pid(pid);
	if(!p){
		return -ESRCH;
	}
	if(p->policy_on == 0){
		return -EINVAL;
	}
	if(password != 234123){
		return -EINVAL;
	}

	p->policy_on=0;
	p->policy_level=2;
	if(p->logArray->size!=0){
        kfree(p->logArray->array);
    }
	kfree(p->logArray);
	
	return 0;
}


/* system call number 245 */
int sys_set_process_capabilities(pid_t pid, int new_level, int password){
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
	if(p->policy_on == 0){
		return -EINVAL;
	}
	 p->policy_level=new_level;
	
	return 0;
}


/* system call number 246 */
int sys_get_process_log(pid_t pid, int size, struct forbidden_activity_info* user_mem){
	
	int write_curr;
    int prev_indexRead;
    struct forbidden_activity_info* tempArray;
	struct task_struct* p = find_task_by_pid(pid);
	
	
	if(!p){
		return -ESRCH;
	}
	if(size > p->logArray->size || size < 0){
		return -EINVAL;
	}
	if(p->policy_on == 0){
		return -EINVAL;
	}
	if(p->logArray->full != 1){ /* all these errors can accure only if array is not full */
		if(p->logArray->indexWrite == p->logArray->indexRead && size > 0){ /* array is empty and asking to copy records */
			return -EINVAL;
		}
		if(p->logArray->indexWrite >= p->logArray->indexRead){ /* not enough records to copy */
			if((p->logArray->indexWrite - p->logArray->indexRead ) < size ){
				return -EINVAL;
			}
		}else if((p->logArray->size - p->logArray->indexRead + p->logArray->indexWrite) < size){ /* same but the indexes write/read are switched */
				return -EINVAL;
		}
	}
	
	prev_indexRead = p->logArray->indexRead; /* backing up the previous index to read from => if copy_to_user failed */
	
	tempArray = (struct forbidden_activity_info*)kmalloc(size* sizeof(struct forbidden_activity_info),GFP_KERNEL);
    if(!tempArray){
        return -ENOMEM;
    }
    for(write_curr = 0; write_curr < size; ++write_curr){
        tempArray[write_curr] = p->logArray->array[p->logArray->indexRead];

        ++p->logArray->indexRead;
        if(p->logArray->indexRead == p->logArray->size){ 
            p->logArray->indexRead = 0;
        }

    }
    if(copy_to_user(user_mem, tempArray, (size)* sizeof(struct forbidden_activity_info)) != 0){
        kfree(tempArray);
        p->logArray->indexRead = prev_indexRead;
        return -ENOMEM;
    }
    kfree(tempArray);
    p->logArray->full = 0;
	
	if(size == p->logArray->size){ /* if we copied the whole array, refreshing the indexes to 0 */
		p->logArray->indexWrite = 0;
        p->logArray->indexRead = 0;
	}
	
	return 0;
}









