/* CSC 360 WINTER 2020 */
/* Assignment #2 */
/* Student name: Yuying Zhang (Nina) */
/* Student #: V00924070 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>

#define MAX_INPUT 1024
#define MAX_THREADS 500
#define DS_TO_MS 100000
#define FALSE 0
#define TRUE 1

/* *************** { Typedefs } *************** */

typedef struct customer
{
    int id;
    int class; // 0 = economy 1 = business
    float arrivalTime;
    float serviceTime;
} customer;

typedef struct waitEntry
{
    int class; // 0 = economy 1 = business
    double waitTime;
} waitEntry;

typedef struct clerk
{
    int id;
    int status;   // 0 = idle 1 = busy
    int signaled; // 0 = economy 1 = business
} clerk;

/* *************** { Pthread and global declarations  } *************** */

struct timeval initTime; // Records the simulation start time
int numOfCustomers;
pthread_t customerThreads[MAX_THREADS];
pthread_t clerkThreads[4];

waitEntry completeWaitingTime[MAX_THREADS]; // Records waiting times for customers
int waitEntryIndex = 0;
pthread_mutex_t waitingTimeMutex; // Wait time entry lock

int economyQueueList[MAX_THREADS]; // The economy class queue
int economyQueueLength = 0;
pthread_mutex_t economyQueue;       // Economy queue entry lock
int businessQueueList[MAX_THREADS]; // The business class queue
int businessQueueLength = 0;
pthread_mutex_t businessQueue; // Business queue entry lock

clerk clerks[4];
int selectedQueue = 0; // To indicate which queue has been selected for processing
pthread_mutex_t selectedQueueMutex;
pthread_mutex_t clerkStatusMutex; // For updating clerk status
pthread_cond_t clerkAvailable;    // To annouce whenever a clerk is idle
pthread_cond_t clerk0Finished;
pthread_cond_t clerk1Finished;
pthread_cond_t clerk2Finished;
pthread_cond_t clerk3Finished;

/* *************** { Time helper functions  } *************** */

/* Returns the system time difference in seconds between a passed start time to function call */
double getTimeDifference(struct timeval startTime)
{
    struct timeval nowTime;
    gettimeofday(&nowTime, NULL);
    long nowMicroseconds = (nowTime.tv_sec * 10 * DS_TO_MS) + nowTime.tv_usec;
    long startMicroseconds = (startTime.tv_sec * 10 * DS_TO_MS) + startTime.tv_usec;
    return (double)(nowMicroseconds - startMicroseconds) / (10 * DS_TO_MS);
}

/* Returns the system time in terms of seconds */
double getTime()
{
    struct timeval nowTime;
    gettimeofday(&nowTime, NULL);
    long nowMicroseconds = (nowTime.tv_sec * 10 * DS_TO_MS) + nowTime.tv_usec;
    long startMicroseconds = (initTime.tv_sec * 10 * DS_TO_MS) + initTime.tv_usec;
    return (double)(nowMicroseconds - startMicroseconds) / (10 * DS_TO_MS);
}

/* *************** { Input file processing functions  } *************** */

void replaceColons(char c[])
{
    int i = 0;
    while (c[i] != '\0')
    {
        if (c[i] == ':')
        {
            c[i] = ',';
        }
        i++;
    }
}

float generateRand(int lower, int upper)
{
    srand((unsigned int)time(0));
    float num = (float)((rand() % (upper - lower + 1)) + lower) / 100;
    return num;
}

int inArrivalTimes(float time, float arrivalTimes[numOfCustomers])
{
    for (int i = 0; i < numOfCustomers; i++)
    {
        if (arrivalTimes[i] == time)
        {
            return 1;
        }
    }
    return 0;
}

/* Reads input file and parses the information into customer type entries */
void fileReader(char *path, char fileContents[numOfCustomers][MAX_INPUT], customer customerList[numOfCustomers])
{
    FILE *fp;
    fp = fopen(path, "r");

    int i = 0;
    while (fgets(fileContents[i], MAX_INPUT, fp) != NULL)
    {
        //  Eliminates newline character at end of input line
        char *pos;
        if ((pos = strchr(fileContents[i], '\n')) != NULL)
        {
            *pos = '\0';
        }
        i++;
    }
    fclose(fp);

    float arrivalTimes[numOfCustomers];
    for (int i = 1; i <= numOfCustomers; i++)
    {

        //  Replaces all instances of colons with comma
        replaceColons(fileContents[i]);

        //  Removes the commas and creates a customer structure for the data
        int idx = 0;
        int customerData[4];
        char *elem = strtok(fileContents[i], ",");
        while (elem != NULL)
        {
            customerData[idx] = atoi(elem);
            elem = strtok(NULL, ",");
            idx++;
        }

        float at = (float)customerData[2];
        float st = (float)customerData[3];

        if (inArrivalTimes(at, arrivalTimes) == 1)
        {
            at = at + generateRand(5, 25);
            arrivalTimes[i - 1] = at;
        }
        else
        {
            arrivalTimes[i - 1] = at;
        }

        customer current = {
            customerData[0], // Unique ID
            customerData[1], // Customer class
            at,              // Arrival time
            st,              // Service time
        };

        customerList[i - 1] = current;
    }
}

