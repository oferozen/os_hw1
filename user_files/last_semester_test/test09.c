/*
- stress test 1
	* enable and then do forbidden activities and get_process_log in loop, if there is a memory leak, the kernel may freeze	
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
        int j;
        for(int k=0;k<5;++k){
                j=0;
                for(int i=0;i<500000;++i){
                        sched_yield();
						j++;
                        if(j>=990){
                                TASSERT((retval=get_process_log(my_pid,990,user_mem)) == 0,
                                        "unexpected error in get_process_log (retval=%d)",retval);
                                j=0;
                        }

                }
        }

        TASSERT((retval=disable_policy(my_pid , password)) ==0,
                "unexpected error in disable_policy (retval=%d)",retval);
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        return 0;
}
