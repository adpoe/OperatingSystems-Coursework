#include <stdlib.h>
#include <stdio.h>
#include <sys/sem.h>

#define N  10
#define K  1
// Semaphore declaration
semaphore s = K;    // K resources
                    // each process allocates a resource, uses it for awhile, returns it to pool
                    // signal says --> hey, i'm done with this resource!
                    // Think:  we start with 1 in Prod/Cons, until we make more
                    // MIT prof, says we start with 0... until we produce once

/* operations for semaphore
 * - wait(sempahore s) -- stall current process if (s <= 0), otherwise s=s-1
 * - signal(semaphore s) -- s = s +1 (can have side effect of letting other processes proceed)
 *   think of semaphore as an integer value
 *   if semaphore is > 0, decrement it and return
 *   if it is < 0, then WAIT until it IS >0... THEN, decrement it and return
 */


/* Semaphore implementation - wait and signal must be atomic
 *   - can use atomicity of kernel handlers
 *   - can use special instruction 'test and set', using atomicity of single instruction execution
 *
 *   Bootstrapping:
 *   - A simple lock ("binary semaphore") allows easy implementation of full semaphore support
 */

// SHARED MEMORY
char buf[N];    /* the buffer */
int in=0, out=0;
semaphore chars=0, space=N;
sempahore mutex=1;

int main()
{
    // main code here
}


// PRODUCER
void send(char c)
{
    wait(space);  // producer 'consumes' spaces
    wait(mutx);
    buf[in] = c;
    in = (in+1) % N;
    signal(mutex);
    signal(chars);
}


// CONSUMER
char recv()
{
    char c;
    wait(chars);
    wait(mutex);
    c = buf[out];
    out = (out+1) % N;
    signal(mutex);
    signal(space);  // consumer 'produces' spaces back into the buffer
    return c;
}


// Semaphore as supervisor call

wait_h()
{
     int *addr;
     addr = User.Regs[R0];  /* get arg */
     if(*addr <= 0) {
         User.Regs[XP] = User.Regs[XP]-4;
         sleep(addr);
     } else
         *addr = *addr-1;
}


signal_h()
{
    int *addr;
    addr = User.Regs[R0]; /* get arg */
    *addr = *addr + 1;
    wakeup(addr);
}

/* Calling sequence
 * ...
 * || put address of lock
 * || into R0
 * CMOVE(lock, R0);
 * SVC(WAIT)
 * SVC call is not interruptible, since it is executed in
 * supervisory mode
 */
