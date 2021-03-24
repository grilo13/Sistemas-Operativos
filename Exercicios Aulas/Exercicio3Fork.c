#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>


/*Há dois outputs possiveis, e por isso alternados, pois o processo pai e o processo filho correm ao mesmo tempo um que o outro.
Então nos não sabemos quando o SO vai dar o controlo ao processo pai ou ao processo filho*/

/*Eles dao run no mesmo programa, mas sao diferentes(pai e filho). O SO aloca diferentes datas e diferentes estados para os dois processos
, e o controlo dos dois pode ser diferente*/

#define MAX_COUNT 200

void ChildProcess(void); /* child process prototype */
void ParentProcess(void); /* parent process prototype */

void main(void)
{
 pid_t pid;

 pid = fork();
 if (pid == 0)
 ChildProcess();
 else
 ParentProcess();
}

void ChildProcess(void)
{
 int i;

 for (i = 1; i <= MAX_COUNT; i++)
 printf(" This line is from child, value = %d\n", i);
 printf(" *** Child process is done ***\n");
}

void ParentProcess(void)
{
 int i;

 for (i = 1; i <= MAX_COUNT; i++)
 printf("This line is from parent, value = %d\n", i);
 printf("*** Parent is done ***\n");
}