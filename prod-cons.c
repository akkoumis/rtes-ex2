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
#define LOOP 360000
#define pNum 3 // Number of PRODUCER threads
#define qNum 8 // Number of CONSUMER threads
#define functionsNum 2 // Size of the FUNCTION POOL

void *producer(void *timer);

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
    timerID; // ID number of the timer/thread.

    void *(*StartFcn)(void *); // Function to be executed during the initialization of the timer.
    void *(*StopFcn)(void *); // Function to be executed before the timer is destroyed.
    void *(*TimerFcn)(void *); // Main function to be executed periodically.
    void *(*ErrorFcn)(void *); // Error function to be executed when FIFO is full.
    void *UserData;
} timer;

queue *fifo; // The queue instance
int areProducersActive; // Flag whether there is at least one producer thread active
FILE *consumer_stats_file, *producer_stats_file[pNum];
int countFull, countEmpty, indexProducerTimes[pNum], indexConsumerTimes, functionSelection, timersCounter;
long int producer_times[pNum][LOOP], consumer_times[LOOP];
int timer_max_loop[] = {360000, 36000, 3600};
char tick0[] = "Tick0!\n";
char tick2[] = "Tick2!\n";

void (*functions[functionsNum])(int);

queue *queueInit(void);

void queueDelete(queue *q);

void queueAdd(queue *q, workFunction *in);

void queueDel(queue *q, workFunction **out);

void consumerPrint(int a);

void consumerCalculate(int a);

void timerInit(timer *t);

void start(timer *t);

void startat(timer *t, int y, int m, int d, int h, int min, int sec);

void print_message(void *str);

void sampleStartFCN(int a);

void sampleStopFCN(int a);

void sampleErrorFCN(int a);

pthread_t pro[pNum], con[qNum]; // Producer and Consumer threads

