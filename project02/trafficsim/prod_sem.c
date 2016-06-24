#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define N 10

int buffer[N];
int counter=0;
int in=0;
int out=0;
int total=0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t semfull;
sem_t semempty;

void *producer(void *junk) {
    while(1) {
        sem_wait(&semempty);
        pthread_mutex_lock(&mutex);

        buffer[in] = total++;
        printf("Produced %d\n", buffer[in]);

        in = (in + 1) % N;  // Circular Queue
        counter++;

        pthread_mutex_lock(&mutex);
        sem_post(&semfull);
    }
}

void *consumer(void *junk) {
    while(1) {
        sem_wait(&semfull);
        pthread_mutex_lock(&mutex);

        printf("Consumed %d\n", buffer[out]);
        out = (out + 1) % N;
        counter--;

        pthread_mutex_unlock(&mutex);
        sem_post(&semempty);
    }
}

int main() {
    pthread_t thread;

    sem_init(&semfull, 0, 0);
    sem_init(&semempty, 0, N);

    pthread_create(&thread, NULL, producer, NULL);

    consumer(NULL);
}
