#include "sockets.h"
#include <stdio.h>
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
				"Configuración:\n"
				"IP_FILESYSTEM=%s\n"
				"PUERTO_FILESYSTEM=%d\n"
				"NOMBRE_NODO=%s\n"
				"IP_NODO=%s\n"
				"PUERTO_WORKER=%d\n"
				"RUTA_DATABIN=%s\n",
				IP_FILESYSTEM,
				PUERTO_FILESYSTEM,
				NOMBRE_NODO,
				IP_NODO,
				PUERTO_WORKER,
				RUTA_DATABIN
				);}

void obtenerDatosDataBin(){
	struct stat st;
    if(stat(RUTA_DATABIN, &st) == -1) printf("Error en Stat()\n");
    else {tamanioDataBin = st.st_size;
    	printf("Tamaño DataBin: %i bytes\n", tamanioDataBin);
		printf("Último acceso: %s\n", ctime(&st.st_atime));
    	printf("Última modificación: %s\n", ctime(&st.st_mtime));
		}
}


int main(){
	//Al iniciar este proceso, leerá el archivo de configuración del nodo..
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();

	//abrirá el archivo data.bin..
	obtenerDatosDataBin();

	//deberá conectarse al proceso Filesystem..
	socketFS = ConectarAServidor(PUERTO_FILESYSTEM, IP_FILESYSTEM, FILESYSTEM, DATANODE, RecibirHandshake);

	//Deberá identificarse y quedarse esperando solicitudes del FS, por ejemplo:
	// ● getBloque(numero): Devolverá el contenido del bloque solicitado almacenado en el espacio de Datos.

	// ● setBloque(numero, [datos]): Grabará los datos enviados en el bloque solicitado del espacio de Datos.

	/*Para simplificar el acceso a los datos de forma aleatoria el alumno debe investigar e
	implementar la llamada al sistema mmap()​*/

	/*Archivo​ ​de​ ​Log
		El proceso DataNode deberá contar con un archivo de configuración en el cual se logearán
		todas las operaciones realizadas. Las mismas deberán ser mostradas por pantalla*/


	/*tamanioDataBin = (int)getFileSize(RUTA_DATABIN); */


	while(1){

	}

	return 0;
}
