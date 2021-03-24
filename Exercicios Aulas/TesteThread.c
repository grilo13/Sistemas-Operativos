#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

void *print_message_function( void *ptr );

int main()
{
     pthread_t thread1, thread2;
     int  x1, x2;
     x1 = 11;
     x2 = 22;
     

    /* Create independent threads each of which will execute function */
    pthread_create( &thread1, NULL, print_message_function, (void *) &x1 );
    pthread_create( &thread2, NULL, print_message_function, (void *) &x2 );
     
     pthread_join( thread1, NULL);
     printf("Join Thread 1 \n");

     pthread_join( thread2, NULL);
     printf("Join Thread 2 \n");
    
    
     return 0;
}

void *print_message_function(void *input)
{
     int* z = (int*) input;
     int  w = *z;
     printf("Thread is printing input w is %d ...           \n", w);  
     pthread_exit(NULL);
     
}