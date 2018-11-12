/*
- parallel processes
	* make some processes, every process will do some forbidden activities,
	  and then we will check that every process has it and only it forbidden activities in its log
 */
#include "test.h"

int main()
{
        int retval;
        int size=20;
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
                        
                        TASSERT((retval=set_process_capabilities(son2,0,password)) ==0,
                                "unexpected error in set_process_capabilities (retval=%d)",retval);

                        sched_yield();
                        fork();
                        
                        TASSERT((retval=get_process_log(son2,2,user_mem)) == 0,
                                "unexpected error in get_process_log (retval=%d)",retval); 

                        TASSERT((retval=user_mem[0].syscall_req_level) == 1,
                                "invalid syscall_req_level value in log");

                        TASSERT((retval=user_mem[0].proc_level) == 0,
                                "invalid proc_level value in log");

                        TASSERT((retval=user_mem[1].syscall_req_level) == 2,
                                "invalid syscall_req_level value in log");

                        TASSERT((retval=user_mem[1].proc_level) == 0,
                                "invalid proc_level value in log");

                        TASSERT((retval=disable_policy(son2 , password)) ==0,
                                "unexpected error in disable_policy (retval=%d)",retval);
                } else{//son1
                        TASSERT((retval=enable_policy(son1 ,size, password)) ==0,
                                "unexpected error in enable_policy (retval=%d)",retval);
                        
                        TASSERT((retval=set_process_capabilities(son1,0,password)) ==0,
                                "unexpected error in set_process_capabilities (retval=%d)",retval);

                        sched_yield();
                        fork();
                        
                        TASSERT((retval=get_process_log(son1,2,user_mem)) == 0,
                                "unexpected error in get_process_log (retval=%d)",retval); 

                        TASSERT((retval=user_mem[0].syscall_req_level) == 1,
                                "invalid syscall_req_level value in log");

                        TASSERT((retval=user_mem[0].proc_level) == 0,
                                "invalid proc_level value in log");

                        TASSERT((retval=user_mem[1].syscall_req_level) == 2,
                                "invalid syscall_req_level value in log");

                        TASSERT((retval=user_mem[1].proc_level) == 0,
                                "invalid proc_level value in log");

                        TASSERT((retval=disable_policy(son1 , password)) ==0,
                                "unexpected error in disable_policy (retval=%d)",retval);                   

                }
        }else{//father1
                        TASSERT((retval=enable_policy(father1 ,size, password)) ==0,
                                "unexpected error in enable_policy (retval=%d)",retval);
                        
                        TASSERT((retval=set_process_capabilities(father1,1,password)) ==0,
                                "unexpected error in set_process_capabilities (retval=%d)",retval);

                        sched_yield();
                        fork();
                        
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
