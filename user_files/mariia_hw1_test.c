#include "hw1_syscalls.h"
#include "mariia_test.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define PASSWORD 234123
#define TEST_FAIL 255

bool boring_test1(){
	
	/*1. pid<0*/
	FAILURE(enable_policy (-1 , 2, 234123));
	ERRNO(ESRCH);
	
	/*2. size<0*/
	FAILURE(enable_policy (getpid(), -2, 234123));
	ERRNO(EINVAL);
	
	/*3. password*/
	FAILURE(enable_policy (getpid(), 10, 11111));
	ERRNO(EINVAL);
	
	/*4. ok*/
	SUCCESS(enable_policy (getpid(), 20, 234123));
	
	/*5. already enabled*/
	FAILURE(enable_policy (getpid(), 20, 234123));
	ERRNO(EINVAL);
	
	/*6. pid<0*/
	FAILURE(disable_policy (-1 , 234123));
	ERRNO(ESRCH);
	
	/*7. password*/
	FAILURE(disable_policy (getpid(), 11111));
	ERRNO(EINVAL);
	
	/*8. ok*/
	SUCCESS(disable_policy (getpid(), 234123));
	
	/*9. alredy disabled*/
	FAILURE(disable_policy (getpid(), 234123));
	ERRNO( EINVAL);
	
	return true;
}

bool boring_test2(){
	FAILURE(set_process_capabilities(getpid(), 1, 234123));
	ERRNO(EINVAL);
	
	SUCCESS(enable_policy (getpid(), 20, 234123));
	
	FAILURE(set_process_capabilities(-2, 1, 234123));
	ERRNO(ESRCH);
	
	FAILURE(set_process_capabilities(getpid(), 3, 234123));
	ERRNO(EINVAL);
	
	FAILURE(set_process_capabilities(getpid(), 1, 2341232));
	ERRNO(EINVAL);
	
	SUCCESS(set_process_capabilities(getpid(), 1, 234123));
	
	SUCCESS(disable_policy(getpid(), 234123));
	
	return true;
}

bool test1(){
	pid_t father = getpid();
	struct forbidden_activity_info log[4];
	
////////////////////////	
	//policy enabled
	SUCCESS(enable_policy(father, 0, PASSWORD));
	
	//forbidden
	wait(NULL);
	wait(NULL);
	wait(NULL);
	sched_yield();
	
	SUCCESS(get_process_log(father, 0, log));
	
	SUCCESS(set_process_capabilities(father, 2, PASSWORD));
	FAILURE(set_process_capabilities(father, -1, PASSWORD));
	ERRNO(EINVAL);
	
	//policy disabled
	SUCCESS(disable_policy(father, PASSWORD));
	
////////////////////////
	//policy enabled
	SUCCESS(enable_policy(father, 4, PASSWORD));
	
	//log_size=0
	SUCCESS(get_process_log(father, 0, log));
	
	FAILURE(get_process_log(father, 1, log));
	ERRNO(EINVAL);
	
	FAILURE(get_process_log(-2, 1, log));
	ERRNO(ESRCH);
	
	//lvl=0
	SUCCESS(set_process_capabilities(father, 0, PASSWORD));
	
	//forbidden
	fork();//0
	ERRNO(EINVAL);
	wait(NULL);//1
	ERRNO(EINVAL);
	sched_yield();//2
	ERRNO(EINVAL);
	wait(NULL);//3
	ERRNO(EINVAL);
	wait(NULL);//0
	ERRNO(EINVAL);
	
	SUCCESS(get_process_log(father, 4, log));
	TEST(log[0].syscall_req_level ==1);
	TEST(log[1].syscall_req_level ==1);
	TEST(log[2].syscall_req_level ==1);
	TEST(log[3].syscall_req_level ==1);
	
	FAILURE(get_process_log(father, 1, log));
	ERRNO(EINVAL);
	
	//forbidden
	fork();//0
	ERRNO(EINVAL);
	sched_yield();//1
	ERRNO(EINVAL);
	
	//lvl = 1
	SUCCESS(set_process_capabilities(father, 1, PASSWORD));
	//not forbidden
	sched_yield();
	ERRNO(EINVAL);
	
	//forbidden
	fork();//2
	ERRNO(EINVAL);
	
	FAILURE(get_process_log(father, 4, log));
	ERRNO(EINVAL);
	
	SUCCESS(get_process_log(father, 3, log));
	TEST(log[0].syscall_req_level ==2);
	TEST(log[1].syscall_req_level ==1);
	TEST(log[2].syscall_req_level ==2);
	TEST(log[0].proc_level ==0);
	TEST(log[1].proc_level ==0);
	TEST(log[2].proc_level ==1);
	
	//lvl = 2
	SUCCESS(set_process_capabilities(father, 2, PASSWORD));
	fflush(0);
	pid_t son = fork();
	if(son == 0){
		
		sleep(1);
		//forbidden
		wait(NULL);
		ERRNO(EINVAL);
		fork();
		ERRNO(EINVAL);
		
		SUCCESS(get_process_log(getpid(), 2, log));
		TEST(log[0].syscall_req_level ==1);
		TEST(log[1].syscall_req_level ==2);
		TEST(log[0].proc_level ==0);
		TEST(log[1].proc_level ==0);
		
		//policy disabled for father
		SUCCESS(disable_policy(father, PASSWORD));
		
		exit(0);
		
	} else {
		SUCCESS(enable_policy(son, 4, PASSWORD));
		SUCCESS(set_process_capabilities(son, 0, PASSWORD));
		sleep(2);
		
		int status;
		wait(&status);
		if(WEXITSTATUS(status) == TEST_FAIL){
			return false;
		}
		
		//son made disable for father
		FAILURE(disable_policy(father, PASSWORD));
		ERRNO(EINVAL);
	}
	
	return true;
}


