#include "types.h"
#include "stat.h"
#include "user.h"

void *thread(void *arg){
	if(arg != (void *)0x12345678){
		printf(1, "WRONG\n");
		exit();
	}

	thread_exit(0);
}

int
main(int argc, char **argv)
{
	void *stack;
	int tid;

	printf(1, "TEST1: ");

	stack = malloc(4096);
	
	tid = thread_create(thread, 10, (void *)0x12345678, stack);
	if(tid == -1){
		printf(1, "WRONG\n");
		exit();
	}

	free(stack);

	printf(1, "OK\n");

	exit();
}
