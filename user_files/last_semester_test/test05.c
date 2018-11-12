/* - simple checks of the policy mechanism:
	* checks that fork doesn't work for process with privilege level 0 or 1.
	* same thing for other discussed system calls.
 */
#include "test.h"

int main()
{
        int retval;
        int my_pid=getpid();
        int size=10;
        int password=234123;
        struct forbidden_activity_info user_mem[10];
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        TASSERT((retval=enable_policy(my_pid ,size, 234123)) ==0,
                "unexpected error in enable_policy (retval=%d)",retval);

        TASSERT((retval=set_process_capabilities(my_pid,0,password)) ==0,
                "unexpected error in set_process_capabilities (retval=%d)",retval);

        TASSERT((retval=fork()) == -1,
                "fork: should not work for process with cap_level=0 (retval=%d)",retval);
        TASSERT(errno == EINVAL,
               "fork: should set errno=EINVAL when process with cap_level=0 calles it (errno=%d)",errno);

        TASSERT((retval=wait(NULL)) == -1,
                "wait: should not work for process with cap_level=0 (retval=%d)",retval);
        TASSERT(errno == EINVAL,
               "wait: should set errno=EINVAL when process with cap_level=0 calles it (errno=%d)",errno);         

        TASSERT((retval=sched_yield()) == -1,
                "sched_yield: should not work for process with cap_level=0 (retval=%d)",retval);
        TASSERT(errno == EINVAL,
               "sched_yield: should set errno=EINVAL when process with cap_level=0 calles it (errno=%d)",errno); 

        TASSERT((retval=set_process_capabilities(my_pid,1,password)) ==0,
                "unexpected error in set_process_capabilities (retval=%d)",retval);

        TASSERT((retval=fork()) == -1,
                "fork: should not work for process with cap_level=1 (retval=%d)",retval);
        TASSERT(errno == EINVAL,
               "fork: should set errno=EINVAL when process with cap_level=1 calles it (errno=%d)",errno);    

        TASSERT((retval=sched_yield()) != -1,
                "sched_yield: should work for process with cap_level=1 (retval=%d)",retval);


        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        return 0;
}
