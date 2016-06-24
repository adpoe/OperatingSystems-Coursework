#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

int i=0;
void doSomeWork(char *name) {
    const int NUM_TIMES = 5;
    for ( ; i < NUM_TIMES; i++) {
        sleep(rand() % 4);
        printf("Done pass %d for %s\n", i, name);
    }
}

int main(int argc, char *argv[]) {
    printf("I am: %d\n", (int) getpid());

    // can isolate the forking code
    pid_t pid = fork();
    srand((int) pid);
    printf("fork returned %d\n", (int) pid);

    if (pid < 0) perror("fork failed");

    if (pid == 0) {
       printf("I am the child with pid %d\n", (int) getpid());
       doSomeWork("Child");
       exit(42);    // so the child process only does operations inside this if statement
                    // and what happens in this if statement is being executed by a SEPARATE PROCESS
    }

    // we must be the parent
    printf("I am the parent waiting for child to end\n");
    doSomeWork("Parent");
    int status;
    pid_t childpid = wait(&status); // wait for child to finish before continuing
    printf("Parent ending, knows child %d finsihed.\n", (int) childpid);
    int childReturnValue = WEXITSTATUS(status);
    printf("    Return value from child was: %d\n", childReturnValue);

}
