/*
 *	File	: pc.c
 *
 *	Title	: Demo Producer/Consumer.
 *
 *	Short	: A solution to the producer consumer problem using
 *		pthreads.	
 *
 *	Long 	:
 *
 *	Author	: Andrae Muys
 *
 *	Date	: 18 September 1997
 *
 *	Revised	:
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>

#define QUEUESIZE 15
#define LOOP 100000
#define pNum 1
#define qNum 128
#define functionsNum 2

void *producer(void *tid);

void *consumer(void *tid);

typedef struct {
    void *args;
    struct timeval *tv;
} functionArgument;

typedef struct {
    void *(*work)(void *); // Pointer to function with void pointer as input and void pointer as output

    void *arg; // Void pointer. This is to be used to point to a functionArgument struct
} workFunction;

// Structure representing the queue
typedef struct {
    workFunction **buf; // Queue buffer
    long head, tail; // Boundries
    int full, empty; // Flags for empty, full
    pthread_mutex_t *mut; // Mutex for modifying the queue
    pthread_cond_t *notFull, *notEmpty; // Condition variables for
} queue;

// Structure representing the timer
typedef struct {
    int Period, // Time between the calls of the TimerFcn function, in milliseconds.
    TasksToExecute, // Non-negative integer = how many times is TimerFcn to be called.
    StartDelay, // Non-negative integer = time before the first call of TimerFcn.
    tid; // ID number of the timer/thread.

    void *(*StartFcn)(void *); // Function to be executed during the initialization of the timer.
    void *(*StopFcn)(void *); // Function to be executed before the timer is destroyed.
    void *(*TimerFcn)(void *); // Main function to be executed periodically.
    void *(*ErrorFcn)(void *); // Error function to be executed when FIFO is full.
    void *UserData;
} timer;

queue *fifo; // The queue
int areProducersActive; // Flag whether there is at least one producer thread active
FILE *fp;
int countFull, countEmpty, indexTimes, functionSelection;
long int times[LOOP];

void (*functions[functionsNum])(int);

queue *queueInit(void);

void queueDelete(queue *q);

void queueAdd(queue *q, workFunction *in);

void queueDel(queue *q, workFunction **out);

void consumerPrint(int a);

void consumerCalculate(int a);

int main() {
    pthread_t pro[pNum], con[qNum]; // Producer and Consumer threads
    countEmpty = 0;
    countFull = 0;
    indexTimes = 0;
    functions[0] = &consumerPrint;
    functions[1] = &consumerCalculate;
    functionSelection = 2; // 0 - print, 1 - cos calculation, 2 - both randomly

    fifo = queueInit(); // Initialize the queue, using the function
    if (fifo == NULL) {
        fprintf(stderr, "main: Queue Init failed.\n");
        exit(1);
    }

    // File Creation
    __time_t timestamp;
    time(&timestamp);
    char buffer[25], name[100];
    struct tm *info = localtime(&timestamp);
    strftime(buffer, 25, "%Y_%m_%d_%H_%M_%S", info);
    sprintf(name, "stats/%s_p_%d_q_%d_LOOP_%d_QS_%d_function_%d.txt", buffer, pNum, qNum, LOOP, QUEUESIZE,
            functionSelection);
    //printf("timestamp: %s\n", name);
    fp = fopen(name, "w+");
    if (fp == NULL) {
        fprintf(stderr, "main: File Open failed.\n");
        exit(2);
    }

    // Thread Creation
    for (int tid = 0; tid < pNum; ++tid) {
        pthread_create(&pro[tid], NULL, producer, tid); // Create the Producer thread
    }
    areProducersActive = 1;
    for (int tid = 0; tid < qNum; ++tid) {
        pthread_create(&con[tid], NULL, consumer, tid); // Create the Consumer thread
    }

    // Thread Join
    for (int tid = 0; tid < pNum; ++tid) {
        pthread_join(pro[tid], NULL); // Join  the Producer thread to main thread and wait for its completion
    }
    pthread_mutex_lock(fifo->mut);
    areProducersActive = 0;
    pthread_cond_broadcast(fifo->notEmpty); // In case any of the consumers is condition waiting
    printf("BROADCAST for possible conditional waiting consumers!!!\n");
    pthread_mutex_unlock(fifo->mut);
    for (int tid = 0; tid < qNum; ++tid) {
        pthread_join(con[tid], NULL); // Join  the Consumer thread to main thread and wait for its completion
    }
    queueDelete(fifo);

    for (int i = 0; i < indexTimes; ++i) {
        fprintf(fp, "%ld\n", times[i]);
    }
    printf("\n\ncountEmpty = %d\tcountFull = %d\n", countEmpty, countFull);
    fprintf(fp, "\n\ncountEmpty = %d\tcountFull = %d\n", countEmpty, countFull);
    fclose(fp);

    return 0;
}

void *producer(void *tid) {
    //queue *fifo;
    int i;

    //fifo = (queue *) q;

    for (i = 0; i < LOOP; i++) {
        workFunction *wF = (workFunction *) malloc(sizeof(workFunction)); // workFunction to add malloc
        functionArgument *fArg = (functionArgument *) malloc(sizeof(functionArgument));
        fArg->tv = (struct timeval *) malloc(sizeof(struct timeval));
        fArg->args = i;
        wF->arg = fArg;
        if (functionSelection < functionsNum)
            wF->work = functions[functionSelection];
        else
            wF->work = functions[i % functionsNum];
        pthread_mutex_lock(fifo->mut); // Attempt to lock queue mutex.
        while (fifo->full) { // When lock is acquired check if queue is full
            //printf("producer %d: queue FULL.\n", (int) tid);
            countFull++;
            pthread_cond_wait(fifo->notFull, fifo->mut); // Conditional wait until queue is full NO MORE
        }
        gettimeofday((fArg->tv), NULL);
        queueAdd(fifo, wF);
        //printf("++\n");
        pthread_mutex_unlock(fifo->mut);
        pthread_cond_signal(fifo->notEmpty);
        //pthread_cond_broadcast(fifo->notEmpty);
        //usleep(100000);
    }

    printf("producer %d: RETURNED.\n", (int) tid);
    return (NULL);
}

void *consumer(void *tid) {
    //queue *fifo;
    int i;
    workFunction *d;

    //fifo = (queue *) q;
    while (1) {
        //for (i = 0; i < LOOP; i++) {
        struct timeval now, res;
        pthread_mutex_lock(fifo->mut);
        while (fifo->empty) {
            if (areProducersActive == 0) {
                pthread_mutex_unlock(fifo->mut);
                printf("consumer %d: RETURNED.\n", (int) tid);
                return (NULL);
            }
            //printf("consumer %d: queue EMPTY.\n", (int) tid);
            countEmpty++;
            pthread_cond_wait(fifo->notEmpty, fifo->mut);

        }
        gettimeofday(&now, NULL);
        queueDel(fifo, &d);
        timersub(&now, ((functionArgument *) (d->arg))->tv, &res);
        //fprintf(fp, "%ld\n", res.tv_sec * 1000000 + res.tv_usec);
        //printf("%ld\n", res.tv_sec * 1000000 + res.tv_usec);
        times[indexTimes] = res.tv_sec * 1000000 + res.tv_usec;
        indexTimes++;
        pthread_mutex_unlock(fifo->mut);
        pthread_cond_signal(fifo->notFull);
        //pthread_cond_broadcast(fifo->notFull);
        //printf("consumer %d: recieved %d, after %ld.\n", (int) tid, d->arg, res.tv_sec * 1000000 + res.tv_usec);
        //printf("consumer %d:", (int) tid);
        functionArgument *fATemp = d->arg;
        (*(d->work))(fATemp->args); // Execute function
        //printf(" after %d.\n", res.tv_sec * 1000000 + res.tv_usec);
        free(((functionArgument *) (d->arg))->tv);
        free(d->arg);
        free(d); // workFunction to delete free
//usleep(200000);
    }

    //return (NULL);
}

/*
  typedef struct {
  int buf[QUEUESIZE];
  long head, tail;
  int full, empty;
  pthread_mutex_t *mut;
  pthread_cond_t *notFull, *notEmpty;
  } queue;
*/

