/**
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
*	El programa esta pensado para ser ejecutado en una sistema GNU/Linux y un procesador\
*	con un solo nucleo. Las pruebas se han realizado en un sistema GNU/Linux,\
*	distribucion Feroda 25 LXDE DESKTOP con version kernel:	4.10.15-200.fc25.x86_64.
*	<p>
*	El programa se debe ejecutar con permisos de super usuario, se crearan 2 ficheros:
*	<ul>
*	<li> sensor_data.txt: Fichero que usarla index.js para pintar las graficas.
*	<li> estacion_sotr.log: Ficherlo ubicado en /var/log para guardar un registro del\
*		 programa.
*	</ul>
*
*	@auth 	 Jose Manuel Rodriguez Montes
*	@version 1.0.1, 24 de Mayo de 2017
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
#define ACT_TEMP   0x02 // 0010 - Bit 1 identificador de estado actuador temperatura
#define ACT_PRES   0x01 // 0001 - Bit 0 identificador de estado actuador presion
#define DEACT_TEMP 0x0D // 1101
#define DEACT_PRES 0x0E // 1110
#define TEMP_MASK  0x02 // 0010
#define PRES_MASK  0x01 // 0001

#define MAX_PRESSURE    1000
#define MIN_PRESSURE    900
#define MAX_TEMPERATURE 100
#define MIN_TEMPERATURE 90

#define ERROR_SET_PROTOCOL     0x01 // #E Codigo de errores, usar la funcion {@link #error(error_t error_code) error}
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
FILE *fp_sensor, *fp_log; // #F
const char *DATA_FILE = "/home/j0sete/Documentos/Universidad/PRACTICA2_SOTR/sensor_data.txt"; // fichero donde guardamos los datos de los sensores
const char *LOG_FILE = "/var/log/estacion_sotr.log"; // Fichero donde guardamos un registro log del programa



// ==============================
// === FUNCIONES ================
// ==============================

// Inicializaciones
void init_station(); // Initialize the station
void configure_mutex(); // Initialize the mutex
void write_data(); // For save the data inside a file

// Para las hebras
void *pressure_control( void *args); // #Cp
void *pressure_sensor( void *args); // #Sp
void *temperature_control( void *args); // #Ct
void *temperature_sensor( void *args); // #St
void *monitoring( void *args); // #M

/**
*	@Deprecated
*  	Funcion para hacer una espera activa. Finalmente no se utiliza.
*
*  	@param tiempo tiempo en milisegundos que queremos que espere la funcion
*/
void ms_espera_activa( int tiempo) {

	struct timespec start, now;
	long diff;

	clock_gettime(CLOCK_MONOTONIC, &start);

	do {
		clock_gettime(CLOCK_MONOTONIC, &now);
		diff = (now.tv_sec - start.tv_sec)*1000 + (now.tv_nsec - start.tv_nsec)/1000000L;
	}while( diff < tiempo);
	
}

/**
*	Temporización mediante relojes.
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

/**
*	Devuelve en el parametro fecha la hora actual en el formato:\
*	HH:MM:SS
*
*	@param fecha cadena de caracteres donde guardar la hora
*	@param fecha_size tamaño que se tiene para guardar la cadena, MINIMO 9
*/
void get_actual_date( char *fecha, size_t fecha_size) {

	time_t rawtime;
	struct tm *tm_info;

	time(&rawtime);
	tm_info = localtime( &rawtime);

	strftime(fecha, fecha_size, "%H:%M:%S", tm_info);

}

/**
*	Funcion para guardar mensajes del programa en un fichero en /var/log
*	TODO: log level for verbose
*
*	@param msg mensaje a guardar junto con el log
*/
void log_msg(char *msg) {

	char *time_data;
	static int cnt = 0;

	time_data = malloc( sizeof(char) * 9);
	get_actual_date(time_data, 9);

	fprintf(fp_log, "%s::DEBUG::%d::%s\n", time_data, cnt++, msg);
	fflush(fp_log);

	free(time_data);

}

