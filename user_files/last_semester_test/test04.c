/* checking return values of get_process_log on failures */
#include "test.h"

int main()
{
        int retval;
        int my_pid=getpid();
        int size=5;
        struct forbidden_activity_info user_mem[5];
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        // get_process_log

        TASSERT((retval=get_process_log(my_pid,size,user_mem)) == -1,
                "get_process_log: should return -1 when the policy dont enforce this process (retval=%d)",retval);
        TASSERT(errno == EINVAL,
               "get_process_log: should set errno=EINVAL when the policy dont enforce this process (errno=%d)",errno);

        TASSERT((retval=enable_policy(my_pid ,size, 234123)) ==0,
                "unexpected error in enable_policy (retval=%d)",retval);  

        TASSERT((retval=get_process_log(-1,0,user_mem)) == -1,
                "get_process_log: should return -1 when pid<0 (retval=%d)",retval);
        TASSERT(errno == ESRCH,
               "get_process_log: should set errno=ESRCH when pid<0 (errno=%d)",errno);

        TASSERT((retval=get_process_log(my_pid,size,user_mem)) == -1,
                "get_process_log: should return -1 when size>number of records  (retval=%d)",retval);
        TASSERT(errno == EINVAL,
               "get_process_log: should set errno=EINVAL when size>number of records  (errno=%d)",errno);

        TASSERT((retval=get_process_log(my_pid,-1,user_mem)) == -1,
                "get_process_log: should return -1 when size<0  (retval=%d)",retval);
        TASSERT(errno == EINVAL,
               "get_process_log: should set errno=EINVAL when size<0  (errno=%d)",errno);    

        TASSERT((retval=disable_policy(my_pid , 234123)) ==0,
                "unexpected error in disable_policy (retval=%d)",retval);     

        TASSERT((retval=get_process_log(my_pid,0,user_mem)) == -1,
                "get_process_log: should return -1 when policy doesn’t apply to this process   (retval=%d)",retval);
        TASSERT(errno == EINVAL,
               "get_process_log: should set errno=EINVAL when policy doesn’t apply to this process  (errno=%d)",errno);  
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        return 0;
}
