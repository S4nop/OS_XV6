#include "types.h"
#include "stat.h"
#include "user.h"

#define NTHREAD 7

void *stack[NTHREAD];
int tid[NTHREAD];
void *retval[NTHREAD];

typedef int lock_t;
//lock_t lock;
//int lock;
//int __test_and_set(int *s, int r){
//	__asm__ __volatile__("xchgl %0, %1" : "+m"(*s), "+r"(r));
//	return r;
//}

//void lock_init(int* lock){
//	*lock = 0;
//}

//void lock_acquire(int* lock){
//	while(__test_and_set(lock, 1));
//}

//void lock_release(int *lock){
//	*lock = 0;
//}
void *thread(void *arg){
	int i;
	//lock_acquire(&lock);
	for(i=0;i<10000;i++)
	printf(1, "%d", gettid());
	//lock_release(&lock);
	thread_exit((void *)uptime());
}

int
main(int argc, char **argv)
{
	int i;
	//lock_init(&lock);
	printf(1, "TEST6: ");

	for(i=0;i<NTHREAD;i++)
		stack[i] = malloc(4096);

	for(i=0;i<NTHREAD;i++){
		tid[i] = thread_create(thread, 30+i, 0, stack[i]);
		if(tid[i] == -1){
			printf(1, "WRONG\n");
			exit();
		}
	
	}

	sleep(100);
//for(i = 0; i < NTHREAD; i++){
//	printf(1, "tid listing, %d\n", tid[i]);
//}
	for(i=0;i<NTHREAD;i++){
//		printf(1, "waiting tid: %d\n", tid[i]);
		if(thread_join(tid[i], &retval[i]) == -1){
			printf(1, "WRONG\n");
			exit();
		}
	}
//printf(1, "here?\n");
	for(i=0;i<NTHREAD-1;i++){
		if((int)retval[i] > (int)retval[i+1]){
			printf(1, "WRONG\n");
			exit();
		}
	}

//printf(1, "go on\n");
	for(i=0;i<NTHREAD;i++)
		free(stack[i]);

	printf(1, "OK\n");

	exit();
}
