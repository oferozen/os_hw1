/*
- stress test 3
	* checks that everything is just fine if we fo not enabling the policy to a process.
 */
#include "test.h"

int main()
{
        int retval;
        int size=20;
        int password=234123;
        int my_pid=getpid();
        int timeTemp;
        struct forbidden_activity_info user_mem[10000];
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        int j;
        for(int k=0;k<5;++k){
                j=0;
                for(int i=0;i<500;++i){
                        sched_yield();

                }
        }
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        return 0;
}
