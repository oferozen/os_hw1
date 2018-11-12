#ifndef _TEST_H_
#define _TEST_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <sys/timeb.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include "hw1_syscalls.h"

void ErrMsg(int res, char *msg)
{
	fprintf(stderr, msg);
	fprintf(stderr, ", result==[%d], errno==[%d], description==[%s]\n", res, errno, strerror(errno));
	exit(1); /* TEST_FAILURE, just not to redefine that const*/
}

void do_long_calc(){

	int i = 99999999;
	int j = 999999999;
	int x;
	while(i){
		while(j){
			x= 1/2;
			x= 5/2;
			x= 5*2;
			x= j--;
		}
		i--;
	}
}
void do_some_calc(){
	int i = 9999999;
	int j = 9999;
	int x;
	while(i){
		while(j){
			x= 1/2;
			x= 5/2;
			x= 5*2;
			x= j--;
		}
		i--;
	}
}

void do_specific_calc(int milli_seconds){

}

int is_process_sleeping(pid_t pid) {
    char command[200];
    int res;
    sprintf(command, "cat /proc/%ld/status | grep State | cut -f2 | head -c 1 | grep -q S", pid);
    res = system(command);
    return (res == 0);
}

int process_is_owned_by(pid_t pid, int uid) {
    char command[200];
    int res;
    sprintf(command, "cat /proc/%ld/status | grep Uid | cut -f3 | grep -q %d", pid, uid);
    res = system(command);
    return (res == 0);
}

int array_contains(int *arr, int n, int val) {
    for (int i = 0; i < n; i++) {
        if (arr[i] == val)
            return 1;
    }
    return 0;
}

int arrays_set_equal(int *a, int *b, int n) {
    for (int i = 0; i < n; i++)
        if (!array_contains(b, n, a[i]) ||
                !array_contains(a, n, b[i])) {
            printf("%d,%d\n",a[i],b[i]);
            return 0;
        }
    return 1;
}

int get_sem_val(int sid, int semnum) {
    return semctl(sid, semnum, GETVAL, 0);
}

#define eprintf(...) fprintf (stderr, __VA_ARGS__)
#define lprintf(...) do {flockfile(stdout); printf(__VA_ARGS__); funlockfile(stdout);} while(0)

#define TASSERT(x, message, ...) \
	if ((x)) {} else { \
		flockfile(stderr); \
		fprintf(stderr, message "\n", ##__VA_ARGS__); \
		funlockfile(stderr); \
		exit(1); \
	}

#define TPASSED	printf("%s - passed\n", __FILE__); return 0;

#define TPL printf("<%s : %d>\n", __FUNCTION__, __LINE__);
#define TPR(...) printf("<%s : %d> "  __VA_ARGS__ "\n", __FUNCTION__, __LINE__);

#define SEM_WAIT(sid, sb, num) \
    sb.sem_num = num; \
    sb.sem_op = -1; \
    semop(sid, &sb, 1);

#define SEM_POST(sid, sb, num) \
    sb.sem_num = num; \
    sb.sem_op = 1; \
    semop(sid, &sb, 1);

#endif /* _TEST_H_ */
