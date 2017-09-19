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

typedef enum { ESHANDSHAKE, ESDATOS, ESSTRING, ESARCHIVO, ESINT, ESERROR, NUEVOWORKER} tipo;
typedef enum { FORMATEAR, REMOVER, RENOMBRAR, MOVER, MOSTRAR, CREARDIR, CPFROM, CPTO, CPBLOCK, MD5, LISTAR, INFO} accionesFilesystem;

typedef struct {
	tipo tipoMensaje;
	uint32_t tamPayload;
	char emisor[11];
}__attribute__((packed)) Header;

typedef struct {
	Header header;
	void* Payload;
}__attribute__((packed)) Paquete;

typedef struct {
	pthread_t hilo;
	int socket;
} structHilo;

void Servidor(char* ip, int puerto, char nombre[11],
		void (*accion)(Paquete* paquete, int socketFD),
		int (*RecibirPaquete)(int socketFD, char receptor[11], Paquete* paquete));
void ServidorConcurrente(char* ip, int puerto, char nombre[11], t_list** listaDeHilos,
		bool* terminar, void (*accionHilo)(void* socketFD));
void ServidorConcurrente(char* ip, int puerto, char nombre[11], t_list** listaDeProcesos,
		bool* terminar, void (*accionHilo)(void* socketFD));
int StartServidor(char* MyIP, int MyPort);
int ConectarAServidor(int puertoAConectar, char* ipAConectar, char servidor[11], char cliente[11],
		void RecibirElHandshake(int socketFD, char emisor[11])); //Sobrecarga para Kernel

void EnviarDatos(int socketFD, char emisor[11], void* datos, int tamDatos);
void EnviarDatosTipo(int socketFD, char emisor[11], void* datos, int tamDatos, tipo tipoMensaje);
void EnviarMensaje(int socketFD, char* msg, char emisor[11]);
void EnviarPaquete(int socketCliente, Paquete* paquete);
void RecibirHandshake(int socketFD, char emisor[11]);
int RecibirPaqueteServidor(int socketFD, char receptor[11], Paquete* paquete); //Responde al recibir un Handshake
int RecibirPaqueteCliente(int socketFD, char receptor[11], Paquete* paquete); //No responde los Handshakes

#endif //SOCKETS_H_