/* *************** { Clerk waiting processing functions  } *************** */

void removeFromQueue(int class)
{
    if (class == 0)
    {
        int x = 0;
        while (x < economyQueueLength - 1)
        {
            economyQueueList[x] = economyQueueList[x + 1];
            x += 1;
        }
        economyQueueLength -= 1;
    }
    else
    {
        int x = 0;
        while (x < businessQueueLength - 1)
        {
            businessQueueList[x] = businessQueueList[x + 1];
            x += 1;
        }
        businessQueueLength -= 1;
    }
}

int findClerk(int class)
{
    for (int i = 0; i < 4; i++)
    {
        if (clerks[i].status == 0 && clerks[i].signaled == class)
        {
            return clerks[i].id;
        }
    }
    return -1;
}

int clerkNotTaken()
{
    for (int i = 0; i < 4; i++)
    {
        if (clerks[i].status == 0)
        {
            return 1;
        }
    }
    return 0;
}

/* Direct customers to the appropriate queue and alerts customers when they are signaled for processing */
int waitForClerk(customer *currCustomer)
{
    int clerkSignal;

    //  This is the ecnonomy class queue
    if (currCustomer->class == 0)
    {
        if (pthread_mutex_lock(&economyQueue) != 0)
        {
            printf("[ Error: failed to lock mutex ]\n");
        }
        economyQueueList[economyQueueLength] = currCustomer->id; // Enters in queue
        economyQueueLength++;
        printf("A customer enters the economy queue with ID %d, the length of the queue is %d. \n", currCustomer->id, economyQueueLength);

        /*  The customer will wait until they are at head of respective queue and the queue was selected for processing and the clerk that
            signaled was not taken */
        while (TRUE)
        {
            if (pthread_cond_wait(&clerkAvailable, &economyQueue) != 0)
            {
                printf("[ Error: failed to execute condition variable ]\n");
            }
            if (currCustomer->id == economyQueueList[0] && selectedQueue == 0 && clerkNotTaken())
            {
                //  Updates clerk status and gets signaling clerk ID
                clerkSignal = findClerk(0);
                if (pthread_mutex_lock(&clerkStatusMutex) != 0)
                {
                    printf("[ Error: failed to lock mutex ]\n");
                }
                clerks[clerkSignal].status = 1;
                if (pthread_mutex_unlock(&clerkStatusMutex) != 0)
                {
                    printf("[ Error: failed to unlock mutex ]\n");
                }
                break;
            }
        }
        removeFromQueue(0); // Removed from queue
        if (pthread_mutex_unlock(&economyQueue) != 0)
        {
            printf("[ Error: failed to unlock mutex ]\n");
        }
        return clerkSignal;
    }
    else //  This is the business class queue
    {
        if (pthread_mutex_lock(&businessQueue) != 0)
        {
            printf("[ Error: failed to lock mutex ]\n");
        }
        businessQueueList[businessQueueLength] = currCustomer->id; // Enters in queue
        businessQueueLength++;
        printf("A customer enters the business queue with ID %d, the length of the queue is %d. \n", currCustomer->id, businessQueueLength);

        /*  The customer will wait until they are at head of respective queue and the queue was selected for processing and the clerk that
            signaled was not taken */
        while (TRUE)
        {
            if (pthread_cond_wait(&clerkAvailable, &businessQueue) != 0)
            {
                printf("[ Error: failed to execute condition variable ]\n");
            }
            if (currCustomer->id == businessQueueList[0] && selectedQueue == 1 && clerkNotTaken())
            {
                //  Updates clerk status and gets signaling clerk ID
                clerkSignal = findClerk(1);
                if (pthread_mutex_lock(&clerkStatusMutex) != 0)
                {
                    printf("[ Error: failed to lock mutex ]\n");
                }
                clerks[clerkSignal].status = 1;
                if (pthread_mutex_unlock(&clerkStatusMutex) != 0)
                {
                    printf("[ Error: failed to unlock mutex ]\n");
                }
                break;
            }
        }
        removeFromQueue(1); // Removed from queue
        if (pthread_mutex_unlock(&businessQueue) != 0)
        {
            printf("[ Error: failed to unlock mutex ]\n");
        }
        return clerkSignal;
    }
}

