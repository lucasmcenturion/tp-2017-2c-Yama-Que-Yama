#ifndef HELPER_
#define HELPER_
#define LAMBDA(c_) ({ c_ _;}) //Para funciones lambda

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <commons/temporal.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/queue.h>
#include <commons/string.h>
#include <commons/txt.h>
#include <commons/bitarray.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <ctype.h>
#include <semaphore.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdbool.h>

#define true 1
#define false 0
#define NULL ((void *) 0)

typedef enum { TRANSFORMACION, REDUCCIONLOCAL, REDUCCIONGLOBAL, ALMACENAMIENTOFINAL} etapa;
typedef enum { ENPROCESO, ERROR, FINALIZADOOK} estado;
typedef enum { ALMACENARARCHIVO, LEERARCHIVO} interfazFilesystem;

typedef struct{
	char* nodo;
	char* ip;
	uint32_t puerto;
	uint32_t cargaDeTrabajo;
	uint32_t disponibilidad;
	uint32_t contTareasRealizadas;
	uint32_t indice;
}
__attribute__((packed))
datosWorker;

typedef struct{
	int id;
	int socket;
	int contJobs;
}
__attribute__((packed))
master;

typedef struct {
	datosWorker worker;
	int nodo;
	int bloque;
	int bytesOcupados;
	char* archTemp;
}__attribute__((packed))
transformacionDatos;



typedef struct {
	int numero_bloque;
	int tamanio;
	int primer_bloque_nodo;
	int segundo_bloque_nodo;
	char*primer_nombre_nodo;
	char*segundo_nombre_nodo;
}__attribute__((packed))t_bloque_yama;

typedef struct {
	bool resultado;
	int bloque;
	char*nombre_nodo;
}__attribute__((packed))
t_resultado_envio;

typedef struct {
	int bloque;
	char*nombre_nodo;
}__attribute__((packed))
t_bloque_enviado;

typedef struct {
	datosWorker worker;
	char** listaArchivosTemps; //Lista de nombres de archivos temporales de cada worker
	char* archTemp;
}__attribute__((packed))
reduccionLocalDatos;

typedef struct {
	datosWorker worker;
	datosWorker WorkerEncargado;
	int nodoEncargado;
	char** listaArchivosTemps; //Lista de nombres de archivos temporales de cada worker
	char* archTemp;
}__attribute__((packed))
reduccionGlobalDatos;

typedef struct {
	datosWorker worker;
	char* progTrans; //Programa de Transformacion
	int bloques;
	int bytesOcupados;
	char* archTemp;
}__attribute__((packed))
datosTransWorker;

typedef struct {
	int job;
	int master;
	char* nodo;
	char* nodoConOtraCopia;
	int nroBloque;
	int nroBloqueCopia;
	int bloque;
	etapa etapa;
	char* archivoTemporal;
	char* archivoFinal;
	estado estado;
	int tamanio;
}__attribute__((packed))
registroEstado;

typedef struct{
	uint32_t socket;
	uint32_t bloques_totales;
	uint32_t bloques_libres;
	uint32_t puertoworker;
	char *nodo;
	char *ip_nodo;
}__attribute__((packed))
info_datanode;

typedef struct{
	char*nombre_nodo;
	int cantidad;
}__attribute__((packed))
t_solicitudes;

typedef struct{
 int numero;
 int copia;
 int tamanio;
 void *datos;
 char *nombre_archivo;
}__attribute__((packed)) bloque;

typedef struct{
 char *nombre;
 uint32_t cantidad;
 uint32_t socket;
}__attribute__((packed)) t_datanode_a_enviar;

typedef struct{
 int index;
 char nombre[255];
 int padre;
}__attribute__((packed)) t_directory;

typedef struct{
 int index;
 int socket;
 int cantidad;
 char* nombre;
}__attribute__((packed)) t_dtdisponible;
typedef struct{
 int tamanio;
 char*nombre_nodo;
 void*datos;
}__attribute__((packed)) t_cpblock;

typedef struct{
 int index_padre;
 int total_bloques;
 int tipo;
 char *nombre_archivo;
 pthread_mutex_t mutex;
}__attribute__((packed)) t_archivo_actual;

char* integer_to_string(char*string,int x);
size_t getFileSize(const char* filename);

#endif /* HELPER_*/
