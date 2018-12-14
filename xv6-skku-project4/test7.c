#include "types.h"
#include "stat.h"
#include "user.h"

#define NTHREAD 7

void *stack[NTHREAD];
int tid[NTHREAD];
void *retval[NTHREAD];
int mem;
int lock;

void *thread(void *arg){
	if(mem != (int)arg){
		printf(1, "WRONG arg %d %d\n", mem, (int)arg);
		exit();
	}

	thread_exit(0);
}

int
main(int argc, char **argv)
{
	int i;
	printf(1, "TEST7: ");
	lock_init(&lock);
	for(i=0;i<NTHREAD;i++)
		stack[i] = malloc(4096);

	for(i=0;i<NTHREAD;i++){
		mem = i;
		lock_acquire(&lock);
		tid[i] = thread_create(thread, 10, (void *)i, stack[i]);
		if(tid[i] == -1){
			printf(1, "WRONG\n");
			exit();
		}
		lock_release(&lock);
	}

	for(i=0;i<NTHREAD;i++){
		if(thread_join(tid[i], &retval[i]) == -1){
			printf(1, "WRONG\n");
			exit();
		}
	}

	for(i=0;i<NTHREAD;i++)
		free(stack[i]);

	printf(1, "OK\n");

	exit();
}
