/**
*	@auth 	 Jose Manuel Rodriguez Montes
*	@version 0.1, 20 de Mayo de 2017
*	
*	Ejercicio practico para la asignatura de Sistemas Operativos en Tiempo Real(SOTR)\
*	de la Universidad de Malaga(UMA).
*	<p>
*	El ejercicio consiste en un sistema de monitoriacion de temperatura y presion. Se\
*	hace uso de sistemas de sincronicacion tales como mutex y timers(clock_nanosleep)\
*	para su correcta ejecucion.
*	Se van a utilizar 5 hebras, cuyas prioridades se han asignado segun su tiempo de\
*	ejecucion.
*	<p>
*	La documentacion se mostrara en ingles y español segun el nivel de aclaracion del\
*	contenido sea necesario. Las funciones y variables tambien se usaran en español e\
*	ingles(nombres de funcion de hebras en ingles, nombre de los pthread_t en español).\ 
*	Los comentarios mostrados por pantalla seran en castellano. SE OMITIRA EL USO DE\
*	TILDES.
*	<p>
*	El programa esta pensado para ser ejecutado en una sistema GNULinux y un procesador\
*	con un solo nucleo. Las pruebas se han realizado en un sistema GNU/Linux,\
*	distribucion Feroda 25 LXDE DESKTOP con version kernel:	4.10.15-200.fc25.x86_64.
*
*/
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
#define ACT_TEMP   0x02 // 0010 - Bit 1 identificador de estado actuador presion
#define ACT_PRES   0x01 // 0001 - Bit 0 identificador de estado actuador presion
#define DEACT_TEMP 0x0D // 1101
#define DEACT_PRES 0x0E // 1110
#define TEMP_MASK  0x02 // 0010
#define PRES_MASK  0x01 // 0001

#define MAX_PRESSURE    1000
#define MIN_PRESSURE    900
#define MAX_TEMPERATURE 100
#define MIN_TEMPERATURE 90

#define ERROR_SET_PROTOCOL     0x01 // Codigo de errores, usar la funcion {@link #error(error_t error_code) error}
#define ERROR_SET_PRIOCEILING  0x02
#define ERROR_CLOCK_GETTIME    0x03
#define ERROR_CLOCK_NANOSLEEP  0x04
#define ERROR_PTRD_ATTR_INIT   0x05
#define ERROR_PTRD_ATTR_SETINH 0x06
#define ERROR_PTRD_ATTR_SETPOL 0x07
#define ERROR_PTRD_SETSCHED    0x08
#define ERROR_PTRD_CREATE      0x09



// ==============================
// === TIPOS ====================
// ==============================

typedef uint8_t error_t;
typedef uint8_t state_t;



// ==============================
// === VARIABLES GLOBALES =======
// ==============================

int temp, press;
pthread_mutex_t mutex_temp, mutex_press;
state_t state;



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
static void normalizar(struct timespec* tm) {
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

/*
*	Muestra por pantalla la hora actual.
*
*/
void get_actual_time() {
	time_t rawtime;
	struct tm * timeinfo;

	time( &rawtime);
	timeinfo = localtime( &rawtime);
	printf ("\n%s::", asctime(timeinfo) );
}

/**
*	Funcion para mostrar errores por pantalla.
*   
*	@see Codigo de errores en linea 54
*/
void error(error_t error_code) {

	get_actual_time();
	
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
		case ERROR_PTRD_ATTR_INIT:
			printf("ERROR en __pthread_attr_init\n");
			break;
		case ERROR_PTRD_ATTR_SETINH:
			printf("ERROR en __pthread_attr_setinheritsched\n");
			break;
		case ERROR_PTRD_ATTR_SETPOL:
			printf("ERROR en __pthread_attr_setschedpolicy\n");
			break;
		case ERROR_PTRD_SETSCHED:
			printf("ERROR en __pthread_attr_setschedparam\n");
			break;
		case ERROR_PTRD_CREATE:
			printf("ERROR en __pthread_create\n");
			break;
		default:
			printf("ERROR UNKOWN\n");

	}

	printf("Finish the execution...\n");
	exit(-1);
}



// ==============================
// === MAIN =====================
// ==============================

