#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <pthread.h> 

#define PLAYERS_PER_TEAM 5

//Globals cos reasons
pthread_mutex_t lock; 



struct semaphore{
    int* avaible;
    int* resource;
};

struct semaphore* new_semaphore(int* pResource) {
    struct semaphore* sem = malloc(sizeof(struct semaphore));
    sem->avaible = (int*) malloc(2);
    *(sem-> avaible) = 1;
    sem -> resource = pResource;
    return sem;
}




int adquire(struct semaphore* sem){
    if (*(sem->avaible) == 0) return 0;
    
    *(sem->avaible) = 0;
    return 1;
}

void release(struct semaphore* sem){
    *(sem->avaible) = 1;
    return;
}


int main(){
    int* goalA = malloc( sizeof(int) );
    int* goalB = malloc( sizeof(int) );

    int* ball = malloc( sizeof(int) );

    int* p = (int*) malloc(2); //Pruebas
    *p = 0;

    struct semaphore* semP = new_semaphore(p);

    pid_t pid;
    int parentID = getpid();
    printf("Arbiter ID: %d.!\n", parentID);

    for (int generated = 0; generated < PLAYERS_PER_TEAM; generated++){
        if (getpid() == parentID) pid = fork();
    }

    //Hijos por otro camino

    for (int generated = 0; generated < PLAYERS_PER_TEAM; generated++){
        if (getpid() == parentID) pid = fork();
    }

    //

    if (pid < 0){
        printf("Welp I failed\n");
        return 1;
    } else if (pid > 0){
        printf("[Arbiter]: Start! \n");
        //Waits for 5min before killing everyone
        wait(NULL);
        printf("ball %d\n", *p); //No one is using it so :p
    } else {

        if (adquire(semP) == 0){
            printf("[Player]: Almost\n");
        } else {
            printf("[Player]: It's mine\n");
            sleep(2);
            printf("[Player]: There, have it\n");
            release(semP);
        }
    
        //printf("[Player]: I'm a player process\n");
        sleep(2);  
    }

    return 0;

}