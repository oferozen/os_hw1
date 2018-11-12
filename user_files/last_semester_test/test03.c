/* checking return values of set_process_capabilities on failures */
#include "test.h"

int main()
{
        int retval;
        int my_pid=getpid();
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        // set_process_capabilities

        TASSERT((retval=set_process_capabilities(my_pid,1,234123)) == -1,
                "set_process_capabilities: should return -1 when the policy dont enforce this process (retval=%d)",retval);
        TASSERT(errno == EINVAL,
               "set_process_capabilities: should set errno=EINVAL when the policy dont enforce this process (errno=%d)",errno);

        TASSERT((retval=enable_policy(my_pid ,10, 234123)) ==0,
                "unexpected error in enable_policy (retval=%d)",retval);  

        TASSERT((retval=set_process_capabilities(my_pid,1,2121)) == -1,
                "set_process_capabilities: should return -1 when password is wrong (retval=%d)",retval);
        TASSERT(errno == EINVAL,
               "set_process_capabilities: should set errno=EINVAL when password is wrong (errno=%d)",errno);

        TASSERT((retval=set_process_capabilities(-1,1,234123)) == -1,
               "set_process_capabilities: should return -1 when pid<0 (retval=%d)",retval);
        TASSERT(errno == ESRCH,
               "set_process_capabilities: should set errno=ESRCH when pid<0 (errno=%d)",errno);

        TASSERT((retval=set_process_capabilities(my_pid,3,234123)) == -1,
                 "set_process_capabilities: should return -1 when new_level>2 (retval=%d)",retval);
        TASSERT(errno == EINVAL,
                  "set_process_capabilities: should set errno=EINVAL when new_level>2 (errno=%d)",errno);
        TASSERT((retval=set_process_capabilities(my_pid,-1,234123)) == -1,
                 "set_process_capabilities: should return -1 when new_level<0 (retval=%d)",retval);
        TASSERT(errno == EINVAL,
                 "set_process_capabilities: should set errno=EINVAL when new_level<0 (errno=%d)",errno);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        return 0;
}
