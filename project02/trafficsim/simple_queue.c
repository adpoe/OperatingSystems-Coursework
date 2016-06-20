/*
 * A Simple LinkedList Queue, for use in the
 * the sempaphore data structure.
 *
 */
#include <stdio.h>
#include <stdlib.h>


/*
 * Our node struct, which forms the basis for the
 * Queue data structure
 */
typedef struct Node {
  int item;
  struct Node* next;
} Node;

/*
 * The Queue data structure itself.
 */
typedef struct Queue {
  // fields
  Node *head;
  Node *tail;
  int size;

  // functions
  void (*push) (struct Queue*, int);
  int (*pop) (struct Queue*);
  int (*peek) (struct Queue*);
  void (*display) (struct Queue*);
} Queue;

// Function Declarations
void  push    (Queue* queue, int item);
int   pop     (Queue* queue);
int   peek    (Queue* queue);
void  display (Queue* queue);
Queue createQueue ();

/*
 * Push onto the queue
 */
void push(Queue* queue, int item) {
  // Make a new node and assign values to its fields
  Node *n = (Node*) malloc(sizeof(Node));
  n->item = item;
  n->next = NULL;

  // handle un-initialized queue
  if (queue->head == NULL) {
    queue->head = n;
  // otherwise, add it to the tail
  } else {
    queue->tail->next = n;
  }

  queue->tail = n;
  queue->size++;
}

/*
 * Pop first item off the queue
 */
int pop (Queue* queue) {
  Node *head = queue->head;
  int item = head->item;

  queue->head = head->next;
  queue->size--;

  free(head);
  return item;
}

/*
 * Peek at item in top of the queue
 */
int peek (Queue* queue) {
  Node *head = queue->head;
  return head->item;
}

/*
 * Display all queue items
 */
 void display (Queue* queue) {
     printf("\nDisplay: ");
     // no item
     if (queue->size == 0)
         printf("No item in queue.\n");
     else { // has item(s)
         Node* head = queue->head;
         int i, size = queue->size;
         printf("%d item(s):\n", queue->size);
         for (i = 0; i < size; i++) {
             if (i > 0)
                 printf(", ");
             printf("%d", head->item);
             head = head->next;
         }
     }
     printf("\n\n");
 }

/**
* Create and initiate a Queue
*/
Queue createQueue () {
     Queue queue;
     queue.size = 0;
     queue.head = NULL;
     queue.tail = NULL;
     queue.push = &push;
     queue.pop = &pop;
     queue.peek = &peek;
     queue.display = &display;
     return queue;
 }

/*
int main() {
Queue queue = createQueue();
queue.display(&queue);

printf("push item 2\n");
queue.push(&queue, 2);
printf("push item 3\n");
queue.push(&queue, 3);
printf("push item 6\n");
queue.push(&queue, 6);

queue.display(&queue);

printf("peek item %d\n", queue.peek(&queue));
queue.display(&queue);

printf("pop item %d\n", queue.pop(&queue));
printf("pop item %d\n", queue.pop(&queue));
queue.display(&queue);

printf("pop item %d\n", queue.pop(&queue));
queue.display(&queue);
printf("push item 6\n");
queue.push(&queue, 6);

queue.display(&queue);
system("PAUSE");

}*/
