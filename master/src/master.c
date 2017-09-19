#include "sockets.h"

char *YAMA_IP;
int YAMA_PUERTO;
int socketYAMA;
t_list* listaHilos;

typedef struct{
	pthread_t hilo;
	datosWorker worker;
} hiloWorker;

void obtenerValoresArchivoConfiguracion() {
	t_config* arch = config_create("masterCFG.txt");
	YAMA_IP = string_duplicate(config_get_string_value(arch, "YAMA_IP"));
	YAMA_PUERTO = config_get_int_value(arch, "YAMA_PUERTO");
	config_destroy(arch);
}

void imprimirArchivoConfiguracion(){
	printf(
				"Configuración:\n"
				"YAMA_IP=%s\n"
				"YAMA_PUERTO=%d\n",
				YAMA_IP,
				YAMA_PUERTO
				);
}

void accionHilo(void* w){
	datosWorker* worker = w;
	int socketWorker = ConectarAServidor(worker->puerto, worker->ip, WORKER, MASTER, RecibirHandshake);
	//hacer lo que quiera
}


int main(){
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();
	listaHilos = list_create();


	socketYAMA = ConectarAServidor(YAMA_PUERTO, YAMA_IP, YAMA, MASTER, RecibirHandshake);
	Paquete* paquete = malloc(sizeof(Paquete));
	while (RecibirPaqueteCliente(socketYAMA, MASTER, paquete)){
		if (paquete->header.tipoMensaje == NUEVOWORKER){
			hiloWorker* itemNuevo = malloc(sizeof(hiloWorker));
			pthread_create(&(itemNuevo->hilo), NULL, (void*)accionHilo,
						&(((transformacionDatos*)paquete->Payload)->worker));
			list_add(listaHilos, itemNuevo);
		}
	}

	list_destroy_and_destroy_elements(listaHilos, LAMBDA(void _(void* elem) {
				pthread_join(((hiloWorker*)elem)->hilo, NULL);
				free(elem); }));
	

	return 0;
}