/**
*	Funcion para mostrar errores por pantalla.
*   
*	@see Codigo de errores, etiqueta #E
*/
void error(error_t error_code) {

	switch(error_code) {
		case ERROR_SET_PROTOCOL:
			log_msg("ERROR en __pthread_mutexattr_setprotocol");
			break;
		case ERROR_SET_PRIOCEILING:
			log_msg("ERROR en __pthread_mutexattr_setprioceiling");
			break;
		case ERROR_CLOCK_GETTIME:
			log_msg("ERROR en __clock_gettime");
			break;
		case ERROR_CLOCK_NANOSLEEP:
			log_msg("ERROR en __clock_nanosleep");
			break;
		case ERROR_PTRD_ATTR_INIT:
			log_msg("ERROR en __pthread_attr_init");
			break;
		case ERROR_PTRD_ATTR_SETINH:
			log_msg("ERROR en __pthread_attr_setinheritsched");
			break;
		case ERROR_PTRD_ATTR_SETPOL:
			log_msg("ERROR en __pthread_attr_setschedpolicy");
			break;
		case ERROR_PTRD_SETSCHED:
			log_msg("ERROR en __pthread_attr_setschedparam");
			break;
		case ERROR_PTRD_CREATE:
			log_msg("ERROR en __pthread_create");
			break;
		default:
			log_msg("ERROR UNKOWN");

	}

	log_msg("Exiting of the program...");
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
	if( pthread_attr_setinheritsched( &attr, PTHREAD_EXPLICIT_SCHED) != 0) error(ERROR_PTRD_ATTR_SETINH);
	if( pthread_attr_setschedpolicy( &attr, SCHED_RR) != 0)                error(ERROR_PTRD_ATTR_SETPOL);
	
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

	// cerrarmos fichero #F
	fclose(fp_sensor);
	fclose(fp_log);
	
	return 0;
}



// ==============================
// === FUNCIONES ================
// ==============================


/**
*	Funcion para inicializar la estacion, con los valores por defecto. #F
*
*/
void init_station() {
	
	// Inicializamos las variables
	state = 0x00;
	temp = 80;
	press = 800;

	// Abrimos los ficheros
	fp_sensor = fopen(DATA_FILE, "a+");
	fp_log = fopen(LOG_FILE, "a+");

}

