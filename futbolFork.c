#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#define PLAYERS_PER_TEAM 5

//Globals cos reasons
pthread_mutex_t lock;
volatile sig_atomic_t usr_interrupt = 0;
/*

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
*/

void synch_signal (int sig){
  usr_interrupt = 1;
}

//Creacion de una cola
struct Nodo{
  pid_t data;
  struct Nodo *siguiente;

};

struct Cola{
  struct Nodo *inicio;
  struct Nodo *final;
  unsigned int size;
};

void init_cola (struct Cola *cola){
  cola->inicio = NULL;
  cola->final = NULL;
  cola->size = 0;
}

pid_t pop(struct Cola *cola){
  cola->size--;

  pid_t data = cola->inicio->data;
  struct Nodo *tmp = cola->inicio;
  cola->inicio = cola->inicio->siguiente;

  free(tmp);
  return data;
}

void push(struct Cola *cola, pid_t data){
  cola->size++;

  if (cola->inicio == NULL) {
    cola->inicio = (struct Nodo *) malloc(sizeof(struct Nodo));
    cola->inicio->data = data;
    cola->inicio->siguiente = NULL;
    cola->final = cola->inicio;
  }else{
    cola->final->siguiente = (struct Nodo *) malloc(sizeof(struct Nodo));
    cola->final->siguiente->data = data;
    cola->final->siguiente->siguiente = NULL;
    cola->final = cola->final->siguiente;
  }
}


struct Semaphore{
  int value;
  struct Cola *cola;
};

void init_semaphore (struct Semaphore *sem){
  init_cola(sem->cola);
  sem->value = 1;
}

//El proceso que desea el recurso lo solicita
void wait_semaphore(struct Semaphore* sem){
  sem -> value--;
  if(sem -> value < 0){
    //Se debe agregar a la lista de procesos que esperan el recurso
    push(sem->cola, getpid());
    //Se suspende el proceso para que espere por el recurso
    sigset_t mask, oldmask;
    /* Se establece la mask de las senales que se van a bloquear temporalmente */
    sigemptyset (&mask);
    sigaddset (&mask, SIGUSR1);
    /* Se espera por la senal SIGUSR1 */
    sigprocmask (SIG_BLOCK, &mask, &oldmask);
    //while (!usr_interrupt)
      //sigsuspend (&oldmask);
    sigsuspend (&oldmask);
    sigprocmask (SIG_UNBLOCK, &mask, NULL);

  }
}

//El proceso que ya uso el recurso lo notifica
void signal_semaphore (struct Semaphore* sem){
  sem -> value++;
  if(sem -> value <= 0){
    pid_t process_wakeup = pop(sem->cola);
    kill(process_wakeup, SIGUSR1);//Suena que lo mato, pero no es asi
  }
}

/*
int adquire(struct semaphore* sem){
    if (*(sem->avaible) == 0) return 0;

    *(sem->avaible) = 0;
    return 1;
}

void release(struct semaphore* sem){
    *(sem->avaible) = 1;
    return;
}
*/

//Permite hacer operaciones de forma atomica
int compare_and_swap(int *value, int expected, int new_value){
  int temp = *value;
  if (*value == expected)
    *value = new_value;
  return temp;
}


int main(){
    int* goalA = malloc( sizeof(int) );
    int* goalB = malloc( sizeof(int) );

    int* ball = malloc( sizeof(int) );

    int tiempoEspera;

    //int* p = (int*) malloc(2); //Pruebas
    //*p = 0;

    //struct semaphore* semP = new_semaphore(p);

    int cantidadJugadores = 10;
    pid_t parentID = getpid();
    pid_t arrayPIDs[cantidadJugadores*2];//Se almacenan los procesos hijos
    int c = 0;//Contador
    char equipo;
    printf("Arbiter ID: %d.!\n", parentID);
    //Se crean los hijos
    do {

  		pid_t pid;

  		pid = fork();

  		if( pid < 0 ) {
  			fprintf(stderr, "Fork Fail");
  			return 1;
  		}
  		else if ( pid == 0 ) {
  			//execlp("/bin/ls", "ls", NULL);
        equipo = (c >= cantidadJugadores/2) ? 'B' : 'A';
  			printf("Equipo %c | Proceso Hijo creado: #%d, proceso padre #%d \n", equipo, getpid() , getppid()); //El hijo está saludando

        break;
  		}
  		else { //Parent process
  			//printf("The parent is creating more childs\n");//El padre va a seguir creando hijs
  			arrayPIDs[c] = pid;//Guardo el ID del hijo creado para poder esperarlo más tarde
  		}

  		c += 1;
  	} while(cantidadJugadores != c);

    if (getpid() == parentID) {
  		//Instrucción dirigida al padre
  		for (int i = 0; i < cantidadJugadores; i++) {
  			waitpid(arrayPIDs[i], NULL, 0);//Esperamos por cada hijo de forma individual
  			printf("El Proceso Hijo: #%d ha terminado. El padre #%d \n",  arrayPIDs[i], getppid());
  		}
  	}else{
  		//Todos los hijos van a esperar
      printf("Voy a jugar\n");
  		sleep (5);
  	}

    /*
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
    */
    return 0;

}
