/*
- ask for more forbidden activities in get_process_log system call than exists 
	* checks that for a process with x forbidden activities if we ask for y>x activities than we get -EINVAL
	  and the log does not affected at all.
 */
#include "test.h"

int main()
{
        int retval;
        int size=20;
        int password=234123;
        int my_pid=getpid();
        struct forbidden_activity_info user_mem[10000];
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        TASSERT((retval=enable_policy(my_pid ,size, 234123)) ==0,
                "unexpected error in enable_policy (retval=%d)",retval);

        TASSERT((retval=set_process_capabilities(my_pid,0,password)) ==0,
                "unexpected error in enable_policy (retval=%d)",retval);
        for(int i=0;i<1000;++i){
                sched_yield();
        }
        TASSERT((retval=get_process_log(my_pid,1001,user_mem)) == -1,
                "get_process_log: should return -1 when asking for more forbidden activities than exists (retval=%d)",retval);
        TASSERT(errno == EINVAL,
               "get_process_log: should set errno=EINVAL when size>number of existing forbidden activities (errno=%d)",errno);
        

        TASSERT((retval=disable_policy(my_pid , password)) ==0,
                "unexpected error in disable_policy (retval=%d)",retval);
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        return 0;
}
