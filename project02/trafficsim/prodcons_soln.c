typedef int semaphore
semaphore mutex = 1;
semaphore empty = 100;
semaphore full = 0;

void producer(void)
{
   int item;

   while(TRUE) {
      item = produce_item(); //generate an item to put in the buffer
      down(&empty);          //decrement empty
      down(&mutex);          //enter critical section
      insert_item(item);     //insert item into shared buffer
      up(&mutex);            //exit critical section
      up(&full);             //increment full
   }
}

void consumer(void)
{
   int item;

   while(TRUE) {
      down(&full);           //decrement full
      down(&mutex);          //enter critical section
      item = remove_item();  //remove item from shared buffer
      up(&mutex);            //exit critical section
      up(&empty);            //increment empty
      consume_item(item);    //do something with the item
   }
}
