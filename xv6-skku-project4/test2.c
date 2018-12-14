#include "types.h"
#include "stat.h"
#include "user.h"

void *thread(void *arg){
	thread_exit((void *)0x87654321);
}

int
main(int argc, char **argv)
{
	void *stack;
	int tid;
	void *retval;

	printf(1, "TEST2: ");

	stack = malloc(4096);
	
	tid = thread_create(thread, 30, (void *)0x12345678, stack);
	if(tid == -1){
		printf(1, "WRONG\n");
		exit();
	}

	if(thread_join(tid, &retval) == -1){
		printf(1, "WRONG\n");
		exit();
	}

	if(retval != (void *)0x87654321){
		printf(1, "WRONG\n");
		exit();
	}

	free(stack);

	printf(1, "OK\n");

	exit();
}