int main() {

	pthread_t monitor;
	pthread_t control_presion, control_temperatura;
	pthread_t sensor_presion, sensor_temperatura;

	pthread_attr_t attr;
	struct sched_param prio;

	init_station();
	configure_mutex();

	// THREADS
	// Configurando attr
	if( pthread_attr_init( &attr) != 0)                                    error(ERROR_PTRD_ATTR_INIT);
	if( pthread_attr_setschedpolicy( &attr, SCHED_FIFO) != 0)              error(ERROR_PTRD_ATTR_SETPOL);
	if( pthread_attr_setinheritsched( &attr, PTHREAD_EXPLICIT_SCHED) != 0) error(ERROR_PTRD_ATTR_SETINH);
	
	// Configurando las prioridades de las hebras y lanzandolas

	// #M
	prio.sched_priority = 1;
	if( pthread_attr_setschedparam(&attr, &prio) != 0)          error(ERROR_PTRD_SETSCHED);
	if( pthread_create(&monitor, &attr, monitoring, NULL) != 0) error(ERROR_PTRD_CREATE);

	// #Ct
	prio.sched_priority = 2;
	if( pthread_attr_setschedparam(&attr, &prio) != 0)                               error(ERROR_PTRD_SETSCHED);
	if( pthread_create(&control_temperatura, &attr, temperature_control, NULL) != 0) error(ERROR_PTRD_CREATE);

	// #St
	prio.sched_priority = 3;
	if( pthread_attr_setschedparam(&attr, &prio) != 0)                             error(ERROR_PTRD_SETSCHED);
	if( pthread_create(&sensor_temperatura, &attr, temperature_sensor, NULL) != 0) error(ERROR_PTRD_CREATE);

	// #Cp
	prio.sched_priority = 4;
	if( pthread_attr_setschedparam(&attr, &prio) != 0)                        error(ERROR_PTRD_SETSCHED);
	if( pthread_create(&control_presion, &attr, pressure_control, NULL) != 0) error(ERROR_PTRD_CREATE);

	// #Sp
	prio.sched_priority = 5;
	if( pthread_attr_setschedparam(&attr, &prio) != 0)                      error(ERROR_PTRD_SETSCHED);
	if( pthread_create(&sensor_presion, &attr, pressure_sensor, NULL) != 0) error(ERROR_PTRD_CREATE);

	pthread_join(monitor, NULL); // #M
	pthread_join(control_temperatura, NULL); // #Ct
	pthread_join(control_presion, NULL); // #Cp
	pthread_join(sensor_presion, NULL); // #Sp
	pthread_join(sensor_temperatura, NULL); // #St

	return 0;
}



// ==============================
// === FUNCIONES ================
// ==============================


/**
*	Funcion para inicializar la estacion, con los valores por defecto.
*
*/
void init_station() {
	state = 0x00;
	temp = 80;
	press = 800;
}

/**
*	Funcion para inicializar los mutex que utilizaran las hebras
*
*/
void configure_mutex() {

	// ====================
	// MUTEX
	// ====================

	pthread_mutexattr_t attr_temp_mutex, attr_press_mutex;

	pthread_mutexattr_init(&attr_temp_mutex);
	pthread_mutexattr_init(&attr_press_mutex);
	
	// temperatura
	if( pthread_mutexattr_setprotocol(&attr_temp_mutex, PTHREAD_PRIO_PROTECT) != 0) error(ERROR_SET_PROTOCOL);
	if( pthread_mutexattr_setprioceiling(&attr_temp_mutex, 3) != 0)                 error(ERROR_SET_PRIOCEILING);

	// presion
	if( pthread_mutexattr_setprotocol(&attr_press_mutex, PTHREAD_PRIO_PROTECT) != 0) error(ERROR_SET_PROTOCOL);
	if( pthread_mutexattr_setprioceiling(&attr_press_mutex, 5) != 0)                 error(ERROR_SET_PRIOCEILING);

	pthread_mutex_init(&mutex_temp, &attr_temp_mutex);
	pthread_mutex_init(&mutex_press, &attr_press_mutex);

}

// FUNCIONES PARA LAS HEBRAS

/**
*	Control Presion. Funcion para el control de la presion; se usa en un hebra.
*	Identificador: #Cp
*	Prioridad: 4
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
			// Abrimos la valvula para bajar la presion
			state |= ACT_PRES;
			get_actual_time();
			printf("CONTROL DE PRESION: ABRIENDO la valvula de presion...\n");
		}
		else if (press > MIN_PRESSURE) {
			// Cerramos la valvula de presion
			state &= DEACT_PRES;
			get_actual_time();
			printf("CONTROL DE PRESION: CERRANDO la valvula de presion...\n");
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
			// Activamos refrigeracion
			state |= ACT_TEMP;

			get_actual_time();
			printf("CONTROL DE TEMPERATURA: ACTIVADNO sistema de refrigeracion...\n");
		}
		else if (temp > MIN_TEMPERATURE) {
			// Desactivamos refrigeracion
			state &= DEACT_TEMP;

			get_actual_time();
			printf("CONTROL DE TEMPERATURA: DESACTIVANDO sistema de refrigeracion...\n");
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
*	Prioridad: 5
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

/**
*	Monitorizacion del sistema. Muestra por pantalla los valores de los sensores y el estado de los actuadores.
*	Identificador: #M
*	Prioridad: 1
*
*	Utiliza clock_nanosleep para hacer los ciclos.
*
*	@mutex mutex_temp  Compartido con #St y #Ct 
*	@mutex mutex_press Compartido con #Sp y #Cp
*
*/
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