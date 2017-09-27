#include "sockets.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> //Para usar Stat, entre otras.
#include <sys/stat.h>  //Para obtener propiedades del databin.
#include <time.h>      //Para mostrar datos de databin.
#include <sys/mman.h>  //Para mmap.


char *IP_FILESYSTEM,*NOMBRE_NODO,*IP_NODO,*RUTA_DATABIN;
int PUERTO_FILESYSTEM, PUERTO_WORKER;
int socketFS;
int tamanioDataBin;

void obtenerValoresArchivoConfiguracion() {
	t_config* arch = config_create("../nodoCFG.txt");
	IP_FILESYSTEM = string_duplicate(config_get_string_value(arch, "IP_FILESYSTEM"));
	PUERTO_FILESYSTEM = config_get_int_value(arch, "PUERTO_FILESYSTEM");
	NOMBRE_NODO = string_duplicate(config_get_string_value(arch, "NOMBRE_NODO"));
	IP_NODO = string_duplicate(config_get_string_value(arch, "IP_NODO"));
	PUERTO_WORKER = config_get_int_value(arch, "PUERTO_WORKER");
	RUTA_DATABIN = string_duplicate(config_get_string_value(arch, "RUTA_DATABIN"));
	config_destroy(arch);}

void imprimirArchivoConfiguracion(){
	printf(
				"-- CONFIGURACIÓN --\n"
				"IP_FILESYSTEM=		%s\n"
				"PUERTO_FILESYSTEM=	%d\n"
				"NOMBRE_NODO=		%s\n"
				"IP_NODO=		%s\n"
				"PUERTO_WORKER=		%d\n"
				"RUTA_DATABIN=		%s\n\n",
				IP_FILESYSTEM,
				PUERTO_FILESYSTEM,
				NOMBRE_NODO,
				IP_NODO,
				PUERTO_WORKER,
				RUTA_DATABIN
				);}

void obtenerStatusDataBin(){
	struct stat st;
    if(stat(RUTA_DATABIN, &st) == -1) printf("Error en Stat()\n");
    else {tamanioDataBin = st.st_size;
    	printf("-- STATUS DEL DATA.BIN --\n");
    	printf("Tamaño:			%i bytes\n", tamanioDataBin);
		printf("Último acceso:		%s", ctime(&st.st_atime));
    	printf("Última modificación:	%s\n\n", ctime(&st.st_mtime));
		}
}

	///////////////////////////////////////////////////  MAN  DE  MMAP  /////////////////////////////////////////////////////////

	//void *mmap(void *addr, size_t len, int prot,int flags, int fildes, off_t off);

	//addr	Is the address we want the file mapped into. The best way to use this is to set it to (caddr_t)0 and let the OS choose it for you.
	//		If you tell it to use an address the OS doesn't like (for instance, if it's not a multiple of the virtual memory page size), it'll give you an error.

	//len	This parameter is the length of the data we want to map into memory. This can be any length you want.
	//		(Aside: if len not a multiple of the virtual memory page size, you will get a blocksize that is rounded up to that size.
	//		The extra bytes will be 0, and any changes you make to them will not modify the file.)

	//prot	The "protection" argument allows you to specify what kind of access this process has to the memory mapped region.
	//	 	This can be a bitwise-ORd mixture of the following values: PROT_READ, PROT_WRITE, and PROT_EXEC, for read, write, and execute permissions, respectively.
	//		The value specified here must be equivalent to the mode specified in the open() system call that is used to get the file descriptor.

	//flags	There are just miscellaneous flags that can be set for the system call.
	//		You'll want to set it to MAP_SHARED if you're planning to share your changes to the file with other processes, or MAP_PRIVATE otherwise.
	//		If you set it to the latter, your process will get a copy of the mapped region,
	//		so any changes you make to it will not be reflected in the original file—thus, other processes will not be able to see them.

	//fildes	This is where you put that file descriptor you opened earlier.

	//off	This is the offset in the file that you want to start mapping from.
	//	 	A restriction: this must be a multiple of the virtual memory page size. This page size can be obtained with a call to getpagesize().
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void getBloque(int numeroDeBloque){ 	//getBloque(numero): Devolverá el contenido del bloque solicitado almacenado en el espacio de Datos.
	int unFileDescriptor;
	int tamanioAleer = tamanioDataBin/20;	// Voy a leer un sólo bloque de 1MB
	char * datos;                                                               //HACER MALLOC. no calloc

	if ((unFileDescriptor = open(RUTA_DATABIN, O_RDONLY)) == -1) {		// Obtengo el fd del data.bin
		printf("ERROR en Open() al obtener el FileDescriptor de data.bin");}

	if ((datos = mmap ( (caddr_t)0 , tamanioAleer , PROT_READ , MAP_SHARED, unFileDescriptor , tamanioAleer )) == (caddr_t)(-1)) {
		printf("ERROR en mmap()");}

}




int main(){
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();
	obtenerStatusDataBin();
	socketFS = ConectarAServidor(PUERTO_FILESYSTEM, IP_FILESYSTEM, FILESYSTEM, DATANODE, RecibirHandshake);
	datosWorker* datos = malloc(sizeof(datosWorker));
	datos->ip = IP_NODO;
	datos->puerto = PUERTO_WORKER;
	EnviarDatosTipo(socketFS, DATANODE, datos, sizeof(datosWorker), NUEVOWORKER);

	// ● setBloque(numero, [datos]): Grabará los datos enviados en el bloque solicitado del espacio de Datos.

	/*Para simplificar el acceso a los datos de forma aleatoria el alumno debe investigar e
	implementar la llamada al sistema mmap()​*/

	/*Archivo​ ​de​ ​Log
		El proceso DataNode deberá contar con un archivo de configuración en el cual se logearán
		todas las operaciones realizadas. Las mismas deberán ser mostradas por pantalla*/


	while(1){

	}

	return 0;
}
