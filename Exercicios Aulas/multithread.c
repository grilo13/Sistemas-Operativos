#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#define MAX_PROCESSES 50
#define MAX_QUEUES 20

struct queuenode {
    void *data;
    struct queuenode *next;
};

struct queue {
    struct queuenode *front;
    struct queuenode *rear;
    int size;
};

struct queue * initqueue();
void freequeue(struct queue *q);
void enqueue(struct queue *q, void *data);
void dequeue(struct queue *q);
void * front(struct queue *q);

typedef struct queue Queue;

typedef struct {
    pthread_t *tid;
    int pid;
    int t_arr;
    int t_comp;
    int t_elap;
    int t_fin;
} ProcessCtx;

typedef struct {
    ProcessCtx      *p;
    int             timer;
    int             q;
    int             *start;
    int             *end;
    pthread_mutex_t *startlock;
    pthread_cond_t  *startcond;
    pthread_mutex_t *endlock;
    pthread_cond_t  *endcond;
} ThreadArgs;

typedef struct {
    int        nb_queues;
    int        nb_processes;
    int        t_boost;
    ProcessCtx process[MAX_PROCESSES];
    Queue      *queue[MAX_QUEUES];
    int        t_slice[MAX_QUEUES];
    int        timer;
    int        proc_fin;    ///> number of processes finished
    int        proc_added;  ///> index of last process arrived
    pthread_mutex_t startlock;
    pthread_mutex_t endlock;
    pthread_cond_t  startcond;
    pthread_cond_t  endcond;
    int        *start;
    int        *end;
} MLFQCtx;

void scheduler(MLFQCtx *m);

void Pthread_mutex_lock(pthread_mutex_t *lock);
void Pthread_mutex_unlock(pthread_mutex_t *lock);
void Pthread_cond_wait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict lock);
void Pthread_cond_signal(pthread_cond_t *restrict cond);

struct queue * initqueue()
{
    struct queue *q = malloc(sizeof(struct queue));
    q->front = q->rear = NULL;
    q->size = 0;
    return q;
}

void freequeue(struct queue *q)
{
    struct queuenode *temp;
    while ((temp = front(q))) {
        dequeue(q);
    }
    free(q);
}

void enqueue(struct queue *q, void *data)
{
    struct queuenode *temp = malloc(sizeof(struct queuenode));
    temp->data = data;
    temp->next = NULL;
    q->size++;
    if (q->front == NULL && q->rear == NULL) {
        q->front = q->rear = temp;
        return;
    }
    q->rear->next = temp;
    q->rear = temp;
}

void dequeue(struct queue *q)
{
    struct queuenode *temp = q->front;
    if (q->front == NULL) {
        return;
    }
    q->size--;
    if (q->front == q->rear) {
        q->front = q->rear = NULL;
    } else {
        q->front = q->front->next;
    }
    free(temp);
}

void * front(struct queue *q)
{
    if(q->front == NULL) {
        return NULL;
    }
    return q->front->data;
}

void threadargs_init(ThreadArgs *args, ProcessCtx *curr, MLFQCtx *m)
{
    args->p = curr;
    args->startlock = &m->startlock;
    args->endlock = &m->endlock;
    args->startcond = &m->startcond;
    args->endcond = &m->endcond;
    args->start = m->start;
    args->end = m->end;
}

void * thread_routine(void *args)
{
    ThreadArgs *t = (ThreadArgs *) args;
    while (t->p->t_elap < t->p->t_comp) {
        // Wait for signal to run
        Pthread_mutex_lock(t->startlock);
        while (*t->start == 0)
            Pthread_cond_wait(t->startcond, t->startlock);
        *t->start = 0;
        Pthread_mutex_unlock(t->startlock);

        // Give signal to end
        Pthread_mutex_lock(t->endlock);
        *t->end = 1;
        Pthread_cond_signal(t->endcond);
        Pthread_mutex_unlock(t->endlock);
    }
    return NULL;
}

