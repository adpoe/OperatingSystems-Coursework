/* @author Anthony Poerio (adp59@pitt.edu)
 * University of Pittsburgh
 * CS1550 - Operating Systems
 * Project #2 - Trafficsim
 * Due:  July 3, 2016
 *
 * Main traffic simulation code
 */

// INCLUDES
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
// #include <linux/spinlock.h> TODO: Figure out how to actually build the kernel source and make a spinlock...
#include "simple_queue.c"

// DEFINITIONS
typedef int bool;
#define true 1
#define false 0

// GLOBAL VARS
int flagpersonPID;
int northRoadPID;
int southRoadPID;

int delayTime=20; /* seconds */
int travelTime=2; /* seconds */

#define BUFFER_MAX 10

typedef struct SharedMemoryQueue {
    bool carArrived;
    int readIndex;
    int writeIndex;
    int buffer[BUFFER_MAX];
    int used_spaces;
    int counter;
    int total;
} SharedMemoryQueue;


typedef struct Flagperson {
   // Consumer *flagperson;
    bool roadInUse;
    bool isAsleep;
    int lineLengthNorth;
    int lineLengthSouth;
} Flagperson;

typedef struct Road {
    SharedMemoryQueue northRoad;
    SharedMemoryQueue southRoad;
    Flagperson flagperson;

    // Space for semaphores
    // POSIX SEMAPHORES     TODO:  Use these to ensure that TrafficSim works, first and foremost
    sem_t north_sem_empty;
    sem_t north_sem_full;

    sem_t south_sem_empty;
    sem_t south_sem_full;

    sem_t sem_mutex;
} Road;


// FUNCTION PROTOTYPES
void producer(Road *road_ptr);
void consumer(Road *road_ptr);

/* ---------- SEMAPHORES -------------- */
/*
// --> TODO:  Implement my own custom semaphores.
// SEMAPHORES
struct cs1550_sem north_sem_empty;
struct cs1550_sem north_sem_full;

struct cs1550_sem south_sem_empty;
struct cs1550_sem south_sem_full;

struct cs1550_sem sem_mutex;

struct cs1550_sem {
    int value; // how many items we have access to
    Queue *process_list;
} cs1550_sem;

void down(struct cs1550_sem *sem) {
    DEFINE_SPINLOCK(sem_lock);
    spin_unlock(&sem_lock);
    sem->value -= 1;
    if (sem->value < 0) {
        push(sem->process_list, getpid());
        set_current_state(TASK_INTERRUPTIBLE);
        schedule();
    }
    spin_unlock(&sem_unlock);
}

void up(struct cs1550_sem *sem) {
    DEFINE_SPINLOCK(sem_lock);
    spin_lock(&sem_lock);
    int mypid;
    sem->value += 1;
    if (sem->value <= 0) {
         mypid = peek(sem->process_list);
         pop(sem->process_list);
         wake_up_process(getpid());
    }
    spin_unlock(&sem_lock);
}

// SEMAPHORES declarations, the values they should have
struct cs1550_sem sem_empty;
sem_empty.value = BUFFER_MAX;

struct cs1550_sem sem_full;
sem_full.value = 0;

struct cs1550_sem sem_mutex;
sem_mutex.value = 1;
*/
/* ---------- END SEMAPHORES ---------- */


/*
 * Create the memory mapped region where our queues will live.
 * Must be accessible to both a producer process and a separate consumer process
 *
 * params: @N = the number of bytes of RAM to allocate for shared memory mapping
 * return: a void to the beginning of the memory mapped region
 */