//types of wait
bool test2(){
	pid_t father = getpid();
	struct forbidden_activity_info log[7];
	
	//lvl=2
	SUCCESS(enable_policy(father, 7, PASSWORD));
	//not forbidden
	sched_yield();
	
	//lvl=0
	SUCCESS(set_process_capabilities(father, 0, PASSWORD));
	//forbidden
	sched_yield();//0
	wait(NULL);//1
	waitpid(0, NULL, WNOHANG);//2
	
	//lvl=1
	SUCCESS(set_process_capabilities(father, 1, PASSWORD));
	//forbidden
	fork();//3
	
	//lvl=0
	SUCCESS(set_process_capabilities(father, 0, PASSWORD));
	//forbidden
	waitpid(0, NULL, WNOHANG);//4
	
	FAILURE(get_process_log(father, 6, log));
	FAILURE(get_process_log(father, 7, log));
	SUCCESS(get_process_log(father, 4, log));
	TEST(log[0].syscall_req_level = 1);
	TEST(log[1].syscall_req_level = 1);
	TEST(log[2].syscall_req_level = 2);
	TEST(log[3].syscall_req_level = 1);
	TEST(log[0].proc_level ==0);
	TEST(log[1].proc_level ==0);
	TEST(log[2].proc_level ==0);
	TEST(log[3].proc_level ==1);
	
	FAILURE(get_process_log(father, 2, log));
	SUCCESS(get_process_log(father, 1, log));
	TEST(log[0].syscall_req_level = 1);
	TEST(log[0].proc_level ==0);
	
	FAILURE(get_process_log(father, 1, log));
	
	//forbidden
	waitpid(0, NULL, WNOHANG);//0
	sched_yield();//1
	wait(NULL);//2
	fork();//3
	
	//lvl=2
	SUCCESS(set_process_capabilities(father, 2, PASSWORD));
	
	fflush(0);
	pid_t son = fork();
	if (son == 0){
		SUCCESS(enable_policy(getpid(), 5, PASSWORD));
		SUCCESS(get_process_log(father, 2, log));
		TEST(log[0].syscall_req_level = 1);
		TEST(log[1].syscall_req_level = 1);
		TEST(log[1].proc_level ==0);
		TEST(log[2].proc_level ==0);
		
		SUCCESS(set_process_capabilities(getpid(), 1, PASSWORD));
		//forbidden
		fork();
		
		sleep(1);
		
		exit(0);
		
	} else {
		sleep(1);
		SUCCESS(get_process_log(son, 1, log));
		TEST(log[0].syscall_req_level = 2);
		TEST(log[0].proc_level ==1);
		
		int status;
		wait(&status);
		if(WEXITSTATUS(status) == TEST_FAIL){
			return false;
		}
	}
	
	SUCCESS(disable_policy(father, PASSWORD));
	
	return true;
}


int main(){
	RUN_TEST(boring_test1);
	RUN_TEST(boring_test2);
	RUN_TEST(test1);
	RUN_TEST(test2);
	printf("Well done, all passed!\n");

	return 0;
}
