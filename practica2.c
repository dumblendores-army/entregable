#include <stdio.h>
#include <unistd.h>
#define __USE_UNIX98
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>



// ==============================
// === DEFINES ==================
// ==============================
#define ACT_TEMP   0x02 // 0x0010
#define ACT_PRES   0x01 // 0x0001
#define DEACT_TEMP 0x0D // 0x1101
#define DEACT_PRES 0x0E // 0x1110
#define TEMP_MASK  0x02 // 0x0010
#define PRES_MASK  0x01 // 0x0001

#define MAX_PRESSURE    1000
#define MIN_PRESSURE    900
#define MAX_TEMPERATURE 100
#define MIN_TEMPERATURE 90

#define ERROR_SET_PROTOCOL    0x01
#define ERROR_SET_PRIOCEILING 0x02
#define ERROR_CLOCK_GETTIME   0x03
#define ERROR_CLOCK_NANOSLEEP 0x04



// ==============================
// === TIPOS ====================
// ==============================
typedef uint8_t error_t;



// ==============================
// === VARIABLES GLOBALES =======
// ==============================

int temp, press;
pthread_mutex_t mutex_temp, mutex_press;
uint8_t state;



// ==============================
// === FUNCIONES ================
// ==============================

// Inicializaciones
void init_station(); // Initialize the station
void configure_mutex(); // Initialize the mutex

// Para las hebras
void *pressure_control( void *args); // #Cp
void *pressure_sensor( void *args); // #Sp
void *temperature_control( void *args); // #Ct
void *temperature_sensor( void *args); // #St
void *monitoring( void *args); // #M

/**
*  	Funcion para hacer una espera activa
*
*  	@param tiempo tiempo en milisegundos que queremos que espere la funcion
*/
void ms_espera_activa( int tiempo) {

	// TODO: BIG PROBLEM, DON'T WORK, consultar con el profe
	struct timeval start, now;

	gettimeofday(&start, NULL);

	do {
		gettimeofday(&now, NULL);
	}while( (now.tv_usec - start.tv_usec) < tiempo*1000);
	
}

/**
*	Temporización mediante relojes. Cosas del profesor
*
*	@param tm variable de tiempo a normalizar
*/
static void normalizar(struct timespec* tm)
{
    if (tm->tv_nsec >= 1000000000L) {
        tm->tv_sec += (tm->tv_nsec / 1000000000L);
        tm->tv_nsec = (tm->tv_nsec % 1000000000L);
    } else if (tm->tv_nsec <= -1000000000L) {
        tm->tv_sec += - (-tm->tv_nsec / 1000000000L);
        tm->tv_nsec = - (-tm->tv_nsec % 1000000000L);
    }
    if ((tm->tv_sec > 0)&&(tm->tv_nsec < 0)) {
        tm->tv_nsec += 1000000000L;
        tm->tv_sec -= 1;
    } else if ((tm->tv_sec < 0)&&(tm->tv_nsec > 0)) {
        tm->tv_nsec -= 1000000000L;
        tm->tv_sec += 1;
    }
    assert((tm->tv_sec>=0 && tm->tv_nsec>=0 && tm->tv_nsec<1000000000L)
           ||(tm->tv_sec<0 && tm->tv_nsec>-1000000000L && tm->tv_nsec<=0));
}

/**
*	Funcion para mostrar errores por pantalla
*   
*	@see Codigo de errores en linea 27
*/
void error(uint8_t error_code) {
	
	switch(error_code) {
		case ERROR_SET_PROTOCOL:
			printf("ERROR en __pthread_mutexattr_setprotocol\n");
			break;
		case ERROR_SET_PRIOCEILING:
			printf("ERROR en __pthread_mutexattr_setprioceiling\n");
			break;
		case ERROR_CLOCK_GETTIME:
			printf("ERROR en __clock_gettime\n");
			break;
		case ERROR_CLOCK_NANOSLEEP:
			printf("ERROR en __clock_nanosleep\n");
			break;
		default:
			printf("ERROR UNKOWN\n");

	}

	printf("Finish the execution\n");
	exit(-1);
}



// ==============================
// === MAIN =====================
// ==============================

