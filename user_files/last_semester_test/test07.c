/*
- fork checks
        * checks that the new process's forbidden list is empty.
	* checks that the policy to the new process is disabled.
 */
#include "test.h"

int main()
{
        int retval;
        int my_pid=getpid();
        int size=20;
        int password=234123;
        struct forbidden_activity_info user_mem[20];
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        TASSERT((retval=enable_policy(my_pid ,size, password)) ==0,
                "unexpected error in enable_policy (retval=%d)",retval);
        
        int forkId=fork();
        if(forkId==0){ //son
                my_pid=getpid();
                TASSERT((retval=enable_policy(my_pid ,size, password)) ==0,
                        "unexpected error in enable_policy (retval=%d)",retval);

                TASSERT((retval=get_process_log(my_pid,1,user_mem)) == -1,
                        "get_process_log: after fork the new process's forbidden list is empty (retval=%d)",retval);
                TASSERT(errno == EINVAL,
                        "get_process_log: should set errno=EINVAL when size>number of existing forbidden activities (errno=%d)",errno);
                        
                TASSERT((retval=disable_policy(my_pid, password)) ==0,
                        "unexpected error in disable_policy (retval=%d)",retval);
                
        }else{//father
                TASSERT((retval=disable_policy(my_pid, password)) ==0,
                        "unexpected error in disable_policy (retval=%d)",retval);
        }
       

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        return 0;
}
