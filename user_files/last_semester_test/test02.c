/* checking return values of disable_policy on failures */
#include "test.h"

int main()
{
    int retval;
    int size=5;
    int my_pid=getpid();
    int password=234123;
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    TASSERT((retval=disable_policy( my_pid, password)) == -1,
            "disable_policy: should return -1 if enable_policy haven’t invoked yet (retval=%d)",retval);
    TASSERT(errno == EINVAL,
            "disable_policy: should set errno=EINVAL if enable_policy haven’t invoked yet (errno=%d)",errno);
    
    TASSERT((retval=enable_policy(my_pid , size, password)) == 0,
            "enable_policy: should return 0 on success (retval=%d)",retval);

    TASSERT((retval=disable_policy( -1, password)) == -1,
            "disable_policy: should return -1 when pid is negative (retval=%d)",retval);
    TASSERT(errno == ESRCH,
            "disable_policy: should set errno=ESRCH when pid is negative (errno=%d)",errno);

    TASSERT((retval=disable_policy( my_pid, 215)) == -1,
            "disable_policy: should return -1 when password is incorrect (retval=%d)",retval);
    TASSERT(errno == EINVAL,
            "disable_policy: should set errno=EINVAL when password is incorrect (errno=%d)",errno);

    TASSERT((retval=disable_policy( my_pid, password)) == 0,
            "disable_policy: should return 0 on success (retval=%d)",retval);
    
    TASSERT((retval=disable_policy( my_pid, password)) == -1,
            "disable_policy: should return -1 if enable_policy haven’t invoked yet  (retval=%d)",retval);
    TASSERT(errno == EINVAL,
            "disable_policy: should set errno=EINVAL if enable_policy haven’t invoked yet  (errno=%d)",errno);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	return 0;
}
