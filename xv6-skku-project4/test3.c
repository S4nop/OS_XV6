#include "types.h"
#include "stat.h"
#include "user.h"

#define NTHREAD 8

void *stack[NTHREAD];
int tid[NTHREAD];
void *retval[NTHREAD];

void *thread(void *arg){
	thread_exit(arg);
}

void *thread2(void *arg){
	printf(1, "OK\n");
	exit();
}

int
main(int argc, char **argv)
{
	int i, j;

	printf(1, "TEST3: ");

	for(i=0;i<NTHREAD;i++)
		stack[i] = malloc(4096);
//printf(1, "1\n");
	for(i=0;i<100;i++){
		for(j=0;j<NTHREAD-1;j++){
			tid[j] = thread_create(thread, 30, (void *)j, stack[j]);
			if(tid[j] == -1){
				printf(1, "WRONG\n");
				exit();
			}
	//		printf(1, "tid : %d\n", tid[j]);
		}

//printf(1, "2\n");
		for(j=0;j<NTHREAD-1;j++){
//printf(1, "3, tid: %d\n", tid[j]);
			if(thread_join(tid[j], &retval[j]) == -1){
				printf(1, "WRONG\n");
				exit();
			}

			if((int)retval[j] != j){
				printf(1, "WRONG\n");
				exit();
			}
		}
	}

//printf(1, "4\n");
	for(i=0;i<NTHREAD-1;i++){
		tid[i] = thread_create(thread, 30, 0, stack[i]);
		if(tid[i] == -1){
			printf(1, "WRONG\n");
			exit();
		}
	}
//printf(1, "5\n");
	if(thread_create(thread, 30, 0, stack[NTHREAD-1]) != -1){
		printf(1, "WRONG\n");
		exit();
	}
//printf(1,"next\n");
	for(i=0;i<NTHREAD-1;i++){
		if(thread_join(tid[i], &retval[i]) == -1){
			printf(1, "WRONG\n");
			exit();
		}
	}

	for(i=0;i<NTHREAD-1;i++){
		tid[i] = thread_create(thread2, 30, 0, stack[i]);
		if(tid[i] == -1){
			printf(1, "WRONG\n");
			exit();
		}
	}

	thread_join(tid[0], &retval[0]);

	for(i=0;i<NTHREAD;i++)
		free(stack[i]);

	printf(1, "WRONG\n");

	exit();
}