int main() {

	init_station();
	configure_mutex();

	return 0;
}



// ==============================
// === FUNCIONES ================
// ==============================


/**
* Funcion para inicializar la estacion, con los valores por defecto
*
*/
void init_station() {
	state = 0x00;
	temp = 80;
	press = 800;
}

/**
* Funcion para inicializar los mutex que utilizaran las hebras
*
*/
void configure_mutex() {

	// ====================
	// MUTEX
	// ====================

	pthread_mutexattr_t attr_temp_mutex, attr_press_mutex;

	pthread_mutexattr_init(&attr_temp_mutex);
	pthread_mutexattr_init(&attr_press_mutex);
	
	if( pthread_mutexattr_setprotocol(&attr_temp_mutex, PTHREAD_PRIO_PROTECT) != 0) error(ERROR_SET_PROTOCOL);
	if( pthread_mutexattr_setprioceiling(&attr_temp_mutex, 3) != 0) error(ERROR_SET_PRIOCEILING);

	if( pthread_mutexattr_setprotocol(&attr_press_mutex, PTHREAD_PRIO_PROTECT) != 0) error(ERROR_SET_PROTOCOL);
	if( pthread_mutexattr_setprioceiling(&attr_press_mutex, 3) != 0) error(ERROR_SET_PRIOCEILING);

	pthread_mutex_init(&mutex_temp, &attr_temp_mutex);
	pthread_mutex_init(&mutex_press, &attr_press_mutex);

}

// FUNCIONES PARA LAS HEBRAS

/**
*	Control Presion. Funcion para el control de la presion; se usa en un hebra.
*	Identificador: #Cp
*	Prioridad: 2
*	
*	Utiliza clock_nanosleep para hacer los ciclos. Cambia el estado del sistema\
*	para abrir/cerrar la valvula, haciendo que baje la presion.
*
*	@mutex mutex_press Compartido con #M y #Sp
*
*/
void *pressure_control( void *args) {

	struct timespec next_activation;

	if( clock_gettime(CLOCK_MONOTONIC, &next_activation)) error(ERROR_CLOCK_GETTIME);

	while(1) {

		pthread_mutex_lock(&mutex_press);

		if (press > MAX_PRESSURE) {
			state |= ACT_PRES;
		}
		else if (press > MIN_PRESSURE) {
			state &= DEACT_PRES;
		}
		ms_espera_activa(350); // TODO: ir a funciones para ver el estado

		pthread_mutex_unlock(&mutex_press);

		// TODO: Comprobar que esto es correcto, para el tema de los milisegundos
		next_activation.tv_nsec += 350000L; // Calculamos siguiente activacion
		normalizar(&next_activation); // Normalizamos

		if( clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_activation, NULL)) error(ERROR_CLOCK_NANOSLEEP);
	}

}

/**
*	Control Temperatura. Funcion para el control de la temperatura; se usa en un hebra.
*	Identificador: #Ct
*	Prioridad: 2
*	
*	Utiliza clock_nanosleep para hacer los ciclos. Cambia el estado del sistema\
*	para abrir/cerrar la refrigeracion, haciendo que baje la temperatura.
*
*	@mutex mutex_temp Compartido con #M y #St
*
*/
void *temperature_control( void *args) {

	struct timespec next_activation;

	if( clock_gettime(CLOCK_MONOTONIC, &next_activation)) error(ERROR_CLOCK_GETTIME);

	while(1) {

		pthread_mutex_lock(&mutex_temp);

		if (temp > MAX_TEMPERATURE) {
			state |= ACT_TEMP;
		}
		else if (temp > MIN_TEMPERATURE) {
			state &= DEACT_TEMP;
		}
		ms_espera_activa(450); // TODO: ir a funciones para ver el estado

		pthread_mutex_unlock(&mutex_temp);

		// TODO: Comprobar que esto es correcto, para el tema de los milisegundos
		next_activation.tv_nsec += 500000L; // Calculamos siguiente activacion
		normalizar(&next_activation); // Normalizamos

		if( clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_activation, NULL)) error(ERROR_CLOCK_NANOSLEEP);
	}
}

