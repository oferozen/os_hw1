/* checking return values of enable_policy on failures */
#include "test.h"

int main()
{
    int retval;
    int size=5;
    int my_pid=getpid();
    int password=234123;
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    TASSERT((retval=enable_policy( -1 , size, password)) == -1,
            "enable_policy: should return -1 when pid is negative (retval=%d)",retval);
    TASSERT(errno == ESRCH,
            "enable_policy: should set errno=ESRCH when pid is negative (errno=%d)",errno);

    TASSERT((retval=enable_policy( my_pid , size, 215)) == -1,
            "enable_policy: should return -1 when password is incorrect (retval=%d)",retval);
    TASSERT(errno == EINVAL,
            "enable_policy: should set errno=EINVAL when password is incorrect (errno=%d)",errno);

    TASSERT((retval=enable_policy(my_pid , -1, password)) == -1,
            "enable_policy: should return -1 when size is negative (retval=%d)",retval);
    TASSERT(errno == EINVAL,
            "enable_policy: should set errno=EINVAL when size is negative (errno=%d)",errno);

    TASSERT((retval=enable_policy(my_pid , size, password)) == 0,
            "enable_policy: should return 0 on success (retval=%d)",retval);
    
    TASSERT((retval=enable_policy(my_pid , size, password)) == -1,
            "enable_policy: should return -1 if this process already enabled (retval=%d)",retval);
    TASSERT(errno == EINVAL,
            "enable_policy: should set errno=EINVAL if this process already enabled (errno=%d)",errno);


    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	return 0;
}