void mlfq_boost(MLFQCtx *m)
{
    int i;
    for (i = 0; i < m->nb_queues; i++) {
        if (m->queue[i]) {
            freequeue(m->queue[i]);
        }
        m->queue[i] = initqueue();
    }
    for (i = 0; i < m->nb_processes; i++) {
        ProcessCtx *p = &m->process[i];
        if (p->t_arr <= m->timer) {
            enqueue(m->queue[m->nb_queues - 1], p);
            m->proc_added = i;
        }
    }
}

void print_mlfq(MLFQCtx *m)
{
    int i;
    struct queuenode *curr;
    ProcessCtx *p;
    printf("TIME %d\n", m->timer);
    printf("----------------\n");
    for (i = m->nb_queues - 1; i >= 0; i--) {
        curr = m->queue[i]->front;
        printf("Q%d: ", i);
        while (curr) {
            if (curr == m->queue[i]->front) printf("<Front>");
            p = (ProcessCtx *) curr->data;
            printf("[ pid %d ]", p->pid);
            if (curr != m->queue[i]->rear)
                printf("-->");
            else
                printf("<Rear>");
            curr = curr->next;
        }
        printf("\n");
    }
    printf("----------------\n\n");
}

int runnable_queue(MLFQCtx *m)
{
    int ret = -1;
    int i;
    for (i = m->nb_queues - 1; i >= 0; i--) {
        if (m->queue[i]->size > 0) {
            ret = i;
            break;
        }
    }
    return ret;
}

void scheduler(MLFQCtx *m)
{
    ProcessCtx *prev = NULL, *curr;
    int q;
    int t_switch = 0; ///> time at which last context switch happened
    int i;
    while (m->proc_fin < m->nb_processes) {
        // Boost processes to top-queue
        if (m->timer % m->t_boost == 0) {
            t_switch = m->timer;
            mlfq_boost(m);
            printf("BOOST OCCURED - ");
            print_mlfq(m);
        }

        // Run process for one time unit
        q = runnable_queue(m);
        curr = (ProcessCtx *) front(m->queue[q]);
        assert(curr != NULL);
        if (curr != prev) {
            t_switch = m->timer;
        }
        /*printf("time: %d, queue: %d, pid: %d, t_elap: %d\n",
               m->timer, q, curr->pid, curr->t_elap);*/

        // Resume process thread
        if (!curr->tid) {
            ThreadArgs *args = malloc(sizeof(ThreadArgs));
            curr->tid = malloc(sizeof(pthread_t));
            threadargs_init(args, curr, m);
            int rc = pthread_create(curr->tid, NULL, thread_routine, args);
            assert(rc == 0);
        }
        // Give signal to start
        Pthread_mutex_lock(&m->startlock);
        *m->start = 1;
        Pthread_cond_signal(&m->startcond);
        Pthread_mutex_unlock(&m->startlock);
        // Wait for end signal
        Pthread_mutex_lock(&m->endlock);
        while (*m->end== 0)
            Pthread_cond_wait(&m->endcond, &m->endlock);
        *m->end = 0;
        Pthread_mutex_unlock(&m->endlock);


        // Increment Process timer
        curr->t_elap += 1;
        prev = curr;

        // Increment Global Timer
        m->timer += 1;

        // Check if process is complete
        if (curr->t_elap >= curr->t_comp) {
            dequeue(m->queue[q]);
            curr->t_fin = m->timer;
            m->proc_fin++;
            printf("PROCESS %d FINISHED - ", curr->pid);
            print_mlfq(m);
        } // Check if slice expired
        else if (m->timer - t_switch >= m->t_slice[q]) {
            t_switch = m->timer;
            dequeue(m->queue[q]);
            if (q - 1 >= 0) q = q - 1;
            enqueue(m->queue[q], curr);
            printf("PROCESS %d TIMESLICE OVER - ", curr->pid);
            print_mlfq(m);
        }

        // Add new processes
        for (i = m->proc_added + 1; i < m->nb_processes; i++) {
            ProcessCtx *p = &m->process[i];
            if (p->t_arr <= m->timer) {
                enqueue(m->queue[m->nb_queues - 1], p);
                m->proc_added = i;
                printf("PROCESS %d ARRIVED - ", p->pid);
                print_mlfq(m);
            }
        }
    }
}

