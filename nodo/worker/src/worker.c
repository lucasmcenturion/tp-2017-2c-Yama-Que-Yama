#include "sockets.h"

char *IP_NODO, *IP_FILESYSTEM,*NOMBRE_NODO,*RUTA_DATABIN;
int PUERTO_FILESYSTEM, PUERTO_WORKER, PUERTO_DATANODE;
int socketFS;
t_list* listaDeProcesos;
bool end;

void obtenerValoresArchivoConfiguracion() {
	t_config* arch = config_create("../nodoCFG.txt");
	IP_FILESYSTEM = string_duplicate(config_get_string_value(arch, "IP_FILESYSTEM"));
	PUERTO_FILESYSTEM = config_get_int_value(arch, "PUERTO_FILESYSTEM");
	NOMBRE_NODO = string_duplicate(config_get_string_value(arch, "NOMBRE_NODO"));
	IP_NODO = string_duplicate(config_get_string_value(arch, "IP_NODO"));
	PUERTO_WORKER = config_get_int_value(arch, "PUERTO_WORKER");
	RUTA_DATABIN = string_duplicate(config_get_string_value(arch, "RUTA_DATABIN"));
	config_destroy(arch);
}

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
				);
}

void accionPadre(void* socketMaster){
}

void accionHijo(void* socketM){

	int socketMaster = *(int*) socketM;
	Paquete* paqueteArecibir = malloc(sizeof(Paquete));
	RecibirPaqueteCliente(socketMaster, WORKER, paqueteArecibir);
	if(!strcmp(paqueteArecibir->header.emisor, MASTER)){ //Posible error en el !

		solicitudPrograma* programa = malloc(paqueteArecibir->header.tamPayload);
		switch(paqueteArecibir->header.tipoMensaje){
		case TRANSFWORKER:{
			programa->programa = ((solicitudPrograma*)paqueteArecibir->Payload)->programa;
			programa->archivoTemporal = ((solicitudPrograma*)paqueteArecibir->Payload)->archivoTemporal;
			programa->bloque = ((solicitudPrograma*)paqueteArecibir->Payload)->bloque;
			programa->cantidadDeBytesOcupados = ((solicitudPrograma*)paqueteArecibir->Payload)->cantidadDeBytesOcupados;
			break;
		}
		case REDLOCALWORKER:{
			programa->programa = ((solicitudPrograma*)paqueteArecibir->Payload)->programa;
			programa->archivoTemporal = ((solicitudPrograma*)paqueteArecibir->Payload)->archivoTemporal;
			programa->ListaArchivosTemporales = ((solicitudPrograma*)paqueteArecibir->Payload)->ListaArchivosTemporales;
			break;
		}
		case REDGLOBALWORKER:{
			programa->programa = ((solicitudPrograma*)paqueteArecibir->Payload)->programa;
			programa->archivoTemporal = ((solicitudPrograma*)paqueteArecibir->Payload)->archivoTemporal;
			programa->workerEncargado = ((solicitudPrograma*)paqueteArecibir->Payload)->workerEncargado;
			programa->ListaArchivosTemporales = ((solicitudPrograma*)paqueteArecibir->Payload)->ListaArchivosTemporales;
			break;
		}
		}

	}
	else{
		perror("Recibido de otro emisor que no es master");
	}

}


int main(){
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();
	ServidorConcurrenteForks(IP_NODO, PUERTO_WORKER, WORKER, &listaDeProcesos, &end, accionPadre, accionHijo);

	socketFS = ConectarAServidor(PUERTO_FILESYSTEM, IP_FILESYSTEM, FILESYSTEM, WORKER, RecibirHandshake);

	return 0;
}
