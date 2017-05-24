# Introduccion

Ejercicio practico para la asignatura de Sistemas Operativos en Tiempo Real(SOTR)
de la Universidad de Malaga(UMA).

El ejercicio consiste en un sistema de monitoriacion de temperatura y presion. Se hace uso 
de sistemas de sincronicacion tales como mutex y timers(clock_nanosleep) para su correcta ejecucion.
Se van a utilizar 5 hebras, cuyas prioridades se han asignado segun su tiempo de ejecucion.

La documentacion se mostrara en ingles y español segun el nivel de aclaracion del contenido sea necesario. 
Las funciones y variables tambien se usaran en español e ingles(nombres de funcion de hebras en ingles, 
nombre de los pthread_t en español). Los comentarios mostrados por pantalla seran en castellano. SE 
OMITIRA EL USO DE TILDES.

El programa esta pensado para ser ejecutado en una sistema GNU/Linux y un procesador con un solo nucleo. 
Las pruebas se han realizado en un sistema GNU/Linux, distribucion Feroda 25 LXDE DESKTOP con version 
kernel: 4.10.15-200.fc25.x86_64.

El programa se debe ejecutar con permisos de super usuario, se crearan 2 ficheros:

* sensor_data.txt: Fichero que usarla index.js para pintar las graficas.

* estacion_sotr.log: Ficherlo ubicado en /var/log para guardar un registro del programa.

# Ejecucion

Para compilar el programa:

> $ gcc -o ejecutable practica2.c -pthread -lrt

Depues **debe ser ejecutado como super usuario**:

> sudo ./ejecutable

Sino las hebras no se podran crear y el programa se cerrara

El programa, como se dice en la introcuccion, creara 2 ficheros, uno en la misma ubicacion de ejecucion y otro en /var/log.

# Visualizacion de datos

Para visualizar los datos basta con abrir el fichero index.html, que debera encontrarse en el mismo directorio, se veran 2
graficas con los ultimos datos guardados por el sistema con su hora.

# Creditos

Autor: Jose Manuel Rodriguez Montes

Version: 1.0, 24 de Mayo de 2017
