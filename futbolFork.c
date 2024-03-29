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

#define PLAYERS_PER_TEAM 6
#define TOTAL_PLAYERS 12

//Propotypes
void* create_shared_memory(size_t size);

//Globals cos reasons
volatile sig_atomic_t usr_interrupt = 0;
bool* inicioPartido;// = create_shared_memory( sizeof(bool) );
//*inicioPartido = false;
bool* finPartido;// =  create_shared_memory( sizeof(bool) );

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
  pid_t data = *(cola->inicio->data);
  *(cola->inicio->data) = -2;
  cola->final->siguiente = cola->inicio;// Sera el nuevo final
  cola->inicio = cola->inicio->siguiente;//Nuevo inicio
  cola->final = cola->final->siguiente;
  *(cola->size) = *(cola->size) - 1;
  return data;
}

int push(struct Cola *cola, pid_t pid){
  if (cola->size == cola->max_size) {
    printf("ERROR: La cola esta llena\n");
    return 0;
  }else{
    struct Nodo *tmp = cola->inicio;

    for (int i = 0; i < *(cola->size); i++) {
      tmp = tmp->siguiente;
    }

    if (*(tmp->data) == -2) {
      *(tmp->data) = pid;
    }else{
      printf("ERROR: Ya existen datos en el nodo\n");
      return 0;
    }
    *(cola->size) = *(cola->size) + 1;
    return 1;
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

void printCola(struct Cola *cola){
  struct Nodo *tmp = cola->inicio;
  for (int i = 0; i < TOTAL_PLAYERS; i++){
    printf("tmp DATOS> %d\n", *(tmp->data));
    tmp = tmp->siguiente;
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
    init_push(sem->cola);
    return sem;
}

//El proceso que desea el recurso lo solicita
//Devuelve un numero para saber si se pudo guardar exitosamente
int wait_semaphore(struct Semaphore* sem, sigset_t* set){
  int return_val;
  *(sem -> value) = *(sem -> value) - 1; // *(sem -> value)--  NO SIRVE
  if(*(sem -> value) < 0){
    //Se debe agregar a la lista de procesos que esperan el recurso
    if (push(sem->cola, getpid()) == 0){ //Si no se agregó
        return 0;
    }
    //Se suspende el proceso para que espere por el recurso
    sigwait(set, &return_val);
    printf("Soy el siguiente de la cola y recibi la signal, soy: %d\n", getpid());
  }

  return 1;
}

//El proceso que ya uso el recurso lo notifica
void signal_semaphore (struct Semaphore* sem){
  *(sem -> value) = *(sem -> value) + 1;
  if(*(sem -> value) <= 0){//Hay procesos esperando
    //printCola(sem->cola);
    pid_t process_wakeup = pop(sem->cola);

    if (process_wakeup == -1){
        printf("Ayuda, la pila esta vacia\n");
        printf("El proceso a despertar es: %d\n", process_wakeup);
    } else {
        kill(process_wakeup, SIGUSR1);//Suena que lo mato, pero no es asi
    }

  }
}

bool wait_semaphore_goals(struct Semaphore* sem){
  bool conseguido = true;
  int tries = 0;
  while (*(sem -> value) <= 0 && tries < 3) {
    printf("[JUGADOR %d]: Intento de anotar #%d\n", getpid(), tries+1);
    sleep(1);//Voy a esperar 3 segundos e intentarlo 3 veces
    tries = tries + 1;
    if (tries == 3) {
      conseguido = false;
    }
  }
  if (conseguido) {
    *(sem -> value) = *(sem -> value) - 1;
  }
  return conseguido;
}

//El proceso que ya uso el recurso lo notifica
void signal_semaphore_goals(struct Semaphore* sem){
  *(sem -> value) = *(sem -> value) + 1;
}

int random_in_range(int min, int max){
  return rand() % (max + 1 - min) + min;
}

int customRandom(int min, int max){
    srand(time(NULL) ^ (getpid() << 16));
    int randomSleep = min + (rand() ) % (max-min);
    //printf("Sleep time %d para: %d\n", randomSleep, getpid());
    return randomSleep;
}

void goalKepperRol(struct Semaphore* ownGoal, char team){
    printf("[Portero %d]: Se acomda los guantes para empezar.\n", getpid());
    while(!*finPartido){
        sleep(customRandom(0, 30));
        if(wait_semaphore_goals(ownGoal)){
            printf("[Portero %d]: Se va capaz de atajar cualquier tiro.\n", getpid());
            //Consegui la cancha estoy atajando
            sleep(customRandom(1, 3));//Protejo la cancha por n segundos
            printf("[Portero %d]: Ha perdido la concentración.\n", getpid());
            signal_semaphore_goals(ownGoal);
          }
    }
}

void playerRol
    (struct Semaphore* semBall, struct Semaphore* ownGoal, struct Semaphore* rivalGoal,
    char team, sigset_t set)
    {

    printf("[JUGADOR %d]: Entro a la cancha a jugar.\n", getpid());
    while(!*finPartido){

        sleep(customRandom(0, 20));
        //sleep (5);//Tiempo antes de ir a volver a buscar el balon
        printf("[JUGADOR %d]: Ha salido a buscar el balón.\n", getpid());

        //Ahora deben obtener el recurso bola y la cancha
        //Queda en cola del semaforo para obtener la bola
        wait_semaphore(semBall, &set); //TENGO LA BOLA, ahora busco la cancha

        printf("[JUGADOR %d]: Tengo la bola, voy a buscar la cancha\n", getpid());


        if( wait_semaphore_goals(rivalGoal) ){ //Consegui la cancha estoy jugando
            printf("[JUGADOR %d]: Lo logró!! Superó las defensas y anotó!!!\n", getpid());
            *(rivalGoal->resource) = *(rivalGoal->resource) + 1;

            printf("--------------------\nEl equipo %c ha anotado!!!El estadio enloquece\nEquipo A: %d | Equipo B: %d\n--------------------\n",
            team, *(ownGoal->resource), *(rivalGoal->resource));
            signal_semaphore_goals(rivalGoal);

        }else {
            printf("[JUGADOR %d]: No logra conectar el balon\n", getpid());
        }

    }
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

    inicioPartido = create_shared_memory( sizeof(bool) );
    finPartido = create_shared_memory( sizeof(bool) );

    *finPartido = false;
    *inicioPartido = false;

    struct Semaphore *semaphoreBall = new_semaphore(ball);
    struct Semaphore *semaphoreGoalA = new_semaphore(goalA);
    struct Semaphore *semaphoreGoalB = new_semaphore(goalB);

    pid_t parentID = getpid();
    pid_t arrayPIDs[TOTAL_PLAYERS];//Se almacenan los procesos hijos
    int c = 0;//Contador
    char equipo;
    bool portero = false;
    printf("Arbiter ID: %d.!\n", parentID);

    //Se crean los hijos
    do {
  		pid_t pid;

  		pid = fork();

  		if ( pid < 0 ) {
  			fprintf(stderr, "Fork Fail");
  			return 1;
  		} else if ( pid == 0 ) {
            portero = (c == 0 || c == PLAYERS_PER_TEAM) ? true : false;
            equipo = (c >= PLAYERS_PER_TEAM) ? 'B' : 'A';
            printf("Equipo %c | Proceso Hijo creado: #%d, proceso padre #%d \n",
            equipo, getpid() , getppid()); //El hijo está saludando
            break;
  		} else { //Parent process
  			arrayPIDs[c] = pid;//Guardo el ID del hijo creado para poder esperarlo más tarde
  		}
  		c += 1;
  	} while(TOTAL_PLAYERS != c);

    srand(time(NULL) ^ (getpid()<<16));

    if (getpid() == parentID) {
        //Instrucción dirigida al padre

        sleep(1);
        printf("Inicio del partido\n");
        *inicioPartido = true;
        sleep(120);//Cantidad de segundos que dura el partido
        *finPartido = true;

        for (int i = 0; i < TOTAL_PLAYERS; i++) {
            kill(arrayPIDs[i], SIGKILL);
  			//waitpid(arrayPIDs[i], NULL, 0);//Esperamos por cada hijo de forma individual
  			printf("El Proceso Hijo: #%d ha sido asesinado por su padre #%d \n",  arrayPIDs[i], getppid());
  		}

        /*
  		for (int i = 0; i < TOTAL_PLAYERS; i++) {
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
      int randomWait;
      if(portero){
        printf("[%c | PORTERO %d]: Voy a proteger la cancha\n", equipo, getpid());
        while(!*finPartido){
          randomWait = random_in_range(5, 20);
          if (equipo == 'A') {
            if(wait_semaphore_goals(semaphoreGoalA)){
              //Consegui la cancha estoy atajando
              sleep (randomWait);//Protejo la cancha por n segundos
              signal_semaphore_goals(semaphoreGoalA);
            }
          }else{
            if(wait_semaphore_goals(semaphoreGoalB)){
              //Consegui la cancha estoy atajando
              sleep (randomWait);//Protejo la cancha por n segundos
              signal_semaphore_goals(semaphoreGoalB);
            }
          }
          printf("[%c | PORTERO %d]: Protegi la cancha por %d segundos.\n", equipo, getpid(), randomWait);
          sleep (random_in_range(5, 20));//Espero n segundos hasta volver a proteger

        }
      }else{
        while(!*finPartido){
          randomWait = random_in_range(5, 20);
          sleep (randomWait);//Tiempo antes de ir a volver a buscar el balon
          printf("[%c | JUGADOR %d]: Voy a buscar el balon. Tiempo de espera [%d segundos]\n", equipo, getpid(), randomWait);
          //Ahora deben obtener el recurso bola y la cancha
          wait_semaphore(semaphoreBall, &set);
          //TENGO LA BOLA, ahora busco la cancha
          printf("[%c | JUGADOR %d]: Tengo la bola, voy a buscar la cancha\n", equipo, getpid());
          if (equipo == 'A') {
            //Anoto en la cancha B
            if(wait_semaphore_goals(semaphoreGoalB)){
              //Consegui la cancha estoy jugando
              printf("[%c | JUGADOR %d]: Tengo la bola y la cancha. Voy a anotar\n", equipo, getpid());
              *(semaphoreGoalB->resource) = *(semaphoreGoalB->resource) + 1;
              printf("El equipo %c ha anotado!!!\nEquipo A: %d | Equipo B: %d\n", equipo,
                *(semaphoreGoalA->resource),
                *(semaphoreGoalB->resource)
              );
              signal_semaphore_goals(semaphoreGoalB);
            }else{
              printf("[%c | JUGADOR %d]: No logra conectar el balon. PORTERO!!!\n", equipo, getpid());
            }
          } else {
            //Anoto en la cancha A
            if(wait_semaphore_goals(semaphoreGoalA)){
              //Consegui la cancha estoy jugando
              printf("[%c | JUGADOR %d]: Tengo la bola y la cancha. Voy a anotar\n", equipo, getpid());
              *(semaphoreGoalA->resource) = *(semaphoreGoalA->resource) + 1;
              printf("El equipo %c ha anotado!!!\nEquipo A: %d | Equipo B: %d\n", equipo,
                *(semaphoreGoalA->resource),
                *(semaphoreGoalB->resource)
              );
              signal_semaphore_goals(semaphoreGoalA);
            }else{
              printf("[%c | JUGADOR %d]: No logra conectar el balon. PORTERO!!!\n", equipo, getpid());
            }
          }
          printf("ANtes de soltar el balon\n");
          signal_semaphore(semaphoreBall);
          printf("[%c | JUGADOR %d]: Solté la bola\n", equipo, getpid());
        }
      }
        */

        printf("El partido ha finalizado. El marcador es:\nEquipo A: %d | Equipo B: %d\n",
            *(semaphoreGoalA->resource),
            *(semaphoreGoalB->resource)
        );

  	} else {
        //struct Semaphore *semaphoreGoalA
        //Todos los hijos van a esperar a que inicie el partido

        struct Semaphore * myGoal;
        struct Semaphore * rivalGoal;

        while (!*inicioPartido);//BUSY WAITNG

        if (equipo == 'A'){
            myGoal = semaphoreGoalA;
            rivalGoal = semaphoreGoalB;
        } else {
            myGoal = semaphoreGoalB;
            rivalGoal = semaphoreGoalA;
        }

        if (portero) goalKepperRol( myGoal, equipo );
        else playerRol(semaphoreBall, myGoal, rivalGoal, equipo, set);
  	}

    return 0;

}
