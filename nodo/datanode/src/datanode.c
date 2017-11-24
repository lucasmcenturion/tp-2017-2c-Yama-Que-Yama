#include "sockets.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> //Para usar Stat, entre otras.
#include <sys/stat.h>  //Para obtener propiedades del databin.
#include <time.h>      //Para mostrar datos de databin.
#include <sys/mman.h>  //Para mmap.
#define TAMBLOQUE (1024*1024)


char *IP_FILESYSTEM,*NOMBRE_NODO,*IP_NODO,*RUTA_DATABIN;
int PUERTO_FILESYSTEM, PUERTO_WORKER;
int socketFS;
int tamanioDataBin;

FILE *LogDatanode;

void obtenerValoresArchivoConfiguracion() {
	t_config* arch = config_create("/home/utnso/workspace/tp-2017-2c-Yama-Que-Yama/nodo/nodoCFG.txt");
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


void getBloque(int numeroDeBloque){
	int unFileDescriptor;
	void * punteroDataBin;
	punteroDataBin= (void *) malloc (TAMBLOQUE);

	if ((unFileDescriptor = open(RUTA_DATABIN, O_RDONLY)) == -1) {
		printf("ERROR en el Open() de getBloque()");
	}
	if ((punteroDataBin = mmap ( 0 , tamanioDataBin , PROT_READ , MAP_SHARED, unFileDescriptor , numeroDeBloque*TAMBLOQUE)) == (caddr_t)(-1)){
		printf("ERROR en el mmap() de getBloque()");
	}else{
			  void *datos_a_enviar=calloc(1,TAMBLOQUE);
			  memmove(datos_a_enviar,punteroDataBin,TAMBLOQUE);
			  EnviarDatosTipo(socketFS, DATANODE, datos_a_enviar, TAMBLOQUE, BLOQUEOBTENIDO);

			  printf("funco bien mmap de getbloque\n");
			  fflush(stdout);
			  printf("%s\n",(char*)datos_a_enviar);
			  fflush(stdout);
			  free(datos_a_enviar);
	}
	int unmap=munmap(punteroDataBin,tamanioDataBin);
	if(unmap==-1){
		printf("Error en munmap de getBloque\n");
		fflush(stdout);
		free(punteroDataBin);
	}else free(punteroDataBin);

}

int setBloque ( int numeroDeBloque, void * datosParaGrabar ){
	int unFileDescriptor;
	void * punteroDataBin;
	bool error=false;

    unFileDescriptor = open(RUTA_DATABIN,O_RDWR);
	if (unFileDescriptor == -1) {
		printf("Error al abrir el archivo\n");
		fflush(stdout);
		close(unFileDescriptor);
		error=true;
	}
	punteroDataBin = mmap(NULL, tamanioDataBin, PROT_WRITE | PROT_READ, MAP_SHARED, unFileDescriptor, 0);
	if (punteroDataBin == MAP_FAILED) {
				printf("Error al mapear a memoria: %s\n", strerror(errno));
				fflush(stdout);
				error=true;
	}else memmove(punteroDataBin,datosParaGrabar,TAMBLOQUE);

	int unmap=munmap(punteroDataBin,tamanioDataBin);
	if(unmap==-1){
		printf("Error en munmap de setBloque\n");
		fflush(stdout);
		error=true;
	}else{

	}

	int resultado= !error ? 1 : -1;   // 1 resultado OK, -1 resultado ERRONEO
	return resultado;
}


void escribirEnArchivoLog(char * contenidoAEscribir, FILE ** archivoDeLog){
	fseek ( *archivoDeLog , 0 , SEEK_END );  // grabo los datos mas recientes al final del archivo.
	fwrite ( contenidoAEscribir , strlen(contenidoAEscribir) , 1 , *archivoDeLog );
	fwrite ( "\n" , 1 , 1 , *archivoDeLog );
	printf("%s\n", contenidoAEscribir);
	fflush(stdout);
}


//   escribirEnArchivoLog(operacion,&LogDatanode);


int main(){


	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();
	fflush( stdout );
	obtenerStatusDataBin();
	fflush( stdout );
	socketFS = ConectarAServidor(PUERTO_FILESYSTEM, IP_FILESYSTEM, FILESYSTEM, DATANODE, RecibirHandshake);
	fflush( stdout );

	int tamanio = sizeof(uint32_t) * 5 + sizeof(char) * strlen(IP_NODO) + sizeof(char) * strlen(NOMBRE_NODO) + 2;
	void* datos = malloc(tamanio);
	*((uint32_t*)datos) = strlen(NOMBRE_NODO);
	datos += sizeof(uint32_t);
	strcpy(datos, NOMBRE_NODO);
	datos +=  strlen(NOMBRE_NODO) + 1;
	*((uint32_t*)datos) = PUERTO_WORKER;
	datos += sizeof(uint32_t);
	*((uint32_t*)datos) = strlen(IP_NODO);
	datos += sizeof(uint32_t);
	strcpy(datos, IP_NODO);
	datos += strlen(IP_NODO) + 1;
	*((uint32_t*)datos) = 0;
	datos += sizeof(uint32_t);
	*((uint32_t*)datos) = 0;
	datos += sizeof(uint32_t);
	datos -= tamanio;

	EnviarDatosTipo(socketFS, DATANODE, datos, tamanio, NUEVOWORKER);

	//getBloque(3);
	//setBloque(2,"1");
	//escribirEnArchivoLog("operacion",&LogDatanode);
	tamanio = sizeof(uint32_t) * 1  + sizeof(char) * strlen(NOMBRE_NODO) + 1;
	datos = malloc(tamanio);
	*((uint32_t*)datos) = tamanioDataBin;
	datos += sizeof(uint32_t);
	strcpy(datos, NOMBRE_NODO);
	datos +=  strlen(NOMBRE_NODO) + 1;
	datos -= tamanio;
	EnviarDatosTipo(socketFS, DATANODE, datos, tamanio, IDENTIFICACIONDATANODE);
	free(datos);
	fflush( stdout );
	printf("Me voy a quedar lupeando chiks\n");
	fflush( stdout );
	Paquete paquete;
	while (RecibirPaqueteCliente(socketFS, FILESYSTEM, &paquete)>0){
		switch(paquete.header.tipoMensaje){
		case GETBLOQUE:

			break;
		case SETBLOQUE:
			{


			//recibimos los datos


			void *datos;
			datos=paquete.Payload;
			int numero = *((uint32_t*)datos);
			datos+=sizeof(uint32_t);
			int copia = *((uint32_t*)datos);
			datos+=sizeof(uint32_t);
			int tamano=*((uint32_t*)datos);
			datos+=sizeof(uint32_t);
			void *bloque=calloc(1,tamano);
			memmove(bloque,datos,tamano);
			datos+=tamano;
			char *nombre_archivo=malloc(100);
			strcpy(nombre_archivo,datos);
			nombre_archivo=realloc(nombre_archivo,strlen(nombre_archivo)+1);
			datos+=strlen(nombre_archivo);
			int resultado=setBloque(numero,bloque);

			//enviamos respuesta a FS

			int tamanio = sizeof(uint32_t) * 4  + sizeof(char)*strlen(NOMBRE_NODO) +1+ sizeof(char)*strlen(nombre_archivo)+1;
			void *datos = malloc(tamanio);
			*((uint32_t*)datos) = numero;
			datos += sizeof(uint32_t);
			*((uint32_t*)datos) =copia;
			datos += sizeof(uint32_t);
			*((uint32_t*)datos) = resultado;
			datos += sizeof(uint32_t);
			*((uint32_t*)datos) = tamano;
			datos += sizeof(uint32_t);
			strcpy(datos,NOMBRE_NODO,strlen(NOMBRE_NODO)+1);
			datos += strlen(NOMBRE_NODO)+1;
			strcpy(datos,nombre_archivo,strlen(nombre_archivo)+1);
			datos += strlen(nombre_archivo)+1;
			datos -= tamanio;

			EnviarDatosTipo(socketFS, DATANODE, datos, tamanio, RESULOPERACION);

			break;
			}
		}
		if (paquete.Payload != NULL){
			free(paquete.Payload);
		}
	}
	return 0;
}
