#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

void *print_message_function( void *ptr );

int main()
{
     pthread_t thread1, thread2;
     int  x;
     int  iret1, iret2;
     

    /* Create independent threads each of which will execute function */
     x=1; 
     iret1 = pthread_create( &thread1, NULL, print_message_function, (void *) &x );
     x=2;
     iret2 = pthread_create( &thread2, NULL, print_message_function, (void *) &x);
     
     pthread_join( thread1, NULL);
     printf("Join Thread 1 \n");
      
     pthread_join( thread2, NULL);
     printf("Join Thread 2 \n");

     exit(0);
     return 0;
}

void *print_message_function(void *input)
{
     int* z = (int*) input;
     int  w = *z;
     printf("Thread is printing input w is %d ...           \n", w);
     printf("Thread is printing input *(int *)input is  %d  \n", *(int *)input);
     
     pthread_exit(NULL);
     
}


// inputs:   x = 1 e x = 2
// outputs:  x = 2 e x = 2

//criámos duas threads, quando as duas threads começam a correr o valor de x é 2 e já não é 1
//pois elas vêem o ultimo valor de x que neste caso era o 2