# ifndef _QUEUE_H_
# define _QUEUE_H_

typedef struct Queue
{
        int capacity;
        int size;
        int front;
        int rear;
        int *elements;
}Queue;

Queue * createQueue(int maxElements);

void Dequeue(Queue *Q);

int front(Queue *Q);

void Enqueue(Queue *Q,int element);

# endif