void *mapSharedMemory(int N) {
    // code to do the memory mapping goes here
    void *mmap_region_start = mmap(NULL, N, PROT_READ|PROT_WRITE,
            MAP_SHARED|MAP_ANONYMOUS, 0, 0);

    if (mmap_region_start == MAP_FAILED) {
         perror("Memory mapping failed");
         exit(-1);
    }

    /*------------ INITIALIZE OUR SEMAPHORES ------------ */
    Road *myRoad = mmap_region_start; // The region is a road struct.
    // TODO:  Initalize our smaphores....
    // Notes at:  http://www.csc.villanova.edu/~mdamian/threads/posixsem.html
    // http://man7.org/linux/man-pages/man3/sem_init.3.html
    //     Re: Semaphores -> If semaphore is shared between processes, should be
    //     in a region of shared memory, and set 2nd flag to nonzero
    sem_init(&myRoad->north_sem_empty, 1, BUFFER_MAX); // empty semaphore starts with size of buffer
    sem_init(&myRoad->north_sem_full, 1, 0); // when we start, there are no full spaces

    sem_init(&myRoad->south_sem_empty, 1, BUFFER_MAX);
    sem_init(&myRoad->south_sem_full, 1, 0);

    sem_init(&myRoad->sem_mutex, 1, 1); // mutexes start with 1 since we have 1 critical region
    /*---------------- END INITIALIZATION ---------------- */


    return mmap_region_start;
}

/*
 * Function to determine if a car has arrived
 *
 */
bool carArrives() {
    bool arrivalDecision;

    int randNum = rand() % 10;
    if (randNum < 8)
        arrivalDecision = true;
    else
        arrivalDecision = false;

    int arrivalPID = getpid();
    if (arrivalPID == northRoadPID)
        printf("Car arrives called by %d (NORTH ROAD) and returned: %d\n\n", getpid(), arrivalDecision);
    else
        printf("Car arrives called by %d(SOUTH ROAD) and returned: %d\n\n", getpid(), arrivalDecision);

    return arrivalDecision;
}

void honkHornIfneeded(Flagperson *flagperson) {
   if (flagperson->isAsleep == true) {
      printf("HONK CALLED\n\n");
      flagperson->isAsleep = false;
   }
}
/* may need a separate producer function for EACH road...
 * so they can each have a separate value....
 * but then they might need to each call a diferent consumer function...
 * which is okay, as long as everything shares one mutex...
 */
void north_road_producer(Road *road_ptr) {
    SharedMemoryQueue *northRoad = &road_ptr->northRoad;
    // also needs to be a while(1) loop, so that it runs forever
    // ANY TIME No Car arrives, wait 20 seconds, THEN a new car MUST come.
    // Applies to both directions... each separately.

    // TODO:   Change this so we can use MY own DOWN() and UP() semaphores
    //down(&sem_empty);  // consume and empty space, when we fill it
    //down(&sem_mutex);  // consume the mutex

        while (true) {
        // start producing! as per the logic in the assignment prompt
        // check if a car is arriving on on the northroad
        northRoad->carArrived = carArrives();
        if (northRoad->carArrived == false) {
            // wait 20 seconds, without the mutex
            sleep(20);

        } else { /* else, we enter the critical section and start writing */
            // Use POSIX Semaphores to start --> DOWN()
            sem_wait(&road_ptr->north_sem_empty);
            sem_wait(&road_ptr->sem_mutex);

            // [ CRITICAL SECTION ]
            // add car to the buffer, check if car arrived again
            northRoad->buffer[northRoad->writeIndex] = northRoad->total++;
            printf("NORTH ROAD PRODUCER:   Produced NorthRoad:%d\n\n", northRoad->buffer[northRoad->writeIndex]);

            northRoad->writeIndex = (northRoad->writeIndex + 1) % BUFFER_MAX;
            northRoad->counter++;
            // [ END CRITICAL SECTION ]

            // POSIX Semaphores --> UP()
            sem_post(&road_ptr->sem_mutex);
            sem_post(&road_ptr->north_sem_full);
        }

    }

    // TODO: USE MY SEMAPHORES --> UP()
    //up(&sem_mutex); // release the mutex when we're done
    //up(&sem_full);  // produce a space in the buffer that can later be consumed
}