void Pthread_mutex_lock(pthread_mutex_t *lock)
{
    int ret;
    ret = pthread_mutex_lock(lock);
    assert(ret == 0);
}

void Pthread_mutex_unlock(pthread_mutex_t *lock)
{
    int ret;
    ret = pthread_mutex_unlock(lock);
    assert(ret == 0);
}

void Pthread_cond_wait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict lock)
{
    int ret;
    ret = pthread_cond_wait(cond, lock);
    assert(ret == 0);
}

void Pthread_cond_signal(pthread_cond_t *restrict cond)
{
    int ret;
    ret = pthread_cond_signal(cond);
    assert(ret == 0);
}

void mlfq_init(MLFQCtx *m)
{
    m->timer = 0;
    m->proc_fin = 0;
    m->proc_added = -1;
    pthread_mutex_init(&m->startlock, NULL);
    pthread_mutex_init(&m->endlock, NULL);
    pthread_cond_init(&m->startcond, NULL);
    pthread_cond_init(&m->endcond, NULL);
    m->start = malloc(sizeof(int));
    m->end = malloc(sizeof(int));
    *m->start = 0;
    *m->end = 0;
}

int process_cmp(const void *a, const void *b)
{
    ProcessCtx *p1 = (ProcessCtx *) a;
    ProcessCtx *p2 = (ProcessCtx *) b;
    if (p1->t_arr != p2->t_arr) {
        return p1->t_arr - p2->t_arr;
    } else {
        return p1->pid - p2->pid;
    }
}

int queue_cmp(const void *a, const void *b)
{
    int *tslice1 = (int *) a;
    int *tslice2 = (int *) b;
    return tslice2 - tslice1;
}

void read_config(MLFQCtx *m, FILE *config)
{
    int i;
    fscanf(config, "%d", &m->nb_queues);
    assert(m->nb_queues <= MAX_QUEUES);
    for (i = 0; i < m->nb_queues; i++) {
        fscanf(config, "%d", &m->t_slice[i]);
        m->queue[i] = NULL;
    }
    fscanf(config, "%d", &m->t_boost);
    fscanf(config, "%d", &m->nb_processes);
    assert(m->nb_processes <= MAX_PROCESSES);
    for (i = 0; i < m->nb_processes; i++) {
        fscanf(config, "%d", &m->process[i].t_arr);
        fscanf(config, "%d", &m->process[i].t_comp);
        m->process[i].pid    = i + 1;
        m->process[i].tid    = NULL;
        m->process[i].t_elap = 0;
        m->process[i].t_fin  = 0;
    }
}

int main(int argc, char **argv)
{
    //assert(argc == 2);
    FILE *config = fopen(argv[1], "r");
    assert(config != NULL);
    MLFQCtx *m = malloc(sizeof(MLFQCtx));
    mlfq_init(m);
    read_config(m, config);

    qsort(m->process, m->nb_processes, sizeof(ProcessCtx), process_cmp);
    qsort(m->t_slice, m->nb_queues, sizeof(int), queue_cmp);

    int i;

    for (i = 0; i < m->nb_queues; i++) {
        printf("Q%d: %d\n", i, m->t_slice[i]);
    }
    for (i = 0; i < m->nb_processes; i++) {
        ProcessCtx *p = &m->process[i];
        printf("Process %d: t_arr - %d, t_comp - %d\n", p->pid, p->t_arr, p->t_comp);
    }
    printf("\n");

    scheduler(m);

    for (i = 0; i < m->nb_processes; i++) {
        ProcessCtx *p = &m->process[i];
        printf("Process %d: turnaround - %d\n", p->pid, p->t_fin - p->t_arr);
    }
    return 0;
}
