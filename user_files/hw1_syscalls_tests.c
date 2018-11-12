#include <sys/types.h>
#include <unistd.h>
#include <sched.h>

#include "hw1_syscalls.h"

int main() {
  enable_policy(getpid(),-24,234123);// FAIL
  enable_policy(getpid(),1,2); // FAIL
  enable_policy(-1,1,234123); // FAIL
  enable_policy(getpid()*3,1,234123); // FAIL
  enable_policy(getpid(),10,234123);
  enable_policy(getpid(),1,234123); // FAIL
  
  set_process_capabilities(getpid(),1,234123);
  set_process_capabilities(getpid(),5,234123); //FAIL
  set_process_capabilities(getpid(),-6,234123); //FAIL
  
  disable_policy(-1,234123); // FAIL
  disable_policy(getpid(),1); // FAIL
  disable_policy(getpid()*3,234123); // FAIL
  disable_policy(getpid(),234123);
  disable_policy(getpid(),234123); // FAIL
  
  set_process_capabilities(getpid()*2,1,234123); // FAIL
  set_process_capabilities(getpid(),1,234123); // FAIL
  set_process_capabilities(getpid(),3,234123); // FAIL
  
  
  struct forbidden_activity_info* log = malloc(sizeof(struct forbidden_activity_info)*10);
  get_process_log(getpid(),0,log); // FAIL
  enable_policy(getpid(),10,234123);
  get_process_log(getpid(),0,log);
  get_process_log(-1,0,log); // FAIL
  get_process_log(getpid(),-1,log); // FAIL
  get_process_log(getpid(),100,log); // FAIL
  get_process_log(getpid(),2,log); // FAIL
  
  
  sched_yield();
  set_process_capabilities(getpid(),0,234123); 
  sched_yield();// FAIL
  sched_yield();// FAIL
  sched_yield();// FAIL
  get_process_log(getpid(),5,log);// FAIL
  get_process_log(getpid(),3,log);
  printf("Tried to run syscall from %d which requires privilege %d while privilege is %d at time %d (sched_yield)\n",getpid(),log[0].syscall_req_level,log[0].proc_level,log[0].time);
  printf("Tried to run syscall from %d which requires privilege %d while privilege is %d at time %d (sched_yield)\n",getpid(),log[1].syscall_req_level,log[1].proc_level,log[1].time);
  printf("Tried to run syscall from %d which requires privilege %d while privilege is %d at time %d (sched_yield)\n",getpid(),log[2].syscall_req_level,log[2].proc_level,log[2].time);
  
  disable_policy(getpid(),234123);
  fork(); // works, we have 2 processes now
  enable_policy(getpid(),20,234123);
  wait(); // FAIL only in parent
  if (!get_process_log(getpid(),1,log)) {
    printf("Tried to run syscall from %d which requires privilege %d while privilege is %d at time %d (wait 1)\n",getpid(),log[0].syscall_req_level,log[0].proc_level,log[0].time);
  }
  fork(); // FAIL only in parent, we have 3 processes now
  if (!get_process_log(getpid(),2,log)) {
    printf("Tried to run syscall from %d which requires privilege %d while privilege is %d at time %d (fork 1)\n",getpid(),log[1].syscall_req_level,log[1].proc_level,log[1].time);
  }
  set_process_capabilities(getpid(),2,234123); 
  fork(); // works, we have 6 processes now
  set_process_capabilities(getpid(),1,234123); 
  fork(); // fails in 2 processes, we have 9 processes now.
  if (!get_process_log(getpid(),3,log)) {
    printf("Tried to run syscall from %d which requires privilege %d while privilege is %d at time %d (fork 2 p)\n",getpid(),log[2].syscall_req_level,log[2].proc_level,log[2].time);
  }
  else if (!get_process_log(getpid(),1,log)) {
    printf("Tried to run syscall from %d which requires privilege %d while privilege is %d at time %d (fork 2 c)\n",getpid(),log[0].syscall_req_level,log[0].proc_level,log[0].time);
  }
  
  disable_policy(getpid(),234123);
  enable_policy(getpid(),20,234123);
  set_process_capabilities(getpid(),0,234123); 
  fork(); // fails in all 9  processes
  if (!get_process_log(getpid(),1,log)) {
    printf("Tried to run syscall from %d which requires privilege %d while privilege is %d at time %d (fork 3)\n",getpid(),log[0].syscall_req_level,log[0].proc_level,log[0].time);
  }
  return 0;
} 
 