/**
*	Funcion para inicializar los mutex que utilizaran las hebras.
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

		//printf("Cp::Cogiendo mutex_press\n");
		pthread_mutex_lock(&mutex_press);
		//printf("Cp::Cogiedo mutex_press\n");

		if (press > MAX_PRESSURE) {
			// Abrimos la valvula para bajar la presion
			state |= ACT_PRES;
			//get_actual_time();
			log_msg("CONTROL DE PRESION: ABRIENDO la valvula de presion...");
		}
		else if (press < MIN_PRESSURE && (state & PRES_MASK) > 0) {
			// Cerramos la valvula de presion
			state &= DEACT_PRES;
			//get_actual_time();
			log_msg("CONTROL DE PRESION: CERRANDO la valvula de presion...");
		}
		//ms_espera_activa(350);

		//printf("Cp::Soltando mutex_press\n");
		pthread_mutex_unlock(&mutex_press);
		//printf("Cp::Soltado mutex_press\n");

		next_activation.tv_nsec += 350000000L; // Calculamos siguiente activacion
		normalizar(&next_activation); // Normalizamos

		//printf("Cp::A dormir\n");
		if( clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_activation, NULL)) error(ERROR_CLOCK_NANOSLEEP);
		//printf("Cp::Despertando\n");
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

		//printf("Ct::Cogiendo mutex_temp\n");
		pthread_mutex_lock(&mutex_temp);
		//printf("Ct::Cogido mutex_temp\n");

		if (temp > MAX_TEMPERATURE) {
			// Activamos refrigeracion
			state |= ACT_TEMP;

			//get_actual_time();
			log_msg("CONTROL DE TEMPERATURA: ACTIVANDO sistema de refrigeracion...");
		}
		else if (temp < MIN_TEMPERATURE && (state & TEMP_MASK) > 0) {
			// Desactivamos refrigeracion
			state &= DEACT_TEMP;

			//get_actual_time();
			log_msg("CONTROL DE TEMPERATURA: DESACTIVANDO sistema de refrigeracion...");
		}
		//ms_espera_activa(450);

		//printf("Ct::Soltando mutex_temp\n");
		pthread_mutex_unlock(&mutex_temp);
		//printf("Ct::Soltado mutex_press\n");

		next_activation.tv_nsec += 500000000L; // Calculamos siguiente activacion
		normalizar(&next_activation); // Normalizamos

		//printf("Ct::Me voy a dormir\n");
		if( clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_activation, NULL)) error(ERROR_CLOCK_NANOSLEEP);
		//printf("Ct::Me he despertado\n");
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

		//printf("Sp::Cogiendo mutex_press\n");
		pthread_mutex_lock(&mutex_press);
		//printf("Sp::Cogido mutex_press\n");

		if( (state & PRES_MASK) > 0) {
			// valvula abierta
			press -= 20;
		} else {
			// valvula cerrada
			press += 10;
		}
		//ms_espera_activa(250);

		//printf("Sp::Soltando mutex_press\n");
		pthread_mutex_unlock(&mutex_press);
		//printf("Sp::Soltado mutex_press\n");

		next_activation.tv_nsec += 300000000L; // Calculamos siguiente activacion
		normalizar(&next_activation); // Normalizamos

		//printf("Sp::Me voy a dormir\n");
		if( clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_activation, NULL)) error(ERROR_CLOCK_NANOSLEEP);
		//printf("Sp::Me he despertado\n");
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

		//printf("St::Cogiendo mutex_temp\n");
		pthread_mutex_lock(&mutex_temp);
		//printf("St::Cogido mutex_temp\n");

		if( (state & TEMP_MASK) > 0) {
			// refrigeracion activada
			temp -= 2;
		} else {
			// refrigeracion desactivada
			temp += 1;
		}
		//ms_espera_activa(370);

		//printf("St::Soltando mutex_temp\n");
		pthread_mutex_unlock(&mutex_temp);
		//printf("St::Soltado mutex_temp\n");

		next_activation.tv_nsec += 400000000L; // Calculamos siguiente activacion
		normalizar(&next_activation); // Normalizamos

		//printf("St::Voy a dormir\n");
		if( clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_activation, NULL)) error(ERROR_CLOCK_NANOSLEEP);
		//printf("St::Me he despertado\n");
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

		//printf("M::Cogiendo mutex_temp\n");
		pthread_mutex_lock(&mutex_temp);
		//printf("M::Cogido mutex_temp\n");
		//printf("M::Cogiendo mutex_press\n");
		pthread_mutex_lock(&mutex_press);
		//printf("M::Cogido mutex_press\n");

		printf("\n\n");
		printf("=============================================\n");
		printf("ESTADO DEL SISTEMA:\n");
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
		printf("=============================================\n");
		printf("\n");

		// guardamos temperatura
		write_data();

		//ms_espera_activa(800);

		//printf("M::Soltando mutex_press\n");
		pthread_mutex_unlock(&mutex_press);
		//printf("M::Soltado mutex_press\n");
		//printf("M::Soltando mutex_temp\n");
		pthread_mutex_unlock(&mutex_temp);
		//printf("M::Soltado mutex_temp\n");

		next_activation.tv_sec += 1; // Calculamos siguiente activacion
		normalizar(&next_activation); // Normalizamos

		//printf("M::Me voy a dormir\n");
		if( clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_activation, NULL)) error(ERROR_CLOCK_NANOSLEEP);
		//printf("M::Me he despertado\n");
	}

}

/**
*	Funcion para guardar los datos de los sensores. Es llamada por #monitoring() #M
*	#F
*/
void write_data() {

	char *time_data;
	time_data = malloc( sizeof(char) * 9); // Usamos 9 porque es el minimo tamaño necesario {@link #get_actual_time(char *, size_t)}

	get_actual_date(time_data, 9);

	fprintf(fp_sensor, "%s %d %d\n", time_data, temp, press);
	fflush(fp_sensor); // Para que los datos se vuelquen en el fichero

	free(time_data);

}