queue *queueInit(void) {
    queue *q;

    q = (queue *) malloc(sizeof(queue));
    if (q == NULL) return (NULL);

    q->buf = (workFunction **) malloc(QUEUESIZE * sizeof(workFunction *)); // Buffer malloc
    if (q->buf == NULL) // If buffer malloc failed then return NULL
        return (NULL);

    q->empty = 1;
    q->full = 0;
    q->head = 0;
    q->tail = 0;
    q->mut = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(q->mut, NULL);
    q->notFull = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
    pthread_cond_init(q->notFull, NULL);
    q->notEmpty = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
    pthread_cond_init(q->notEmpty, NULL);

    return (q);
}

void queueDelete(queue *q) {
    pthread_mutex_destroy(q->mut);
    free(q->mut);
    pthread_cond_destroy(q->notFull);
    free(q->notFull);
    pthread_cond_destroy(q->notEmpty);
    free(q->notEmpty);

    free(q->buf); // Buffer free
    free(q);
}

void queueAdd(queue *q, workFunction *in) {
    q->buf[q->tail] = in;
    q->tail++;
    if (q->tail == QUEUESIZE)
        q->tail = 0;
    if (q->tail == q->head)
        q->full = 1;
    q->empty = 0;

    return;
}

void queueDel(queue *q, workFunction **out) {
    *out = q->buf[q->head];

    q->head++;
    if (q->head == QUEUESIZE)
        q->head = 0;
    if (q->head == q->tail)
        q->empty = 1;
    q->full = 0;

    return;
}

void consumerPrint(int a) {
    printf(" received %d\n", a);
}

void consumerCalculate(int a) {
    double temp = 0;

    for (int i = 1; i < 11; ++i) {
        temp += cos(a * i);
    }
    //printf("cosin %d\n", a);
    //printf("cosin %d\t%lf\t%lf\n", a, cos(a), cos(2*a));
}