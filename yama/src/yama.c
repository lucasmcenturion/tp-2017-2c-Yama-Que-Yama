#include "sockets.h"

char *IP, *FS_IP, *ALGORITMO_BALANCEO;
int PUERTO, FS_PUERTO, RETARDO_PLANIFICACION;
int socketFS;

void obtenerValoresArchivoConfiguracion() {
	t_config* arch = config_create("yamaCFG.txt");
	IP = string_duplicate(config_get_string_value(arch, "IP"));
	PUERTO = config_get_int_value(arch, "PUERTO");
	FS_IP = string_duplicate(config_get_string_value(arch, "FS_IP"));
	FS_PUERTO = config_get_int_value(arch, "FS_PUERTO");
	RETARDO_PLANIFICACION = config_get_int_value(arch, "RETARDO_PLANIFICACION");
	ALGORITMO_BALANCEO = string_duplicate(config_get_string_value(arch, "ALGORITMO_BALANCEO"));
	config_destroy(arch);
}

void accion(void* paquete, int socket){
}

void imprimirArchivoConfiguracion(){
	printf(
				"Configuración:\n"
				"IP=%s\n"
				"PUERTO=%d\n"
				"FS_IP=%s\n"
				"FS_PUERTO=%d\n"
				"RETARDO_PLANIFICACION=%d\n"
				"ALGORITMO_BALANCEO=%s\n",
				IP,
				PUERTO,
				FS_IP,
				FS_PUERTO,
				RETARDO_PLANIFICACION,
				ALGORITMO_BALANCEO
				);
}

//TRUE SI NO ES WORKER, FALSE SI ES WORKER
bool RecibirPaqueteFilesystem(Paquete* paquete){
	RecibirPaqueteCliente(socketFS,YAMA,paquete);
	if(paquete->header.tipoMensaje == NUEVOWORKER){
		//TODO: GUARDAR Y ENVIAR A MASTER
		void* datos = paquete->Payload;
		int tamanio = paquete->header.tamPayload;
		datosWorker* worker = malloc(tamanio);
		uint32_t largo;
		largo = *((uint32_t*)datos);
		datos += sizeof(uint32_t);
		worker->nodo = string_new();
		strcpy(worker->nodo, datos);
		datos+=largo + 1;
		worker->puerto = *((uint32_t*)datos);
		datos += sizeof(uint32_t);
		largo = *((uint32_t*)datos);
		datos += sizeof(uint32_t);
		worker->ip = string_new();
		strcpy(worker->ip, datos);
		datos += largo + 1;
		datos -= tamanio;
		return false;
	}
	else
	{
		return true;
	}
}

int main(){
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();
	socketFS = ConectarAServidor(FS_PUERTO, FS_IP, FILESYSTEM, YAMA, RecibirHandshake);
	pthread_t conexionFilesystem;
	pthread_create(&conexionFilesystem, NULL, (void*)RecibirPaqueteFilesystem,NULL);
	Servidor(IP, PUERTO, YAMA, accion, RecibirPaqueteServidor);
	pthread_join(conexionFilesystem, NULL);

	return 0;
} 