/* *************** { Customer Thread  } *************** */

void *customerEntry(void *customerInfo)
{
    customer *currCustomer = (customer *)customerInfo;
    struct timeval waitingTime;

    usleep(currCustomer->arrivalTime * DS_TO_MS); // Simulates arrival time

    printf("A customer arrives: Customer ID %d. \n", currCustomer->id);

    gettimeofday(&waitingTime, NULL); // Starts waiting time

    int clerkSignaled = waitForClerk(currCustomer); // Enters into the queue till clerk signals

    double overallWaitingTime = getTimeDifference(waitingTime); // Ends waiting time

    usleep(10);

    double startTime = getTime();
    printf("A clerk starts serving a customer: Start time %.2f, the customer ID %2d, the clerk ID %1d. \n", startTime, currCustomer->id, clerkSignaled);

    usleep(currCustomer->serviceTime * DS_TO_MS); // Processing time

    double endTime = getTime();
    printf("A clerk finishes serving a customer: End time %.2f, the customer ID %2d, the clerk ID %1d. \n", endTime, currCustomer->id, clerkSignaled);

    switch (clerkSignaled) // Lets the clerk know their customer is processed
    {
    case 0:
        if (pthread_cond_signal(&clerk0Finished) != 0)
        {
            printf("[ Error: failed to signal clerk 0 finished ]\n");
        }
        break;
    case 1:
        if (pthread_cond_signal(&clerk1Finished) != 0)
        {
            printf("[ Error: failed to signal clerk 1 finished ]\n");
        }
        break;
    case 2:
        if (pthread_cond_signal(&clerk2Finished) != 0)
        {
            printf("[ Error: failed to signal clerk 2 finished ]\n");
        }
        break;
    case 3:
        if (pthread_cond_signal(&clerk3Finished) != 0)
        {
            printf("[ Error: failed to signal clerk 3 finished ]\n");
        }
        break;
    }

    if (pthread_mutex_lock(&waitingTimeMutex) != 0)
    {
        printf("[ Error: failed to lock mutex ]\n");
    }
    waitEntry newEntry = {currCustomer->class, overallWaitingTime};
    completeWaitingTime[waitEntryIndex] = newEntry; // Inserts waiting time of customer
    waitEntryIndex++;
    if (pthread_mutex_unlock(&waitingTimeMutex) != 0)
    {
        printf("[ Error: failed to unlock mutex ]\n");
    }

    pthread_exit(NULL);
}

/* *************** { Clerk Thread  } *************** */

