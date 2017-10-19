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



typedef enum{TRANSFORMACION, REDUCCIONLOCAL, REDUCCIONGLOBAL}programa;

typedef struct{
	programa header;
	datosWorker worker;
	int bloque;
	char* archivoTemporal;
	char* programa;
	int cantidadDeBytesOcupados;
}solicitudPrograma;



void accionHilo(solicitudPrograma* solicitud){ //hacer struct structHiloWorker, donde ponemos worker y sacarlo de datosTransWorker
	datosWorker* worker = &solicitud->worker;

	int socketWorker = ConectarAServidor(worker->puerto, worker->ip, WORKER, MASTER, RecibirHandshake);
	Paquete* paqueteArecibir = malloc(sizeof(Paquete));
	switch (solicitud->header)
	{
	case TRANSFORMACION:
		EnviarDatosTipo(socketWorker, "MASTER" , solicitud, sizeof(solicitud), TRANSFWORKER);
		RecibirPaqueteCliente(socketWorker, "MASTER", paqueteArecibir);
		if(paqueteArecibir->header.tipoMensaje == VALIDACIONWORKER){
			if((bool*)paqueteArecibir->Payload) {
				//Envia msj a YAMA con verificacion
			}
			else {
				//manejo de error
			}
		}
		break;

	case REDUCCIONLOCAL:
		EnviarDatosTipo(socketWorker, "MASTER" , solicitud, sizeof(solicitud), REDLOCALWORKER);
		RecibirPaqueteCliente(socketWorker, "MASTER", paqueteArecibir);
		if(paqueteArecibir->header.tipoMensaje == VALIDACIONWORKER){
			if((bool*)paqueteArecibir->Payload) {
				//Envia msj a YAMA con verificacion
			}
			else {
				//manejo de error
			}
		}
		break;


	case REDUCCIONGLOBAL:
		EnviarDatosTipo(socketWorker, "MASTER" , solicitud, sizeof(solicitud), REDGLOBALWORKER);
		RecibirPaqueteCliente(socketWorker, "MASTER", paqueteArecibir);
		if(paqueteArecibir->header.tipoMensaje == VALIDACIONWORKER){
			if((bool*)paqueteArecibir->Payload) {
				//Envia msj a YAMA con verificacion
			}
			else {
				//manejo de error
			}
			break;
		}
	}

	//NO ES DEFINITIVO!
	/*EnviarDatosTipo(socketWorker, "MASTER" , solicitud, sizeof(solicitud), TRANSFWORKER);
	Paquete* paqueteArecibir = malloc(sizeof(Paquete));
	RecibirPaqueteCliente(socketWorker, "MASTER", paqueteArecibir);
	if(paqueteArecibir->header.tipoMensaje == VALIDACIONWORKER){
		if((bool*)paqueteArecibir->Payload) {
			//Envia msj a YAMA con verificacion
		}
		else {
			//manejo de error
		}
	}

	 */
	/*
	datosWorker* worker = w;
	int socketWorker = ConectarAServidor(worker->puerto, worker->ip, WORKER, MASTER, RecibirHandshake);*/
}


void realizarTransformacion(Paquete* paquete){

	hiloWorker* itemNuevo = malloc(sizeof(hiloWorker));
	itemNuevo->worker = ((transformacionDatos*)paquete->Payload)->worker;

	datosTransWorker* datosParaTransformacion = malloc(sizeof(datosTransWorker));
	datosParaTransformacion->worker = &((transformacionDatos*)paquete->Payload)->worker;
	datosParaTransformacion->archTemp = ((transformacionDatos*)paquete->Payload)->archTemp;
	datosParaTransformacion->bloques = ((transformacionDatos*)paquete->Payload)->bloque;
	datosParaTransformacion->bytesOcupados = ((transformacionDatos*)paquete->Payload)->bytesOcupados;


	pthread_create(&(itemNuevo->hilo),NULL,(void*)accionHilo,datosParaTransformacion);

	/*
						pthread_create(&(itemNuevo->hilo), NULL, (void*)accionHilo,
									&(((transformacionDatos*)paquete->Payload)->worker));
	 */

	list_add(listaHilos, itemNuevo);


}


int main(){
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();
	listaHilos = list_create();

	//datosParaTransformacion.progTrans = asdasd; Asignar el programa de transformacion.

	//FALTA: Mandar mensaje a Yama de que comience transformacion

	socketYAMA = ConectarAServidor(YAMA_PUERTO, YAMA_IP, YAMA, MASTER, RecibirHandshake);
	Paquete* paquete = malloc(sizeof(Paquete));

	while (RecibirPaqueteCliente(socketYAMA, MASTER, paquete)){
		if (paquete->header.tipoMensaje == NUEVOWORKER){


			realizarTransformacion(paquete);


			/*hiloWorker* itemNuevo = malloc(sizeof(hiloWorker));
			itemNuevo->worker = ((transformacionDatos*)paquete->Payload)->worker;

			datosTransWorker* datosParaTransformacion = malloc(sizeof(datosTransWorker));
			datosParaTransformacion->worker = &((transformacionDatos*)paquete->Payload)->worker;
			datosParaTransformacion->archTemp = ((transformacionDatos*)paquete->Payload)->archTemp;
			datosParaTransformacion->bloques = ((transformacionDatos*)paquete->Payload)->bloque;
			datosParaTransformacion->bytesOcupados = ((transformacionDatos*)paquete->Payload)->bytesOcupados;
			 */


			//pthread_create(&(itemNuevo->hilo),NULL,(void*)accionHilo,datosParaTransformacion);

			/*
			pthread_create(&(itemNuevo->hilo), NULL, (void*)accionHilo,
						&(((transformacionDatos*)paquete->Payload)->worker));
			 */

			//list_add(listaHilos, itemNuevo);
		}
	}

	list_destroy_and_destroy_elements(listaHilos, LAMBDA(void _(void* elem) {
		pthread_join(((hiloWorker*)elem)->hilo, NULL);
		free(elem); }));


	return 0;
}
