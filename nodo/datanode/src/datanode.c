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

void obtenerValoresArchivoConfiguracion(char* valor) {
	char* a = string_from_format("/home/utnso/workspace/tp-2017-2c-Yama-Que-Yama/nodo/nodo%sCFG.txt", valor);
	t_config* arch = config_create(a);
	//t_config* arch = config_create("/home/utnso/workspace/tp-2017-2c-Yama-Que-Yama/nodo/nodoCFG.txt");//"/home/utnso/workspace/tp-2017-2c-Yama-Que-Yama/nodo/nodoCFG.txt");
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


void* getBloque(int numeroDeBloque,int tamanio){
	int unFileDescriptor;
	void * punteroDataBin;
	void *datos_a_enviar=malloc(tamanio);
	if ((unFileDescriptor = open(RUTA_DATABIN, O_RDONLY)) == -1) {
		printf("ERROR en el Open() de getBloque()");
	}else{
		if ((punteroDataBin = mmap ( 0 , tamanioDataBin , PROT_READ , MAP_SHARED, unFileDescriptor , numeroDeBloque*TAMBLOQUE)) == (caddr_t)(-1)){
			printf("ERROR en el mmap() de getBloque()");
		}else{
				  memmove(datos_a_enviar,punteroDataBin,tamanio);
		}
	}
	int unmap=munmap(punteroDataBin,tamanioDataBin);
	if(unmap==-1){
		printf("Error en munmap de getBloque\n");
		fflush(stdout);
	}else{
		return datos_a_enviar;
	}

}

int setBloque ( int numeroDeBloque, void * datosParaGrabar,int tamano ){
	int unFileDescriptor;
	void * punteroDataBin;
	bool error=false;

    unFileDescriptor = open(RUTA_DATABIN,O_RDWR);
	if (unFileDescriptor == -1) {
		printf("Error al abrir el archivo\n");
		fflush(stdout);
		close(unFileDescriptor);
		error=true;
	}else{
		punteroDataBin = mmap(NULL, tamanioDataBin, PROT_WRITE | PROT_READ, MAP_SHARED, unFileDescriptor, numeroDeBloque*TAMBLOQUE);
		if (punteroDataBin == MAP_FAILED) {
					printf("Error al mapear a memoria: %s\n", strerror(errno));
					fflush(stdout);
					error=true;
		}else{
			memmove(punteroDataBin,datosParaGrabar,tamano);
		}
		int unmap=munmap(punteroDataBin,tamanioDataBin);
		if(unmap==-1){
			printf("Error en munmap de setBloque\n");
			fflush(stdout);
			error=true;
		}
		int resultado= !error ? 1 : -1;   // 1 resultado OK, -1 resultado ERRONEO
		return resultado;
	}
}


void escribirEnArchivoLog(char * contenidoAEscribir, FILE ** archivoDeLog){
	fseek ( *archivoDeLog , 0 , SEEK_END );  // grabo los datos mas recientes al final del archivo.
	fwrite ( contenidoAEscribir , strlen(contenidoAEscribir) , 1 , *archivoDeLog );
	fwrite ( "\n" , 1 , 1 , *archivoDeLog );
	printf("%s\n", contenidoAEscribir);
	fflush(stdout);
}


//   escribirEnArchivoLog(operacion,&LogDatanode);

void enviarHandshakeDatanode(int socketFD,char emisor[11]){
	Paquete* paquete = malloc(TAMANIOHEADER+strlen(NOMBRE_NODO)+1);
	Header header;
	header.tipoMensaje = ESHANDSHAKE;
	header.tamPayload = strlen(NOMBRE_NODO)+1;
	paquete->Payload=malloc(strlen(NOMBRE_NODO)+1);
	strcpy(paquete->Payload,NOMBRE_NODO);
	strcpy(header.emisor, emisor);
	paquete->header = header;
	bool valor_retorno=EnviarPaquete(socketFD, paquete);
	//free(paquete->Payload);
	free(paquete);
}
int main(int argc, char* argv[]){
	obtenerValoresArchivoConfiguracion(argv[1]);
	imprimirArchivoConfiguracion();
	fflush( stdout );
	obtenerStatusDataBin();
	fflush( stdout );
	escribir_log("datanode","dt","Iniciando..","info");
	socketFS = ConectarAServidorDatanode(PUERTO_FILESYSTEM, IP_FILESYSTEM, FILESYSTEM, DATANODE, RecibirHandshake,enviarHandshakeDatanode);
	fflush( stdout );

	//escribirEnArchivoLog("operacion",&LogDatanode);
	int tamanio = sizeof(uint32_t) * 2  +  sizeof(char) * strlen(IP_NODO) +1+sizeof(char) * strlen(NOMBRE_NODO) + 1;
	void*datos = malloc(tamanio);
	*((uint32_t*)datos) = tamanioDataBin;
	datos += sizeof(uint32_t);
	*((uint32_t*)datos) = PUERTO_WORKER;
	datos += sizeof(uint32_t);
	strcpy(datos, IP_NODO);
	datos += strlen(IP_NODO) + 1;
	strcpy(datos, NOMBRE_NODO);
	datos +=  strlen(NOMBRE_NODO) + 1;
	datos -= tamanio;

	EnviarDatosTipo(socketFS, DATANODE, datos, tamanio, IDENTIFICACIONDATANODE);

	escribir_log("datanode","dt","Identificacion enviada a fs","info");
	free(datos);
	Paquete paquete;
	void *datos_solicitud;
	int bloque_archivo;
	while (RecibirPaqueteCliente(socketFS, FILESYSTEM, &paquete)>0){
		switch(paquete.header.tipoMensaje){
		case SOLICITUDBLOQUE:
		{
			escribir_log("datanode","dt","Se solicito un bloque ","info");
			datos_solicitud=paquete.Payload;
			bloque_archivo = *((uint32_t*)datos_solicitud);
			datos_solicitud+=sizeof(uint32_t);
			int bloque_nodo = *((uint32_t*)datos_solicitud);
			datos_solicitud+=sizeof(uint32_t);
			int bloques_totales=*((uint32_t*)datos_solicitud);
			datos_solicitud+=sizeof(uint32_t);
			int index_directorio=*((uint32_t*)datos_solicitud);
			datos_solicitud+=sizeof(uint32_t);
			int tamanio_bloque=*((uint32_t*)datos_solicitud);
			datos_solicitud+=sizeof(uint32_t);
			int tamanio_archivo=*((uint32_t*)datos_solicitud);
			datos_solicitud+=sizeof(uint32_t);
			char*nombre_archivo=malloc(100);
			strcpy(nombre_archivo,datos_solicitud);
			nombre_archivo=realloc(nombre_archivo,strlen(nombre_archivo)+1);
			datos_solicitud+=strlen(nombre_archivo)+1;

			void *bloque=getBloque(bloque_nodo,tamanio_bloque);
			//bloque=getBloque(bloque_nodo,tamanio_bloque);
			//memmove(bloque,getBloque(bloque_nodo,tamanio_bloque),tamanio_bloque);
			//printf((char*)bloque);
			escribir_log("datanode","dt","Bloque obtenido","info");
			int tamanio_a_enviar = sizeof(uint32_t) * 5  + sizeof(char)*strlen(nombre_archivo)+1+tamanio_bloque;

			datos_solicitud = calloc(1,tamanio_a_enviar);

			*((uint32_t*)datos_solicitud) = bloque_archivo;
			datos_solicitud += sizeof(uint32_t);
			*((uint32_t*)datos_solicitud) = bloques_totales;
			datos_solicitud += sizeof(uint32_t);
			*((uint32_t*)datos_solicitud) = index_directorio;
			datos_solicitud += sizeof(uint32_t);
			*((uint32_t*)datos_solicitud) = tamanio_bloque;
			datos_solicitud += sizeof(uint32_t);
			*((uint32_t*)datos_solicitud) = tamanio_archivo;
			datos_solicitud += sizeof(uint32_t);
			strcpy(datos_solicitud, nombre_archivo);
			datos_solicitud +=  strlen(nombre_archivo) + 1;
			memmove(datos_solicitud,bloque,tamanio_bloque);
			datos_solicitud+=tamanio_bloque;
			datos_solicitud -= tamanio_a_enviar;

			EnviarDatosTipo(socketFS,DATANODE,datos_solicitud,tamanio_a_enviar,RESPUESTASOLICITUD);
			escribir_log("datanode","dt","Bloque enviado","info");
			free(bloque);

		}
		break;
		case SOLICITUDBLOQUECPTO:{

			escribir_log("datanode","dt","Solicitaron bloque cpto","info");
			datos_solicitud=paquete.Payload;
			bloque_archivo = *((uint32_t*)datos_solicitud);
			datos_solicitud+=sizeof(uint32_t);
			int bloque_nodo = *((uint32_t*)datos_solicitud);
			datos_solicitud+=sizeof(uint32_t);
			int bloques_totales=*((uint32_t*)datos_solicitud);
			datos_solicitud+=sizeof(uint32_t);
			int index_directorio=*((uint32_t*)datos_solicitud);
			datos_solicitud+=sizeof(uint32_t);
			int tamanio_bloque=*((uint32_t*)datos_solicitud);
			datos_solicitud+=sizeof(uint32_t);
			int tamanio_archivo=*((uint32_t*)datos_solicitud);
			datos_solicitud+=sizeof(uint32_t);
			char*nombre_archivo=malloc(100);
			strcpy(nombre_archivo,datos_solicitud);
			nombre_archivo=realloc(nombre_archivo,strlen(nombre_archivo)+1);
			datos_solicitud+=strlen(nombre_archivo)+1;
			char*ruta_a_guardar=malloc(100);
			strcpy(ruta_a_guardar,datos_solicitud);
			ruta_a_guardar=realloc(ruta_a_guardar,strlen(ruta_a_guardar)+1);
			datos_solicitud+=strlen(ruta_a_guardar)+1;
			void *bloque=getBloque(bloque_nodo,tamanio_bloque);
			escribir_log("datanode","dt","Bloque obtenido","info");
			int tamanio_a_enviar = sizeof(uint32_t) * 5  + sizeof(char)*strlen(nombre_archivo)+1
					+sizeof(char)*strlen(ruta_a_guardar)+1+tamanio_bloque;

			datos_solicitud = calloc(1,tamanio_a_enviar);

			*((uint32_t*)datos_solicitud) = bloque_archivo;
			datos_solicitud += sizeof(uint32_t);
			*((uint32_t*)datos_solicitud) = bloques_totales;
			datos_solicitud += sizeof(uint32_t);
			*((uint32_t*)datos_solicitud) = index_directorio;
			datos_solicitud += sizeof(uint32_t);
			*((uint32_t*)datos_solicitud) = tamanio_bloque;
			datos_solicitud += sizeof(uint32_t);
			*((uint32_t*)datos_solicitud) = tamanio_archivo;
			datos_solicitud += sizeof(uint32_t);
			strcpy(datos_solicitud, nombre_archivo);
			datos_solicitud +=  strlen(nombre_archivo) + 1;
			strcpy(datos_solicitud,ruta_a_guardar);
			datos_solicitud+=strlen(ruta_a_guardar)+1;
			memmove(datos_solicitud,bloque,tamanio_bloque);
			datos_solicitud+=tamanio_bloque;
			datos_solicitud -= tamanio_a_enviar;

			EnviarDatosTipo(socketFS,DATANODE,datos_solicitud,tamanio_a_enviar,RESPUESTASOLICITUDCPTO);
			escribir_log("datanode","dt","IBloque enviado","info");
			free(bloque);

		}
		break;
		case GETBLOQUE:
		{
			datos_solicitud=paquete.Payload;
			escribir_log("datanode","dt","Get bloque","info");
			int tamanio_bloque= *((uint32_t*)datos_solicitud);
			datos_solicitud+=sizeof(uint32_t);
			int numero_bloque=*((uint32_t*)datos_solicitud);
			datos_solicitud+=sizeof(uint32_t);
			char*nombre_nodo_a_setear=malloc(100);
			strcpy(nombre_nodo_a_setear,datos_solicitud);
			datos_solicitud+=strlen(nombre_nodo_a_setear)+1;
			void*bloque=malloc(tamanio_bloque);
			bloque=getBloque(numero_bloque,tamanio_bloque);
			escribir_log("datanode","dt","Bloque obtenido","info");
			int tamanio_a_enviar=sizeof(uint32_t)+strlen(nombre_nodo_a_setear)+1+tamanio_bloque;
			void* datos2=malloc(tamanio_a_enviar);
			*((uint32_t*)datos2)=tamanio_bloque;
			datos2+=sizeof(uint32_t);
			strcpy(datos2,nombre_nodo_a_setear);
			datos2+=strlen(nombre_nodo_a_setear)+1;
			memmove(datos2,bloque,tamanio_bloque);
			datos2+=tamanio_bloque;
			datos2-=tamanio_a_enviar;

			EnviarDatosTipo(socketFS,DATANODE,datos2,tamanio_a_enviar,RESPUESTAGETBLOQUE);
			escribir_log("datanode","dt","Bloque enviado","info");
			free(bloque);
			//free(datos2);
		}
		break;
		case SETBLOQUE:
		{


			//recibimos los datos

			escribir_log("datanode","dt","Set bloque","info");
			void *datos;
			datos=paquete.Payload;
			int numero = *((uint32_t*)datos);
			datos+=sizeof(uint32_t);
			int copia = *((uint32_t*)datos);
			datos+=sizeof(uint32_t);
			int tamano=*((uint32_t*)datos);
			datos+=sizeof(uint32_t);
			bloque_archivo=*((uint32_t*)datos);
			datos+=sizeof(uint32_t);
			void *bloque=calloc(1,tamano);
			memmove(bloque,datos,tamano);
			datos+=tamano;
			char *nombre_archivo=malloc(100);
			strcpy(nombre_archivo,datos);
			nombre_archivo=realloc(nombre_archivo,strlen(nombre_archivo)+1);
			datos+=strlen(nombre_archivo);
			datos-=tamanio;
			int resultado=setBloque(numero,bloque,tamano);
			escribir_log("datanode","dt","Bloque seteado","info");
			//enviamos respuesta a FS

			int tamanio = sizeof(uint32_t) * 5  + sizeof(char)*strlen(NOMBRE_NODO) +1+ sizeof(char)*strlen(nombre_archivo)+1;
			datos = malloc(tamanio);
			*((uint32_t*)datos) = numero;
			datos += sizeof(uint32_t);
			*((uint32_t*)datos) =copia;
			datos += sizeof(uint32_t);
			*((uint32_t*)datos) = resultado;
			datos += sizeof(uint32_t);
			*((uint32_t*)datos) = tamano;
			datos += sizeof(uint32_t);
			*((uint32_t*)datos) = bloque_archivo;
			datos += sizeof(uint32_t);
			strcpy(datos,NOMBRE_NODO);
			datos += strlen(NOMBRE_NODO)+1;
			strcpy(datos,nombre_archivo);
			datos += strlen(nombre_archivo)+1;
			datos -= tamanio;

			EnviarDatosTipo(socketFS, DATANODE, datos, tamanio, RESULOPERACION);
			escribir_log("datanode","dt","Resultado de bloque seteado enviado","info");
			free(datos);
			free(bloque);
		}
		break;
		case SETBLOQUECPTO:{
			datos_solicitud=paquete.Payload;

			escribir_log("datanode","dt","Set bloque extra","info");
			bloque_archivo=*((uint32_t*)datos_solicitud);
			datos_solicitud+=sizeof(uint32_t);
			int bloque_a_setear=*((uint32_t*)datos_solicitud);
			datos_solicitud+=sizeof(uint32_t);
			int index_padre=*((uint32_t*)datos_solicitud);
			datos_solicitud+=sizeof(uint32_t);
			int tamanio_bloque=*((uint32_t*)datos_solicitud);
			datos_solicitud+=sizeof(uint32_t);
			char*nombre_archivo=malloc(100);
			strcpy(nombre_archivo,datos_solicitud);
			datos_solicitud+=strlen(nombre_archivo)+1;
			nombre_archivo=realloc(nombre_archivo,strlen(nombre_archivo)+1);
			void*bloque=malloc(tamanio_bloque);
			memmove(bloque,datos_solicitud,tamanio_bloque);
			datos_solicitud+=tamanio_bloque;
			int resultado=setBloque(bloque_a_setear,bloque,tamanio_bloque);
			escribir_log("datanode","dt","Bloque extra seteado","info");
			int tamanio_a_enviar=4*sizeof(uint32_t)+strlen(nombre_archivo)+1+strlen(NOMBRE_NODO)+1;
			void*datos_a_enviar=malloc(tamanio_a_enviar);
			*((uint32_t*)datos_a_enviar) = bloque_archivo;
			datos_a_enviar += sizeof(uint32_t);
			*((uint32_t*)datos_a_enviar) = bloque_a_setear;
			datos_a_enviar += sizeof(uint32_t);
			*((uint32_t*)datos_a_enviar) = index_padre;
			datos_a_enviar += sizeof(uint32_t);
			*((uint32_t*)datos_a_enviar) = resultado;
			datos_a_enviar += sizeof(uint32_t);
			strcpy(datos_a_enviar,nombre_archivo);
			datos_a_enviar+=strlen(nombre_archivo)+1;
			strcpy(datos_a_enviar,NOMBRE_NODO);
			datos_a_enviar+=strlen(NOMBRE_NODO)+1;
			datos_a_enviar-=tamanio_a_enviar;
			EnviarDatosTipo(socketFS,DATANODE,datos_a_enviar,tamanio_a_enviar,RESPUESTASETBLOQUECPTO);
			escribir_log("datanode","dt","Resultado bloque extra enviado","info");
			free(datos_a_enviar);
			free(bloque);
		}
		break;
		}
		if (paquete.Payload != NULL){
			free(paquete.Payload);
		}
	}
	return 0;
}