void south_road_producer(Road *road_ptr) {
    // also needs to be a while(1) loop, so that it runs forever
    // ANY TIME No Car arrives, wait 20 seconds, THEN a new car MUST come.
    // Applies to both directions... each separately.
    // TODO:  USE MY SEMAPHORES --> DOWN()
    //down(&sem_empty);  // consume and empty space, when we fill it
    ///down(&sem_mutex);  // consume the mutex

    SharedMemoryQueue *southRoad = &road_ptr->southRoad;

    while (true) {
        /// start producing! as per the logic in the assignment prompt
        // check if a car is arriving on on the southroad
        southRoad->carArrived = carArrives();
        if (southRoad->carArrived == false) {
            // wait 20 seconds, without the mutex
            sleep(20);

        } else { /* else, we enter the critical section and start writing */
            // Use POSIX Semaphores to start --> DOWN()
            sem_wait(&road_ptr->south_sem_empty);
            sem_wait(&road_ptr->sem_mutex);

            // [ CRITICAL SECTION ]
            // add car to the buffer, check if car arrived again
            southRoad->buffer[southRoad->writeIndex] = southRoad->total++;
            printf("SOUTH ROAD PRODUCER:   Produced SouthRoad:%d\n\n", southRoad->buffer[southRoad->writeIndex]);

            southRoad->writeIndex = (southRoad->writeIndex + 1) % BUFFER_MAX;
            southRoad->counter++;
            // [ END CRITICAL SECTION ]

            // POSIX Semaphores --> UP()
            sem_post(&road_ptr->sem_mutex);
            sem_post(&road_ptr->south_sem_full);
        } // end-if

    } // end-while


    // TODO:  USE MY UP()
    //up(&sem_mutex); // release the mutex when we're done
    //up(&sem_full);  // produce a space in the buffer that can later be consumed
}

void consumer(Road *road_ptr) {
    Flagperson *flagperson = &road_ptr->flagperson;
    SharedMemoryQueue *northRoad = &road_ptr->northRoad;
    SharedMemoryQueue *southRoad = &road_ptr->southRoad;

    /* Ensure we can access ands et set values correctly on our Road Model, via the ptr passed in */
    // ------------ DEBUG --------------
    // road_ptr->flagperson.roadInUse = true;
    // printf("Memory mapped region start = %p\n", road_ptr);
    // printf("Flagperson_ptr->roadInUse = %d\n", road_ptr->flagperson.roadInUse);
    // ----------- END DEBUG ------------
    // this needs to a be a while(1) loop, so that it runs forever...
    // TODO:   USE MY SEMAHORES
    //down(&sem_full);  // consumes a full space in the buffer
    //down(&sem_mutex); // consumes the mutex

    while(true) {
        // start consuming! as per the logic defined in the assignment prompt

        // check if we need to sleep
        if (flagperson->isAsleep == false && northRoad->counter == 0 && southRoad->counter == 0) {
            // fall asleep...
            sem_wait(&road_ptr->sem_mutex);
            printf("There are no cars in either line. Flagperson has fallen asleep....\n\n");
            flagperson->isAsleep = true;
            sem_post(&road_ptr->sem_mutex);
        }


        // these will strictly alternate if both are >10... so should be okay.
        // CONSUME NORTH
        if (northRoad->counter > 0 /*&& flagperson->isAsleep == false*/) {
               // POSIX STYLE DOWN()
            sem_wait(&road_ptr->north_sem_full);
            sem_wait(&road_ptr->sem_mutex);

            // [ CRITICAL SECTION ]
            // let a car pass from north to south, ensure it takes 2 seconds,
            // and no other cars can use the road during this time
            do {
            honkHornIfneeded(flagperson);
            printf("\tCONSUMED:   NorthRoad #%d\n\n", northRoad->buffer[northRoad->readIndex]);
            northRoad->readIndex = (northRoad->readIndex + 1) % BUFFER_MAX;
            northRoad->counter--; // this is the line length....
            sleep(2);

            // let cars pass WHILE lineLengthSouth < 10 && lineLengthNorth > 0
            } while(southRoad->counter < 10 && northRoad->counter > 0);

            // [ END CRITICAL SECTION ]

            // POSIX STYLE UP()
            sem_post(&road_ptr->sem_mutex);
            sem_post(&road_ptr->north_sem_empty);
        }

        // CONSUME SOUTH
        if (southRoad->counter > 0 /*&& flagperson->isAsleep == false*/) {
               // POSIX STYLE DOWN()
            sem_wait(&road_ptr->south_sem_full);
            sem_wait(&road_ptr->sem_mutex);

            // [ CRITICAL SECTION ]
            // let a car pass from north to south, ensure it takes 2 seconds,
            // and no other cars can use the road during this time
            do {
            honkHornIfneeded(flagperson);
            printf("\tCONSUMED:   SouthRoad #%d\n\n", southRoad->buffer[southRoad->readIndex]);
            southRoad->readIndex = (southRoad->readIndex + 1) % BUFFER_MAX;
            southRoad->counter--; // this is the line length....
            sleep(2);

            // let cars pass WHILE lineLengthSouth < 10 && lineLengthNorth > 0
            } while(northRoad->counter < 10 && southRoad->counter > 0);

            // [ END CRITICAL SECTION ]

            // POSIX STYLE UP()
            sem_post(&road_ptr->sem_mutex);
            sem_post(&road_ptr->north_sem_empty);
        }

    }

}

