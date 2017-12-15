#ifndef SOCKETS_H_
#define SOCKETS_H_

#include "helper.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>

#define BACKLOG 10 // Cuántas conexiones pendientes se mantienen en cola del server
#define TAMANIOHEADER sizeof(Header)
#define STRHANDSHAKE "10"
//Emisores:
#define MASTER "Master    "
#define WORKER "Worker    "
#define FILESYSTEM "FileSystem"
#define DATANODE "Datanode  "
#define YAMA "YAMA      "

typedef enum { ESHANDSHAKE, ESDATOS, ESSTRING, ESARCHIVO, ESINT, ESERROR, NUEVOWORKER, TRANSFWORKER, VALIDACIONWORKER,
	REDLOCALWORKER, REDGLOBALWORKER, VALIDACIONYAMA,ESNOMBRE,IDENTIFICACIONDATANODE,ESBLOQUE,GETBLOQUE,SETBLOQUE,
	RESULOPERACION,BLOQUEOBTENIDO, SOLICITUDBLOQUE , RESPUESTASOLICITUD, NODODESCONECTADO, SOLICITUDBLOQUESYAMA,
	SOLICITUDTRANSFORMACION, SOLICITUDREDUCCIONLOCAL, SOLICITUDREDUCCIONGLOBAL, SOLICITUDALMACENADOFINAL, ARCHIVOTEMPRL,FINARCHIVOTEMPRL,LISTAWORKERS,
	FINTRANSFORMACION, FINREDUCCIONLOCAL, FINREDUCCIONGLOBAL, FINALMACENADOFINAL, SOLICITUDBLOQUECPTO,RESPUESTASOLICITUDCPTO,
	RESPUESTAGETBLOQUE,SETBLOQUECPTO,RESPUESTASETBLOQUECPTO} tipo;

typedef enum { FORMATEAR, REMOVER, RENOMBRAR, MOVER, MOSTRAR, CREARDIR, CPFROM, CPTO, CPBLOCK, MD5, LISTAR, INFO} accionesFilesystem;

typedef struct{
	etapa header;
	datosWorker worker;
	int bloque;
	char* archivoTemporal;
	char* programa;
	int cantidadDeBytesOcupados;
	char** ListaArchivosTemporales;
	int cantArchTempRL;
	datosWorker workerEncargado;
}__attribute__((packed)) solicitudPrograma;

typedef struct{
	int bloque;
	int idJob;//jXnYbZesX;
	char* nodo;
	bool exito;
}__attribute__((packed)) rtaEstado;

typedef struct{
	etapa header; //No se si es necesaria, ahora lo chequeo.
	datosWorker worker;
	char* archTempRL;
	bool encargado;
}__attribute__((packed)) nodoRG;

typedef struct{
	etapa header;
	t_list* nodos;
	char* archRG; //Archivo destino
	char* programaR;
	int cantNodos;
}__attribute__((packed)) solicitudRG;

typedef struct{
	etapa header;
	datosWorker worker;
	int bloque;
	int bytesOcupados;
	char* archivoTemporal;
	char* programaT;
}__attribute__((packed)) nodoT; //Recibo/Envias una lista de nodos (o array)

typedef struct{
	etapa header;
	datosWorker worker;
	t_list* listaArchivosTemporales;
	char* archivoTemporal; //Destino
	char* programaR;
	int idJob;
}__attribute__((packed)) nodoRL; //Recibo/Envias una lista de nodos (o array)


typedef struct {
	tipo tipoMensaje;
	uint32_t tamPayload;
	char emisor[11];
}__attribute__((packed)) Header;


typedef struct {
	int bloques_totales;
	int bloque_nodo;
	int bloque_archivo;
	int tamanio_bloque;
	int index_directorio;
	char* nombre_archivo;
	char* nombre_nodo;
}__attribute__((packed)) t_solicitud_bloque;

typedef struct {
	Header header;
	void* Payload;
}__attribute__((packed)) Paquete;

typedef struct {
	pthread_t hilo;
	int socket;
} structHilo;

typedef struct {
	pid_t pid;
	int socket;
} structProceso;

void Servidor(char* ip, int puerto, char nombre[11],
		void (*accion)(Paquete* paquete, int socketFD),
		int (*RecibirPaquete)(int socketFD, char receptor[11], Paquete* paquete));
void ServidorConcurrente(char* ip, int puerto, char nombre[11], t_list** listaDeHilos,
		bool* terminar, void (*accionHilo)(void* socketFD));
void ServidorConcurrenteForks(char* ip, int puerto, char nombre[11], t_list** listaDeProcesos,
		bool* terminar, void (*accionPadre)(void* socketFD), void (*accionHijo)(void* socketFD));
int StartServidor(char* MyIP, int MyPort);
int ConectarAServidor(int puertoAConectar, char* ipAConectar, char servidor[11], char cliente[11],
		void RecibirElHandshake(int socketFD, char emisor[11])); //Sobrecarga para Kernel

bool EnviarDatos(int socketFD, char emisor[11], void* datos, int tamDatos);
bool EnviarDatosTipo(int socketFD, char emisor[11], void* datos, int tamDatos, tipo tipoMensaje);
bool EnviarMensaje(int socketFD, char* msg, char emisor[11]);
bool EnviarPaquete(int socketCliente, Paquete* paquete);
void RecibirHandshake(int socketFD, char emisor[11]);
int RecibirPaqueteServidor(int socketFD, char receptor[11], Paquete* paquete); //Responde al recibir un Handshake
int RecibirPaqueteCliente(int socketFD, char receptor[11], Paquete* paquete); //No responde los Handshakes

#endif //SOCKETS_H_