void *clerkEntry(void *clerkInfo)
{
    clerk *currClerk = (clerk *)clerkInfo;

    printf("A clerk is working: Clerk ID %d. \n", currClerk->id);

    while (TRUE)
    {
        /* The clerk will continue working until the number of wait entries equals total customers served */
        if (businessQueueLength == 0 && economyQueueLength == 0)
        {
            if (currClerk->status == 0 && waitEntryIndex == numOfCustomers)
            {
                break;
            }
        }

        if (currClerk->status == 0) // If clerk is idle...
        {
            if (pthread_mutex_lock(&selectedQueueMutex) != 0)
            {
                printf("[ Error: failed to lock mutex ]\n");
            }
            if (businessQueueLength > 0) // First check if business queue has customers
            {
                selectedQueue = 1; // For indicating which queue has been selected for processing
                if (pthread_mutex_lock(&clerkStatusMutex) != 0)
                {
                    printf("[ Error: failed to lock mutex ]\n");
                }
                currClerk->signaled = selectedQueue;
                if (pthread_mutex_unlock(&clerkStatusMutex) != 0)
                {
                    printf("[ Error: failed to unlock mutex ]\n");
                }

                if (pthread_mutex_lock(&businessQueue) != 0)
                {
                    printf("[ Error: failed to lock mutex ]\n");
                }
                pthread_cond_broadcast(&clerkAvailable);
                if (pthread_mutex_unlock(&businessQueue) != 0)
                {
                    printf("[ Error: failed to unlock mutex ]\n");
                }
            }
            else
            {
                selectedQueue = 0; // For indicating which queue has been selected for processing
                if (pthread_mutex_lock(&clerkStatusMutex) != 0)
                {
                    printf("[ Error: failed to lock mutex ]\n");
                }
                currClerk->signaled = selectedQueue;
                if (pthread_mutex_unlock(&clerkStatusMutex) != 0)
                {
                    printf("[ Error: failed to unlock mutex ]\n");
                }

                if (pthread_mutex_lock(&economyQueue) != 0)
                {
                    printf("[ Error: failed to lock mutex ]\n");
                }
                pthread_cond_broadcast(&clerkAvailable);
                if (pthread_mutex_unlock(&economyQueue) != 0)
                {
                    printf("[ Error: failed to unlock mutex ]\n");
                }
            }
            if (pthread_mutex_unlock(&selectedQueueMutex) != 0)
            {
                printf("[ Error: failed to unlock mutex ]\n");
            }
        }
        else // If clerk is busy...
        {
            if (pthread_mutex_lock(&clerkStatusMutex) != 0)
            {
                printf("[ Error: failed to lock mutex ]\n");
            }
            switch (currClerk->id) // Clerk waits till customer processing is done
            {
            case 0:
                if (pthread_cond_wait(&clerk0Finished, &clerkStatusMutex) != 0)
                {
                    printf("[ Error: clerk 0 finished condition variable failed ]\n");
                }
                break;
            case 1:
                if (pthread_cond_wait(&clerk1Finished, &clerkStatusMutex) != 0)
                {
                    printf("[ Error: clerk 1 finished condition variable failed ]\n");
                }
                break;
            case 2:
                if (pthread_cond_wait(&clerk2Finished, &clerkStatusMutex) != 0)
                {
                    printf("[ Error: clerk 2 finished condition variable failed ]\n");
                }
                break;
            case 3:
                if (pthread_cond_wait(&clerk3Finished, &clerkStatusMutex) != 0)
                {
                    printf("[ Error: clerk 3 finished condition variable failed ]\n");
                }
                break;
            }
            currClerk->status = 0; // Change clerk status back to idle
            if (pthread_mutex_unlock(&clerkStatusMutex) != 0)
            {
                printf("[ Error: failed to unlock mutex ]\n");
            }
        }
    }

    pthread_exit(NULL);

    return NULL;
}

/* *************** { Calculates and prints overall waiting times  } *************** */

void printWaitingTimes()
{
    float economy = 0.0;
    float economyEntries = 0;
    float business = 0.0;
    float businessEntries = 0;
    float total = 0.0;

    for (int i = 0; i < waitEntryIndex; i++)
    {
        if (completeWaitingTime[i].class == 0)
        {
            economy += completeWaitingTime[i].waitTime;
            economyEntries++;
        }
        else
        {
            business += completeWaitingTime[i].waitTime;
            businessEntries++;
        }
        total += completeWaitingTime[i].waitTime;
    }

    economy = economy / economyEntries;
    business = business / businessEntries;
    total = total / waitEntryIndex;

    printf("The average waiting time for all %d customers in the system is: %.2f seconds. \n", waitEntryIndex, total);
    printf("The average waiting time for all %d business-class customers is: %.2f seconds. \n", (int)businessEntries, business);
    printf("The average waiting time for all %d economy-class customers is: %.2f seconds. \n", (int)economyEntries, economy);
}

/* *************** { Starts simulation and creates/joins/destroys threads  } *************** */