void runSimulation(void *mmap_region_ptr) {
    // main simulation code goes here
    Road *myRoad;
    myRoad = mmap_region_ptr;


    /* FORK PROCESS FOR CONSUMER (FLAPGPERSON) */
    // -------- FLAGPERSON -------------
    printf("I am: %d\n", (int) getpid());
    pid_t pid_flagperson = fork();
    printf("fork returned %d\n", (int)pid_flagperson);

    if (pid_flagperson < 0) perror("fork failed");

    if (pid_flagperson == 0) {
        flagpersonPID = getpid();
        printf("I am the child with pid %d\n", (int) getpid());
        consumer(myRoad);
        exit(0); // ensure child only does work inside this if statement
    }
    // ------- END FLAGPERSON ---------


    /* FORK PROCESS FOR PRODUCER (NORTH ROAD) */
    // -------- NORTH ROAD --------------
    printf("I am: %d\n", (int) getpid());
    pid_t pid_northRoad = fork();
    printf("fork returned %d\n", (int)pid_northRoad);

    if (pid_northRoad < 0) perror("fork failed");

    if (pid_northRoad == 0) {
        northRoadPID = getpid();
        printf("I am the child with pid %d\n", (int) getpid());
        north_road_producer(myRoad);
        exit(0); // ensure child only does work inside this if statement
    }
    // ------ END NORTH ROAD -----------


    /* FORK PROCESS FOR PRODUCER (SOUTH ROAD) */
    // --------- SOUTH ROAD -------------
    printf("I am: %d\n", (int) getpid());
    pid_t pid_southRoad = fork();
    printf("fork returned %d\n", (int)pid_southRoad);

    if (pid_southRoad < 0) perror("fork failed");

    if (pid_southRoad == 0) {
        southRoadPID = getpid();
        printf("I am the child with pid %d\n", (int) getpid());
        south_road_producer(myRoad);
        exit(0); // ensure child only does work inside this if statement
    }
    // --------- END SOUTH ROAD -----------

    printf("I make it to the end of the code block %d\n", (int)getpid());
}


int main() {
    // main method for the whole project
    //
    // Basic outline...
    // create the memory mapping we need
    int northRoadSize  = sizeof(SharedMemoryQueue);
    int southRoadSize  = sizeof(SharedMemoryQueue);
    int flagpersonSize = sizeof(Flagperson);
    int sizeOfAllSemaphores = sizeof(sem_t)*5;
    int totalSharedMemoryNeeded = northRoadSize + southRoadSize + flagpersonSize + sizeOfAllSemaphores;


    void *mmap_region_start_ptr = mapSharedMemory(totalSharedMemoryNeeded);
    // create a road struct
    //Road *myRoad = malloc(sizeof(Road));
    //printf("Road size is: %d\n", (int)sizeof(Road));
    //printf("Memory size is: %d\n", totalSharedMemoryNeeded);

    Road *myRoad = mmap_region_start_ptr;

    /* INITIALIZE FIELDS */
    // NORTH ROAD
    myRoad->northRoad.carArrived = false;
    myRoad->northRoad.readIndex = 0;
    myRoad->northRoad.writeIndex = 0;
    myRoad->northRoad.counter = 0;
    myRoad->northRoad.total = 0;

    // SOUTH ROAD
    myRoad->southRoad.carArrived = false;
    myRoad->southRoad.readIndex = 0;
    myRoad->southRoad.writeIndex = 0;
    myRoad->southRoad.counter = 0;
    myRoad->southRoad.total = 0;

    // FLAGPERSON
    myRoad->flagperson.roadInUse = false;
    myRoad->flagperson.isAsleep = false;
    /* END INITIALIZATION */

    // start the simulation
    runSimulation(mmap_region_start_ptr);

}
