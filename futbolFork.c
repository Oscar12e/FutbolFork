#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/mman.h>
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

void* create_shared_memory(size_t size) {
  // Readable and writable:
  int protection = PROT_READ | PROT_WRITE;

  // Visibility defined so
  // Only this process and its children will be able to use it:
  int visibility = MAP_SHARED | MAP_ANONYMOUS;

  return mmap(NULL, size, protection, visibility, -1, 0);
}

//Creacion de una cola
struct Nodo{
  pid_t* data;
  struct Nodo *siguiente;

};

struct Nodo* new_nodo(pid_t data) {
    struct Nodo* nodo = create_shared_memory(sizeof(struct Nodo));
    nodo->data = create_shared_memory(sizeof(pid_t));
    *(nodo->data) = data;
    nodo->siguiente = NULL;
    return nodo;
}

struct Cola{
  struct Nodo *inicio;
  struct Nodo *final;
  unsigned int *size;
};

struct Cola* new_cola() {
    struct Cola* cola = create_shared_memory(sizeof(struct Cola));
    cola->inicio = NULL;
    cola->final = NULL;
    cola->size = create_shared_memory(sizeof(int));
    *(cola->size) = 0;
    return cola;
}

pid_t pop(struct Cola *cola){
  cola->size--;

  pid_t data = *(cola->inicio->data);
  struct Nodo *tmp = cola->inicio;
  cola->inicio = cola->inicio->siguiente;

  free(tmp);
  return data;
}

void push(struct Cola *cola, pid_t data){
  cola->size++;

  if (cola->inicio == NULL) {
    cola->inicio = new_nodo(data);
    cola->final = cola->inicio;
  }else{
    cola->final->siguiente = new_nodo(data);
    cola->final = cola->final->siguiente;
  }
}


struct Semaphore{
  int *value;
  int *resource;
  struct Cola *cola;
};

struct Semaphore* new_semaphore(int* pResource) {
    struct Semaphore* sem = create_shared_memory(sizeof(struct Semaphore));
    sem->value = create_shared_memory(sizeof(int));
    *(sem->value) = 1;
    sem->resource = pResource;
    sem->cola = new_cola();
    return sem;
}

//El proceso que desea el recurso lo solicita
void wait_semaphore(struct Semaphore* sem, sigset_t* set){
  int return_val;
  printf("VALOR ANTES WAIT > %d\n", *(sem -> value));
  *(sem -> value) = *(sem -> value) - 1; // *(sem -> value)--  NO SIRVE
  printf("VALOR DESPUES WAIT > %d\n", *(sem -> value));
  if(*(sem -> value) < 0){
    //Se debe agregar a la lista de procesos que esperan el recurso
    push(sem->cola, getpid());
    //Se suspende el proceso para que espere por el recurso
    sigwait(set, &return_val);
    printf("Soy el siguiente y recibi signal, soy: %d\n", getpid());
  }
}

//El proceso que ya uso el recurso lo notifica
void signal_semaphore (struct Semaphore* sem){
  printf("VALOR ANTES SIGNAL > %d\n", *(sem -> value));
  *(sem -> value) = *(sem -> value) + 1;
  printf("VALOR DESPUES SIGNAL > %d\n", *(sem -> value));
  if(*(sem -> value) <= 0){//Hay procesos esperando
    pid_t process_wakeup = pop(sem->cola);
    printf("El proceso a despertar es: %d\n", process_wakeup);
    kill(process_wakeup, SIGUSR1);//Suena que lo mato, pero no es asi
  }
  printf("Solte el recurso, soy: %d\n", getpid());
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
    //Signals info
    sigset_t set;
    sigfillset(&set);
    sigprocmask(SIG_BLOCK, &set, NULL);

    //Program
    int* goalA = create_shared_memory( sizeof(int) );
    int* goalB = create_shared_memory( sizeof(int) );

    int* ball = create_shared_memory( sizeof(int) );

    int tiempoEspera;

    bool* inicioPartido = create_shared_memory( sizeof(bool) );
    *inicioPartido = false;
    bool* finPartido = create_shared_memory( sizeof(bool) );
    *finPartido = false;

    struct Semaphore *semaphoreBall = new_semaphore(ball);

    //int* p = (int*) malloc(2); //Pruebas
    //*p = 0;

    //struct semaphore* semP = new_semaphore(p);

    int cantidadJugadores = 10;
    pid_t parentID = getpid();
    pid_t arrayPIDs[cantidadJugadores];//Se almacenan los procesos hijos
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
      sleep(1);
      printf("Inicio del partido\n");
      *inicioPartido = true;
      sleep(5);//Cantidad de segundos que dura el partido
      *finPartido = true;

  		for (int i = 0; i < cantidadJugadores; i++) {
  			waitpid(arrayPIDs[i], NULL, 0);//Esperamos por cada hijo de forma individual
  			printf("El Proceso Hijo: #%d ha terminado. El padre #%d \n",  arrayPIDs[i], getppid());
  		}
  	}else{
  		//Todos los hijos van a esperar a que inicie el partido
      while (!*inicioPartido) {
        ;//BUSY WAITNG
      }
      while(!*finPartido){
        printf("Voy a jugar\n");
        //Ahora deben obtener el recurso bola y la cancha
        sleep (1);
        wait_semaphore(semaphoreBall, &set);
        //TENGO LA BOLA
        sleep (1);
        *(semaphoreBall->resource) = *(semaphoreBall->resource) + 1;
        printf("Consegui la bola %d. Yo soy %d\n", *(semaphoreBall->resource), getpid());
        sleep (1);
        signal_semaphore(semaphoreBall);
    		sleep (5);
      }

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