int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        printf("[ Error: Execute program as follows: ./ACS <input.txt> ]\n");
        exit(1);
    }

    FILE *fp;
    char numberOfCustomers[20];
    fp = fopen(argv[1], "r");
    if (fp != NULL)
    {
        fgets(numberOfCustomers, 20, fp);
        fclose(fp);
    }
    else
    {
        printf("[ Error: Customer input file was unable to be processed ]\n");
        exit(1);
    }

    numOfCustomers = atoi(numberOfCustomers); // Total number of customers for simulating
    char fileContents[numOfCustomers][MAX_INPUT];
    customer customerList[numOfCustomers];
    fileReader(argv[1], fileContents, customerList); // Parses input info to customer array

    /* Mutex lock declarations */
    if (pthread_mutex_init(&waitingTimeMutex, NULL) != 0)
    {
        printf("[ Error: failed to initialize mutex ]\n");
        exit(1);
    }
    if (pthread_mutex_init(&businessQueue, NULL) != 0)
    {
        printf("[ Error: failed to initialize mutex ]\n");
        exit(1);
    }
    if (pthread_mutex_init(&economyQueue, NULL) != 0)
    {
        printf("[ Error: failed to initialize mutex ]\n");
        exit(1);
    }
    if (pthread_mutex_init(&selectedQueueMutex, NULL) != 0)
    {
        printf("[ Error: failed to initialize mutex ]\n");
        exit(1);
    }
    if (pthread_mutex_init(&clerkStatusMutex, NULL) != 0)
    {
        printf("[ Error: failed to initialize mutex ]\n");
        exit(1);
    }

    /* Thread condition variable declarations */
    if (pthread_cond_init(&clerkAvailable, NULL) != 0)
    {
        printf("[ Error: failed to initialize condition variable ]\n");
        exit(1);
    }
    if (pthread_cond_init(&clerk0Finished, NULL) != 0)
    {
        printf("[ Error: failed to initialize condition variable ]\n");
        exit(1);
    }
    if (pthread_cond_init(&clerk1Finished, NULL) != 0)
    {
        printf("[ Error: failed to initialize condition variable ]\n");
        exit(1);
    }
    if (pthread_cond_init(&clerk2Finished, NULL) != 0)
    {
        printf("[ Error: failed to initialize condition variable ]\n");
        exit(1);
    }
    if (pthread_cond_init(&clerk3Finished, NULL) != 0)
    {
        printf("[ Error: failed to initialize condition variable ]\n");
        exit(1);
    }

    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0)
    {
        printf("[ Error: failed to initialize attr ]\n");
        exit(1);
    }
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) != 0)
    {
        printf("[ Error: failed to set detached state ]\n");
        exit(1);
    }

    for (int i = 0; i < 4; i++) // Creates clerk input info array
    {
        clerk newClerk = {
            i, 0, 0};
        clerks[i] = newClerk;
    }

    gettimeofday(&initTime, NULL); // Recieves simulation start time

    //  Creating customer threads
    for (int i = 0; i < numOfCustomers; i++)
    {
        if (pthread_create(&customerThreads[i], &attr, customerEntry, (void *)&customerList[i]) != 0)
        {
            printf("[ Error: failed to create customer thread ]\n");
            exit(1);
        }
    }

    // Creating clerk threads
    for (int i = 0; i < 4; i++)
    {
        if (pthread_create(&clerkThreads[i], &attr, clerkEntry, (void *)&clerks[i]) != 0)
        {
            printf("[ Error: failed to create clerk thread ]\n");
            exit(1);
        }
    }

    // Wait for all threads to terminate
    for (int i = 0; i < numOfCustomers; i++)
    {
        if (pthread_join(customerThreads[i], NULL) != 0)
        {
            printf("[ Error: Failed to join all customer threads ]\n");
            exit(1);
        }
    }

    for (int i = 0; i < 4; i++)
    {
        if (pthread_join(clerkThreads[i], NULL) != 0)
        {
            printf("[ Error: Failed to join all clerk threads ]\n");
            exit(1);
        }
    }

    // Destroy mutex and conditional variables
    if (pthread_attr_destroy(&attr) != 0)
    {
        printf("[ Error: failed to destroy attr ]\n");
        exit(1);
    }

    if (pthread_mutex_destroy(&businessQueue) != 0)
    {
        printf("[ Error: failed to destroy mutex lock ]\n");
        exit(1);
    }
    if (pthread_mutex_destroy(&economyQueue) != 0)
    {
        printf("[ Error: failed to destroy mutex lock ]\n");
        exit(1);
    }
    if (pthread_mutex_destroy(&waitingTimeMutex) != 0)
    {
        printf("[ Error: failed to destroy mutex lock ]\n");
        exit(1);
    }
    if (pthread_mutex_destroy(&selectedQueueMutex) != 0)
    {
        printf("[ Error: failed to destroy mutex lock ]\n");
        exit(1);
    }
    if (pthread_mutex_destroy(&clerkStatusMutex) != 0)
    {
        printf("[ Error: failed to destroy mutex lock ]\n");
        exit(1);
    }

    if (pthread_cond_destroy(&clerkAvailable) != 0)
    {
        printf("[ Error: failed to destroy condition variable ]\n");
        exit(1);
    }
    if (pthread_cond_destroy(&clerk0Finished) != 0)
    {
        printf("[ Error: failed to destroy condition variable ]\n");
        exit(1);
    }
    if (pthread_cond_destroy(&clerk1Finished) != 0)
    {
        printf("[ Error: failed to destroy condition variable ]\n");
        exit(1);
    }
    if (pthread_cond_destroy(&clerk2Finished) != 0)
    {
        printf("[ Error: failed to destroy condition variable ]\n");
        exit(1);
    }
    if (pthread_cond_destroy(&clerk3Finished) != 0)
    {
        printf("[ Error: failed to destroy condition variable ]\n");
        exit(1);
    }

    printWaitingTimes(); // Prints the waiting times
    double simulationTime = getTimeDifference(initTime);
    printf("This is the total simulation time: %f seconds.\n", simulationTime);

    pthread_exit(NULL);
    return 0;
}