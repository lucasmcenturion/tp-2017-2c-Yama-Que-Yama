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
#include <dirent.h>
#include <ctype.h>
#include <semaphore.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>

#define true 1
#define false 0
#define NULL ((void *) 0)

typedef struct{
	char* ip;
	int puerto;
}
__attribute__((packed))
datosWorker;

typedef enum { ALMACENARARCHIVO, LEERARCHIVO} interfazFilesystem;

typedef struct {
	datosWorker worker;

}__attribute__((packed))
transformacionDatos;

char* integer_to_string(int x);
size_t getFileSize(const char* filename);

#endif /* HELPER_*/
