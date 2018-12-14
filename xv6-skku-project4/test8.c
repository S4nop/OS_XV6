#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define NTHREAD 7

void *stack[NTHREAD];
int tid[NTHREAD];
void *retval[NTHREAD];

void *thread1(void *arg){
	int *fd = (int *)arg;

	*fd = open("testfile", O_CREATE|O_RDWR);
	if(*fd < 0){
		printf(1, "WRONG\n");
		exit();
	}

	if(write(*fd, "hello", 5) != 5){
		printf(1, "WRONG\n");
		exit();
	}
//printf(1, "write 1 & file: %d\n", proc->ofile[0]->off);
	thread_exit(0);
}

void *thread2(void *arg){
	int fd = (int)arg;
	int tmp;
//printf(1, "write 2 %d\n", fd);
	if((tmp = write(fd, "world", 5)) != 5){
		printf(1, "WRONG %d\n", tmp);
		exit();
	}

	close(fd);

	thread_exit(0);
}

int
main(int argc, char **argv)
{
	int i;
	int fd;
	char buf[100];

	printf(1, "TEST8: ");

	for(i=0;i<NTHREAD;i++)
		stack[i] = malloc(4096);

	if((tid[0] = thread_create(thread1, 10, (void *)&fd, stack[0])) == -1){
		printf(1, "WRONG\n");
		exit();
	}

//printf(1, "1 & file: %d\n", proc->ofile[0]->off);
	if(thread_join(tid[0], &retval[0]) == -1){
		printf(1, "WRONG\n");
		exit();
	}
//printf(1, "2 & file: %d\n", proc->ofile[0]->off);
	if((tid[1] = thread_create(thread2, 10, (void *)fd, stack[1])) == -1){
		printf(1, "WRONG\n");
		exit();
	}
//printf(1, "3\n");
	if(thread_join(tid[1], &retval[1]) == -1){
		printf(1, "WRONG\n");
		exit();
	}
//printf(1, "4\n");

	fd = open("testfile", O_RDONLY);
	if(read(fd, buf, 10) != 10){
		printf(1, "WRONG\n");
		exit();
	}
//printf(1, "5\n");

	if(strcmp(buf, "helloworld") != 0){
		printf(1, "WRONG\n");
		exit();
	}
//printf(1, "6\n");

	for(i=0;i<NTHREAD;i++)
		free(stack[i]);

	printf(1, "OK\n");

	exit();
}
