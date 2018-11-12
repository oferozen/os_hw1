/*
- communication between processes
	* let some processes invoke get, enable, disable on other processes
 */
#include "test.h"

int main()
{
        int retval;
        int size=20;
        int status;
        int password=234123;
        struct forbidden_activity_info user_mem[20];
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        int father1=getpid();
        int son1;
        int son2;
        retval=fork();
        if(retval==0){//son1
                son1=getpid();
                retval=fork();
                if(retval==0){//son2
                        son2=getpid();
                        TASSERT((retval=enable_policy(son2 ,size, password)) ==0,
                                "unexpected error in enable_policy (retval=%d)",retval);
                        TASSERT((retval=enable_policy(son1 ,size, password)) ==0,
                                "unexpected error in enable_policy (retval=%d)",retval);
                        TASSERT((retval=enable_policy(father1 ,size, password)) ==0,
                                "unexpected error in enable_policy (retval=%d)",retval);
                        
                        TASSERT((retval=set_process_capabilities(son1,1,password)) ==0,
                                "unexpected error in set_process_capabilities (retval=%d)",retval);
                        TASSERT((retval=set_process_capabilities(father1,1,password)) ==0,
                                "unexpected error in set_process_capabilities (retval=%d)",retval);
                        TASSERT((retval=disable_policy(son2 , password)) ==0,
                                "unexpected error in disable_policy (retval=%d)",retval);
                } else{//son1
                        wait(&status);
                        sched_yield();
                        fork();
                        TASSERT((retval=get_process_log(son1,2,user_mem)) == -1,
                                "get_process_log: should return -1 when asking more than existing (retval=%d)",retval);
                        TASSERT(errno == EINVAL,
                                "get_process_log: should set errno=EINVA when asking more than existing  (errno=%d)",errno);

                        TASSERT((retval=get_process_log(son1,1,user_mem)) == 0,
                                "unexpected error in get_process_log (retval=%d)",retval);
                        
                        TASSERT((retval=user_mem[0].syscall_req_level) == 2,
                                "invalid syscall_req_level value in log");

                        TASSERT((retval=user_mem[0].proc_level) == 1,
                                "invalid proc_level value in log");

                        TASSERT((retval=disable_policy(son1 , password)) ==0,
                                "unexpected error in disable_policy (retval=%d)",retval);                       

                }
        }else{//father1
                wait(&status);
                sched_yield();
                fork();
                TASSERT((retval=get_process_log(father1,2,user_mem)) == -1,
                         "get_process_log: should return -1 when asking more than existing (retval=%d)",retval);
                TASSERT(errno == EINVAL,
                        "get_process_log: should set errno=EINVA when asking more than existing  (errno=%d)",errno);

                TASSERT((retval=get_process_log(father1,1,user_mem)) == 0,
                        "unexpected error in get_process_log (retval=%d)",retval);
                        
                TASSERT((retval=user_mem[0].syscall_req_level) == 2,
                        "invalid syscall_req_level value in log");

                TASSERT((retval=user_mem[0].proc_level) == 1,
                        "invalid proc_level value in log");

                TASSERT((retval=disable_policy(father1 , password)) ==0,
                        "unexpected error in disable_policy (retval=%d)",retval);    
        
        }
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        return 0;
}
