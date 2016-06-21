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
#include <linux/spinlock.h>
#include "simple_queue.c"

// DEFINITIONS
typedef int bool;
#define true 1
#define false 0

// GLOBAL VARS
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
} SharedMemoryQueue;  /* TODO:  Can open this in the memory mapped region.
                             Inside Producer/Consumer */

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
} Road;  /* TODO: Model this using processes instead.....
                  fork a process, send it into a while loop
                  like in the example from 449... where it loops on
                  making rand number, if < 8 add another car
                  else wait 20 seconds then add another car... */


// FUNCTION PROTOTYPES
void producer(Road *road_ptr);
void consumer(Road *road_ptr);

// SEMAPHORES
struct cs1550_sem north_sem_empty;
struct cs1550_sem north_sem_full;

struct cs1550_sem south_sem_empty;
struct cs1550_sem soutth_sem_full;

struct cs1550_sem sem_mutex;

/* ---------- SEMAPHORES -------------- */
// --> TODO:  Semaphore data type
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

/*
// SEMAPHORES
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
    void *mmmap_region_start = mmap(NULL, N, PROT_READ|PROT_WRITE,
            MAP_SHARED|MAP_ANONYMOUS, 0, 0);

    if (mmmap_region_start == MAP_FAILED) {
         perror("Memory mapping failed");
         exit(-1);
    }

    return mmmap_region_start;
}

/*
 * Function to determine if a car has arrived
 *
 */
bool carArrives() {
    bool arrivalDecision;

    int randNum = rand() % 10;
    if (randNum > 8)
        arrivalDecision = true;
    else
        arrivalDecision = false;

    return arrivalDecision;
}

void honkHornIfneeded(Flagperson *flagperson) {
   if (flagperson->isAsleep == true) {
      flagperson->isAsleep = false; 
   }
}
/* may need a separate producer function for EACH road... 
 * so they can each have a separate value....
 * but then they might need to each call a diferent consumer function...
 * which is okay, as long as everything shares one mutex...
 */
void north_road_producer(Road *road_ptr) {
    SharedMemoryQueue northRoad = road_ptr->northRoad;
    // also needs to be a while(1) loop, so that it runs forever
    // ANY TIME No Car arrives, wait 20 seconds, THEN a new car MUST come.
    // Applies to both directions... each separately.
    down(&sem_empty);  // consume and empty space, when we fill it
    down(&sem_mutex);  // consume the mutex
    while (true) {
        // start producing! as per the logic in the assignment prompt
        // check if a car is arriving on on the northroad
        northRoad.carArrived = carArrives(); 
        if (northRoad.carArrived == false) {
            // wait 20 seconds
        } else {
            // add car to the buffer, check if car arrived again
        }
       
    }
    up(&sem_mutex); // release the mutex when we're done
    up(&sem_full);  // produce a space in the buffer that can later be consumed
}

void south_road_producer(Road *road_ptr) {
    // also needs to be a while(1) loop, so that it runs forever
    // ANY TIME No Car arrives, wait 20 seconds, THEN a new car MUST come.
    // Applies to both directions... each separately.
    down(&sem_empty);  // consume and empty space, when we fill it
    down(&sem_mutex);  // consume the mutex
    while (true) {
        // start producing! as per the logic in the assignment prompt
       
    }
    up(&sem_mutex); // release the mutex when we're done
    up(&sem_full);  // produce a space in the buffer that can later be consumed
}

void consumer(Road *road_ptr) {
    flagperson = road_ptr->flagperson;
    /* Ensure we can access ands et set values correctly on our Road Model, via the ptr passed in */
    // ------------ DEBUG --------------
    // road_ptr->flagperson.roadInUse = true;
    // printf("Memory mapped region start = %p\n", road_ptr);
    // printf("Flagperson_ptr->roadInUse = %d\n", road_ptr->flagperson.roadInUse);
    // ----------- END DEBUG ------------
    // this needs to a be a while(1) loop, so that it runs forever...
    down(&sem_full);  // consumes a full space in the buffer
    down(&sem_mutex); // consumes the mutex
    while(true) {
        // start consuming! as per the logic defined in the assignment prompt
              
        // check if we need to sleep
        if (flagperson->lineLengthNorth == 0 && flagperson->lineLengthSouth == 0) {
            // fall asleep...
           flagperson->isAsleep = true; 
        }
 
 
        // these will strictly alternate if both are >10... so should be okay.
        if (flagperson->lineLengthNorth > 0 && flagperson->isAsleep == false) {
           // let a car pass from north to south, ensure it takes 2 seconds,
           // and no other cars can use the road during this time
           // let cars pass WHILE lineLengthSouth < 10 && lineLengthNorth > 0
           // WHILE ...
           //     call honkHorn
        }

        if (flagperson->lineLengthSouth > 0 && flagperson0>isAsleep == false) {
            
            // let cars pass from south, and ensure ti takes 2 seconds,
            // WHILE lineLengthNorth < 10 && lineLengthSouth > 0
            // WHILE ... 
            //    call honkHorn
        }
    }
    up(&sem_mutex); // releases the mutex
    up(&sem_empty); // produces an empty space, by consuming an item

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
        printf("I am the child with pid %d\n", (int) getpid());
        producer(myRoad);
        exit(0); // ensure child only does work inside this if statement
    }
    // ------ END NORTH ROAD -----------


    /* FORK PROCESS FOR PRODUCER (SOUTH ROAD) */
    // --------- SOUTH ROAD --------------
    printf("I am: %d\n", (int) getpid());
    pid_t pid_southRoad = fork();
    printf("fork returned %d\n", (int)pid_southRoad);

    if (pid_southRoad < 0) perror("fork failed");

    if (pid_southRoad == 0) {
        printf("I am the child with pid %d\n", (int) getpid());
        producer(myRoad);
        exit(0); // ensure child only does work inside this if statement
    }
    // --------- END SOUTH ROAD -----------

    printf("I make it to the end of the code block %d\n", (int)getpid());
}


int main() {
    // TODO:  Store these in shared memory
    sem_empty.value = BUFFER_MAX;
    sem_full.value = 0;
    sem_mutex.value = 1;
    // main method for the whole project
    //
    // Basic outline...
    // create the memory mapping we need
    int northRoadSize  = sizeof(SharedMemoryQueue);
    int southRoadSize  = sizeof(SharedMemoryQueue);
    int flagpersonSize = sizeof(Flagperson);
    // TODO:  Do I want to store the sempahores in shared memory, too? int semaphoreSize; 
    int totalSharedMemoryNeeded = northRoadSize + southRoadSize + flagpersonSize;


    void *mmap_region_start_ptr = mapSharedMemory(totalSharedMemoryNeeded);
    // create a road struct
    Road *myRoad = malloc(sizeof(Road));
    printf("Road size is: %d\n", (int)sizeof(Road));
    printf("Memory size is: %d\n", totalSharedMemoryNeeded);

    // TODO:  Initalize our smaphores....
    //sem_init(&semfull, 0, 0);
    //sem_init(&semempty, 0, N);

    // start the simulation
    runSimulation(mmap_region_start_ptr);

}
