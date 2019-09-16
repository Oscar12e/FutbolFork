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
#include <time.h>

#define PLAYERS_PER_TEAM 5
#define TOTAL_PLAYERS 10

//Globals cos reasons
pthread_mutex_t lock;
volatile sig_atomic_t usr_interrupt = 0;

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

struct Nodo* new_nodo() {
    struct Nodo* nodo = create_shared_memory(sizeof(struct Nodo));
    nodo->data = create_shared_memory(sizeof(pid_t));
    *(nodo->data) = -2;
    nodo->siguiente = NULL;
    return nodo;
}


struct Cola{
  struct Nodo *inicio;
  struct Nodo *final;
  unsigned int *size;
  unsigned int *max_size;
};

//max_size = TOTAL_PLAYERS
struct Cola* new_cola() {
    struct Cola* cola = create_shared_memory(sizeof(struct Cola));
    cola->inicio = create_shared_memory(sizeof(struct Nodo));
    cola->final = create_shared_memory(sizeof(struct Nodo));
    cola->inicio = NULL;
    cola->final = NULL;
    cola->size = create_shared_memory(sizeof(int));
    *(cola->size) = 0;
    return cola;
}
//Estos push y pop funcionan con una cola ya establecida

//Pop: Elimina el valor y lo devuelve al final de la cola
pid_t pop(struct Cola *cola){
  *(cola->size) = *(cola->size) - 1;
  pid_t data = *(cola->inicio->data);
  cola->final->siguiente = cola->inicio;// Sera el nuevo final
  cola->inicio = cola->inicio->siguiente;//Nuevo inicio
  cola->final = cola->final->siguiente;
  cola->final->data = NULL;

  return data;
}

void push(struct Cola *cola, pid_t pid){
  if (cola->size == cola->max_size) {
    printf("ERROR: La cola esta llena\n");
  }else{
    struct Nodo *tmp = cola->inicio;

    for (int i = 0; i < *(cola->size); i++) {
      tmp = tmp->siguiente;
    }

    if (*(tmp->data) == -2) {
      *(tmp->data) = pid;
    }else{
      printf("ERROR: Ya existen datos en el nodo\n");
    }
    *(cola->size) = *(cola->size) + 1;
  }
}

void init_push(struct Cola *cola){
  for (int i = 0; i < TOTAL_PLAYERS; i++) {
    if (cola->inicio == NULL) {
      cola->inicio = new_nodo();
      cola->final = cola->inicio;
    }else{
      cola->final->siguiente = new_nodo();
      cola->final = cola->final->siguiente;
    }
  }

}
/**
pid_t pop(struct Cola *cola){
  *(cola->size) = *(cola->size) - 1;
  printf("POP> %d\n", *(cola->inicio->data));
  pid_t data = *(cola->inicio->data);
  struct Nodo *tmp = cola->inicio;
  cola->inicio = cola->inicio->siguiente;

    if (cola->inicio == cola->final){
        cola->inicio = NULL;
        cola->final = NULL;
        return data;
    } else {
        struct Nodo *tmp = cola->inicio;
        cola->inicio = cola->inicio->siguiente;

        //free(tmp);
        return data;
    }
}

void push(struct Cola *cola, pid_t data){
  *(cola->size) = *(cola->size) + 1;

  if (cola->inicio == NULL) {
    cola->inicio = new_nodo(data);
    cola->final = cola->inicio;
  }else{
    cola->final->siguiente = new_nodo(data);
    cola->final = cola->final->siguiente;

  }
}
*/

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
    init_push(sem->cola);
    return sem;
}

//El proceso que desea el recurso lo solicita
void wait_semaphore(struct Semaphore* sem, sigset_t* set){
  int return_val;
  *(sem -> value) = *(sem -> value) - 1; // *(sem -> value)--  NO SIRVE
  if(*(sem -> value) < 0){
    //Se debe agregar a la lista de procesos que esperan el recurso
    push(sem->cola, getpid());
    printf("COLA SIZE: %d \n", *sem->cola->size);
    //Se suspende el proceso para que espere por el recurso
    sigwait(set, &return_val);
    printf("Soy el siguiente y recibi signal, soy: %d\n", getpid());
  }
}

//El proceso que ya uso el recurso lo notifica
void signal_semaphore (struct Semaphore* sem){
  *(sem -> value) = *(sem -> value) + 1;
  if(*(sem -> value) <= 0){//Hay procesos esperando
    printf("Llegué hasta aquí :p cola-size:%d \n", *sem->cola->size);
    pid_t process_wakeup = pop(sem->cola);
    printf("El proceso a despertar es: %d\n", process_wakeup);
    kill(process_wakeup, SIGUSR1);//Suena que lo mato, pero no es asi
  }
  printf("Solte el recurso, soy: %d\n", getpid());
}


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
    srand(time(NULL));

    int* goalA = create_shared_memory( sizeof(int) );
    int* goalB = create_shared_memory( sizeof(int) );

    int* ball = create_shared_memory( sizeof(int) );

    int tiempoEspera;

    bool* inicioPartido = create_shared_memory( sizeof(bool) );
    *inicioPartido = false;
    bool* finPartido = create_shared_memory( sizeof(bool) );
    *finPartido = false;

    struct Semaphore *semaphoreBall = new_semaphore(ball);
    struct Semaphore *semaphoreGoalA = new_semaphore(goalA);
    struct Semaphore *semaphoreGoalB = new_semaphore(goalB);

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
      sleep(3);//Cantidad de segundos que dura el partido
      *finPartido = true;

  		for (int i = 0; i < cantidadJugadores; i++) {
  			waitpid(arrayPIDs[i], NULL, 0);//Esperamos por cada hijo de forma individual
  			printf("El Proceso Hijo: #%d ha terminado. El padre #%d \n",  arrayPIDs[i], getppid());
  		}
      printf("El partido ha finalizado. El marcador es:\nEquipo A: %d | Equipo B: %d\n",
        *(semaphoreGoalA->resource),
        *(semaphoreGoalB->resource)
      );
  	}else{
  		//Todos los hijos van a esperar a que inicie el partido
      while (!*inicioPartido);//BUSY WAITNG

      while(!*finPartido){
        printf("[JUGADOR]: Voy a jugar\n");
        //Ahora deben obtener el recurso bola y la cancha
        wait_semaphore(semaphoreBall, &set);
        //TENGO LA BOLA, ahora busco la cancha
        printf("[JUGADOR %d]: Tengo la bola, voy a buscar la cancha\n", getpid());
        if (equipo == 'A') {
          //Anoto en la cancha B
          wait_semaphore(semaphoreGoalB, &set);
          printf("[JUGADOR %d]: Tengo la bola y la cancha. Voy a anotar\n", getpid());
          *(semaphoreGoalB->resource) = *(semaphoreGoalB->resource) + 1;
          printf("El equipo %c ha anotado!!!\n", equipo);
          signal_semaphore(semaphoreGoalB);
        } else {
          //Anoto en la cancha A
          wait_semaphore(semaphoreGoalA, &set);
          printf("[JUGADOR %d]: Tengo la bola y la cancha. Voy a anotar\n", getpid());
          *(semaphoreGoalA->resource) = *(semaphoreGoalA->resource) + 1;
          printf("El equipo %c ha anotado!!!\n", equipo);
          signal_semaphore(semaphoreGoalA);
        }
        printf("[JUGADOR %d]: Voy a soltar la bola\n", getpid());
        signal_semaphore(semaphoreBall);
        printf("[JUGADOR %d]: Solté la bola\n", getpid());
    	sleep (5);
      }

  	}

    return 0;

}