int main() {
    // Initialize counters
    countEmpty = 0;
    countFull = 0;
    for (int tid = 0; tid < pNum; ++tid) {
        indexProducerTimes[tid] = 0;
    }
    indexConsumerTimes = 0;
    timersCounter = 0;

    // Set up FUNCTION POOL
    functions[0] = &consumerPrint;
    functions[1] = &consumerCalculate;
    functionSelection = 2; // 0 - print, 1 - cos calculation, 2 - both randomly

    // Initialize the queue, using the provided function
    fifo = queueInit();
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
    // Consumer file
    sprintf(name, "stats/cons_%s_p_%d_q_%d_LOOP_%d_QS_%d_function_%d.txt", buffer, pNum, qNum, LOOP, QUEUESIZE,
            functionSelection);
    //printf("timestamp: %s\n", name);
    consumer_stats_file = fopen(name, "w+");
    if (consumer_stats_file == NULL) {
        fprintf(stderr,
                "main: Consumer File Open failed. Make sure there is a \"stats\" directory where the executable is.\n");
        exit(2);
    }
    // Producer file
    for (int tid = 0; tid < pNum; ++tid) {
        sprintf(name, "stats/prod_%d_%s_p_%d_q_%d_LOOP_%d_QS_%d_function_%d.txt", tid, buffer, pNum, qNum, LOOP,
                QUEUESIZE,
                functionSelection);
        //printf("timestamp: %s\n", name);
        producer_stats_file[tid] = fopen(name, "w+");
        if (producer_stats_file == NULL) {
            fprintf(stderr,
                    "main: Producer File Open failed. Make sure there is a \"stats\" directory where the executable is.\n");
            exit(2);
        }
    }

    int testDurationInSeconds = 3600;

    // Thread Creation
    //for (int tid = 0; tid < pNum; ++tid) {
    //    pthread_create(&pro[tid], NULL, producer, tid); // Create the Producer thread
    //}
    areProducersActive = 1;
    for (int tid = 0; tid < qNum; ++tid) {
        pthread_create(&con[tid], NULL, consumer, tid); // Create the Consumer thread
    }

    // Create Timers
    timer timer0;
    timer0.Period = 10;
    timer0.TasksToExecute = testDurationInSeconds * 100;
    timer0.StartDelay = 0;
    timer0.StartFcn = &sampleStartFCN;
    timer0.StopFcn = &sampleStopFCN;
    timer0.TimerFcn = &consumerCalculate;
    timer0.ErrorFcn = &sampleErrorFCN;
    timer0.UserData = 10;
    start(&timer0);

    timer timer1;
    timer1.Period = 100;
    timer1.TasksToExecute = testDurationInSeconds * 10;
    timer1.StartDelay = 0;
    timer1.StartFcn = &sampleStartFCN;
    timer1.StopFcn = &sampleStopFCN;
    timer1.TimerFcn = &consumerCalculate;
    timer1.ErrorFcn = &sampleErrorFCN;
    timer1.UserData = 20;
    start(&timer1);

    timer timer2;
    timer2.Period = 1000;
    timer2.TasksToExecute = testDurationInSeconds;
    timer2.StartDelay = 0;
    timer2.StartFcn = &sampleStartFCN;
    timer2.StopFcn = &sampleStopFCN;
    timer2.TimerFcn = &consumerCalculate;
    timer2.ErrorFcn = &sampleErrorFCN;
    timer2.UserData = 30;
    start(&timer2);


    // Thread Join
    //for (int tid = 0; tid < pNum; ++tid) {
    //    pthread_join(pro[tid], NULL); // Join  the Producer thread to main thread and wait for its completion
    //}
    //pthread_mutex_lock(fifo->mut);
    //areProducersActive = 0;
    //pthread_cond_broadcast(fifo->notEmpty); // In case any of the consumers is condition waiting
    //printf("BROADCAST for possible conditional waiting consumers!!!\n");
    //pthread_mutex_unlock(fifo->mut);

    // Thread Join
    for (int tid = 0; tid < timersCounter; ++tid) {
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


    // Print CONSUMER times
    for (int i = 0; i < indexConsumerTimes; ++i) {
        fprintf(consumer_stats_file, "%ld\n", consumer_times[i]);
    }
    // Print PRODUCER times
    for (int tid = 0; tid < timersCounter; ++tid) {
        for (int i = 0; i < indexProducerTimes[tid]; ++i) {
            fprintf(producer_stats_file[tid], "%ld\n", producer_times[tid][i]);
        }
    }

    printf("\n\ncountEmpty = %d\tcountFull = %d\n", countEmpty, countFull);
    fprintf(consumer_stats_file, "\n\ncountEmpty = %d\tcountFull = %d\n", countEmpty, countFull);
    fclose(consumer_stats_file);
    for (int tid = 0; tid < timersCounter; ++tid) {
        fclose(producer_stats_file[tid]);
    }

    //printf("\n28/9/20 15:36\n");

    return 0;
}

void timerInit(timer *t) {
    pthread_mutex_lock(fifo->mut);
    t->timerID = timersCounter;
    pthread_create(&pro[t->timerID], NULL, producer, t); // Create the Producer thread
    timersCounter++;
    countEmpty = 0;
    pthread_mutex_unlock(fifo->mut);
}

void start(timer *t) {
    // Initialize the timer
    t->StartDelay = 0;
    timerInit(t);
}

void startat(timer *t, int y, int m, int d, int h, int min, int sec) {
    // Calculate the time until initialization and add info to StartDelay of timer t
    time_t now_t, future_t;
    struct tm future_tm;
    double differnce_in_seconds;

    future_tm.tm_year = y - 1900;
    future_tm.tm_mon = m - 1;
    future_tm.tm_mday = d;
    future_tm.tm_hour = h;
    future_tm.tm_min = min;
    future_tm.tm_sec = sec;
    future_tm.tm_isdst = -1;
    future_t = mktime(&future_tm);
    if (future_t == -1) {
        printf("Error: unable to make time using mktime. Aborting initialization of timer...\n");
        return;
    }

    time(&now_t);

    differnce_in_seconds = difftime(future_t, now_t);
    if (differnce_in_seconds < 0) {
        printf("Please provide a future timestamp for the timer, not a past one. Aborting initialization of timer...\n");
        return;
    }
//    printf("Difference in seconds = %f\n", differnce_in_seconds);

    // Initialize the timer
    t->StartDelay += (int) differnce_in_seconds;
    timerInit(t);
}

void *producer(void *t) {
    //queue *fifo;
    int i, drift_difference = 0;
    timer *timer_casted = (timer *) t;
    struct timeval now, before, res;
    char tempString[100];

    //Execute StartFcn
    (*(timer_casted->StartFcn))(timer_casted->UserData);

    // Wait for the StartDelay to pass
    printf("Producer %d started.\n", timer_casted->timerID);
    usleep(timer_casted->StartDelay * 1000000);
    //gettimeofday(&before, NULL);
    before = (struct timeval) {0};


    for (i = 0; i < timer_casted->TasksToExecute; i++) {

        // Create thw workFunction object.
        workFunction *wF = (workFunction *) malloc(sizeof(workFunction)); // workFunction to add malloc
        functionArgument *fArg = (functionArgument *) malloc(sizeof(functionArgument));
        fArg->tv = (struct timeval *) malloc(sizeof(struct timeval));
        fArg->args = timer_casted->UserData; // Data necessary for the function.
        wF->arg = fArg;
        wF->work = timer_casted->TimerFcn;

        // Add work to queue.
        pthread_mutex_lock(fifo->mut); // Attempt to lock queue mutex.
        // Execute ErrorFcn when queue is full
        if (fifo->full) { // When lock is acquired check if queue is full
            //printf("producer %d: queue FULL.\n", (int) timer_casted->tid);
            countFull++;
            //pthread_cond_wait(fifo->notFull, fifo->mut); // Conditional wait until queue is full NO MORE

            // Free pointers of unused wF because ti wont be added to queue
            free(((functionArgument *) (wF->arg))->tv);
            free(wF->arg);
            free(wF); // workFunction to delete free

            // Execute ErrorFcn
            (*(timer_casted->ErrorFcn))(timer_casted->UserData);
        } else {
            gettimeofday((fArg->tv),
                         NULL); // Time to be passed to the consumer, for calculating duration of stay in queue
            queueAdd(fifo, wF);
        }

        gettimeofday(&now, NULL); // Time for queue-insertion period calculation
        if (before.tv_sec != 0) {
            timersub(&now, &before, &res); // Calculate time beteween queue-insertions
            // Update producer_times with the amount of time since last queue-insertion
            producer_times[timer_casted->timerID][indexProducerTimes[timer_casted->timerID]] =
                    res.tv_sec * 1000000 + res.tv_usec;
            indexProducerTimes[timer_casted->timerID]++;

            // Drift Correction
            if ( timer_casted->Period * 1000 > 150)
                drift_difference = 150;
            else
                drift_difference = timer_casted->Period * 1000;
        }
        before = now;
        //printf("Job added to queue after %ld useconds.\n", res.tv_sec * 1000000 + res.tv_usec);

        pthread_mutex_unlock(fifo->mut);
        pthread_cond_signal(fifo->notEmpty);
        //pthread_cond_broadcast(fifo->notEmpty);

        if (i == timer_casted->TasksToExecute - 1)
            break;

        // Sleep for a time specified in Period
        usleep((timer_casted->Period * 1000) - drift_difference);
    }

    //Execute StartFcn
    (*(timer_casted->StopFcn))(timer_casted->UserData);

    printf("producer %d: RETURNED.\n", (int) timer_casted->timerID); // Prints the ID the timer
    return (NULL);
}

void *consumer(void *tid) {
    //queue *fifo;
    int i;
    workFunction *wF;

    //fifo = (queue *) q;
    while (1) {
        //for (i = 0; i < LOOP; i++) {

        // Remove work from queue
        struct timeval now, res;
        pthread_mutex_lock(fifo->mut); // Try to get the mutex lock.
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
        queueDel(fifo, &wF);
        timersub(&now, ((functionArgument *) (wF->arg))->tv, &res);
        //fprintf(consumer_stats_file, "%ld\n", res.tv_sec * 1000000 + res.tv_usec);
        //printf("%ld\n", res.tv_sec * 1000000 + res.tv_usec);

        // Update consumer_times with the amount of time the work stayed in the queue
        consumer_times[indexConsumerTimes] = res.tv_sec * 1000000 + res.tv_usec;
        indexConsumerTimes++;
        pthread_mutex_unlock(fifo->mut); // Unlock mutex
        pthread_cond_signal(fifo->notFull);
        //pthread_cond_broadcast(fifo->notFull);
        //printf("consumer %d: recieved %d, after %ld useconds.\n", (int) tid, wF->arg,res.tv_sec * 1000000 + res.tv_usec);

        //printf("consumer %d:", (int) tid);
        functionArgument *fATemp = wF->arg;

        // Execute function
        (*(wF->work))(fATemp->args);
        //printf(" after %d.\n", res.tv_sec * 1000000 + res.tv_usec);
        free(((functionArgument *) (wF->arg))->tv);
        free(wF->arg);
        free(wF); // workFunction to delete free
        //usleep(200000);
    }

    //return (NULL);
}


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

void print_message(void *str) {
    printf("%s\n", str);
}

void sampleStartFCN(int a) {
    printf("StartFcn in Producer with User Data = %d\n", a);
}

void sampleStopFCN(int a) {
    printf("StopFcn in Producer with User Data = %d\n", a);
}

void sampleErrorFCN(int a) {
    printf("ErrorFcn in Producer with User Data = %d\n", a);
}