/**
*	Sensor Presion. Funcion para actualizar el valor de la presion; se usa en una hebra.
*	Identificador: #Sp
*	Prioridad: 3
*
*	Utiliza clock_nanosleep para hacer los ciclos. Modifica el valor del sensor\
*	dependiendo de si esta o no abierta la valvula, aumentara o disminuira.
*
*	@mutex mutex_press Compartido con #M y #Cp 
*
*/
void *pressure_sensor( void *args) {

	struct timespec next_activation;

	if( clock_gettime(CLOCK_MONOTONIC, &next_activation)) error(ERROR_CLOCK_GETTIME);

	while(1) {

		pthread_mutex_lock(&mutex_press);

		if( (state & PRES_MASK) > 0) {
			// valvula abierta
			press -= 20;
		} else {
			// valvula cerrada
			press += 10;
		}
		ms_espera_activa(250); // TODO: ir a funciones para ver el estado

		pthread_mutex_unlock(&mutex_press);

		// TODO: Comprobar que esto es correcto, para el tema de los milisegundos
		next_activation.tv_nsec += 300000L; // Calculamos siguiente activacion
		normalizar(&next_activation); // Normalizamos

		if( clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_activation, NULL)) error(ERROR_CLOCK_NANOSLEEP);
	}

}

/**
*	Sensor Presion. Funcion para actualizar el valor de la presion; se usa en una hebra.
*	Identificador: #St
*	Prioridad: 3
*
*	Utiliza clock_nanosleep para hacer los ciclos. Modifica el valor del sensor\
*	dependiendo de si esta o no acativada la refrigeracion, aumentara o disminuira.
*
*	@mutex mutex_temp Compartido con #M y #Ct 
*
*/
void *temperature_sensor( void *args) {

	struct timespec next_activation;

	if( clock_gettime(CLOCK_MONOTONIC, &next_activation)) error(ERROR_CLOCK_GETTIME);

	while(1) {

		pthread_mutex_lock(&mutex_temp);

		if( (state & TEMP_MASK) > 0) {
			// refrigeracion activada
			temp -= 2;
		} else {
			// refrigeracion desactivada
			temp += 1;
		}
		ms_espera_activa(370); // TODO: ir a funciones para ver el estado

		pthread_mutex_unlock(&mutex_temp);

		// TODO: Comprobar que esto es correcto, para el tema de los milisegundos
		next_activation.tv_nsec += 400000L; // Calculamos siguiente activacion
		normalizar(&next_activation); // Normalizamos

		if( clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_activation, NULL)) error(ERROR_CLOCK_NANOSLEEP);
	}

}

void *monitoring( void *args) {

	struct timespec next_activation;

	if( clock_gettime(CLOCK_MONOTONIC, &next_activation)) error(ERROR_CLOCK_GETTIME);

	while(1) {

		pthread_mutex_lock(&mutex_temp);
		pthread_mutex_lock(&mutex_press);

		printf("\n\n");
		printf("-Temperatura = %d\n", temp);
		printf("-Presion     = %d\n", press);
		printf("-Estado de los sistemas auxiliares:\n");

		switch(state) {
			case 0x00:
				printf("\tREFRIGERACION DESACTIVADA\n");
				printf("\tVALVULA de presion CERRADA\n");
				break;
			case 0x01:
				printf("\tREFRIGERACION DESACTIVADA\n");
				printf("\tVALVULA de presion ABIERTA\n");
				break;
			case 0x02:
				printf("\tREFRIGERACION ACTIVADA\n");
				printf("\tVALVULA de presion CERRADA\n");
				break;
			case 0x03:
				printf("\tREFRIGERACION ACTIVADA\n");
				printf("\tVALVULA de presion ABIERTA\n");
				break;
		}
		printf("\n");

		pthread_mutex_unlock(&mutex_press);
		pthread_mutex_unlock(&mutex_temp);

		// TODO: Comprobar que esto es correcto, para el tema de los milisegundos
		next_activation.tv_nsec += 400000L; // Calculamos siguiente activacion
		normalizar(&next_activation); // Normalizamos

		if( clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_activation, NULL)) error(ERROR_CLOCK_NANOSLEEP);
	}

}