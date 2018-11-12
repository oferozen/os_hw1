/*
- simple checks of the forbidden activities log 
        * checks that the forbidden activities log of process is removed after disabling the policy to this process
        * checks that the forbidden activities log of process is ordered by time of occuring.
	* checks that the returned forbidden activities from get_process_log are deleted from the log.
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

        TASSERT((retval=enable_policy(my_pid ,size, 234123)) ==0,
                "unexpected error in enable_policy (retval=%d)",retval);

        TASSERT((retval=set_process_capabilities(my_pid,0,password)) ==0,
                "unexpected error in set_process_capabilities (retval=%d)",retval);

        TASSERT((retval=fork()) == -1,
                "fork: should not work for process with cap_level=0 (retval=%d)",retval);
        TASSERT(errno == EINVAL,
               "fork: should set errno=EINVAL when process with cap_level=0 calles it (errno=%d)",errno);

        TASSERT((retval=disable_policy(my_pid , password)) ==0,
                "unexpected error in disable_policy (retval=%d)",retval);

        TASSERT((retval=enable_policy(my_pid ,size, 234123)) ==0,
                "unexpected error in enable_policy (retval=%d)",retval);

        TASSERT((retval=get_process_log(my_pid,1,user_mem)) == -1,
                "get_process_log: disable_policy should removed the list of forbidden activities (retval=%d)",retval);
        TASSERT(errno == EINVAL,
               "get_process_log: should set errno=EINVAL when size>number of existing forbidden activities (errno=%d)",errno);
        
        TASSERT((retval=sched_yield()) ==0,
                "unexpected error in sched_yield (retval=%d)",retval);       

        TASSERT((retval=set_process_capabilities(my_pid,0,password)) ==0,
                "unexpected error in set_process_capabilities (retval=%d)",retval);

        for(int i=0;i<10;i++){
                fork();
        }

        TASSERT((retval=get_process_log(my_pid,10,user_mem)) == 0,
                "unexpected error in get_process_log (retval=%d)",retval);

        int timeTemp=user_mem[0].time;

        for(int i=0;i<10;i++){
                TASSERT((retval=user_mem[i].time) >=timeTemp,
                        "log error: time should be rising up");
                timeTemp=user_mem[i].time;
        }

        TASSERT((retval=get_process_log(my_pid,1,user_mem)) == -1,
                "get_process_log: returned forbidden activities from get_process_log should be deleted from the log (retval=%d)",retval);
        TASSERT(errno == EINVAL,
               "get_process_log: should set errno=EINVAL when size>number of existing forbidden activities (errno=%d)",errno);
        
        TASSERT((retval=disable_policy(my_pid , password)) ==0,
                "unexpected error in disable_policy (retval=%d)",retval);
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        return 0;
}
