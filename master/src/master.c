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


void accionHilo(solicitudPrograma* solicitud){
	datosWorker* worker = &solicitud->worker;

	int socketWorker = ConectarAServidor(worker->puerto, worker->ip, WORKER, MASTER, RecibirHandshake);
	Paquete* paqueteArecibir = malloc(sizeof(Paquete));
	switch (solicitud->header)
	{

	case TRANSFORMACION:
		EnviarDatosTipo(socketWorker, MASTER , solicitud, sizeof(solicitud), TRANSFWORKER);
		RecibirPaqueteCliente(socketWorker, MASTER, paqueteArecibir);
		if(paqueteArecibir->header.tipoMensaje == VALIDACIONWORKER){
			if((bool*)paqueteArecibir->Payload) {
				bool* ok = true;
				EnviarDatosTipo(socketYAMA, MASTER ,ok, sizeof(bool), VALIDACIONYAMA);
			}
			else {
				perror("No se puedo realizar la operacion de Transformacion");
			}
		}
		else {
			perror("Error en el header, se esperaba VALIDACIONWORKER");
		}
		break;


	case REDUCCIONLOCAL:
		EnviarDatosTipo(socketWorker, MASTER , solicitud, sizeof(solicitud), REDLOCALWORKER);
		RecibirPaqueteCliente(socketWorker, MASTER, paqueteArecibir);
		if(paqueteArecibir->header.tipoMensaje == VALIDACIONWORKER){
			if((bool*)paqueteArecibir->Payload) {
				bool* ok = true;
				EnviarDatosTipo(socketYAMA, MASTER ,ok, sizeof(bool), VALIDACIONYAMA);
			}
			else {
				perror("No se puedo realizar la operacion de Reduccion Local");
			}
		}
		else{
			perror("Error en el header, se esperaba VALIDACIONWORKER");
		}
		break;


	case REDUCCIONGLOBAL:
		EnviarDatosTipo(socketWorker, MASTER , solicitud, sizeof(solicitud), REDGLOBALWORKER);
		RecibirPaqueteCliente(socketWorker, MASTER, paqueteArecibir);
		if(paqueteArecibir->header.tipoMensaje == VALIDACIONWORKER){
			if((bool*)paqueteArecibir->Payload) {
				bool* ok = true;
				EnviarDatosTipo(socketYAMA, MASTER ,ok, sizeof(bool), VALIDACIONYAMA);
			}
			else {
				perror("No se puedo realizar la operacion de Reduccion Global");
			}
			break;
		}
		else{
			perror("Error en el header, se esperaba VALIDACIONWORKER");
		}
	}

}


void realizarTransformacion(Paquete* paquete, char* programaT){

	hiloWorker* itemNuevo = malloc(sizeof(hiloWorker));
	itemNuevo->worker = ((transformacionDatos*)paquete->Payload)->worker;
	solicitudPrograma* datosParaTransformacion = malloc(sizeof(solicitudPrograma));

	datosParaTransformacion->programa = programaT;
	datosParaTransformacion->worker = ((transformacionDatos*)paquete->Payload)->worker;
	datosParaTransformacion->archivoTemporal = ((transformacionDatos*)paquete->Payload)->archTemp;
	datosParaTransformacion->bloque = ((transformacionDatos*)paquete->Payload)->bloque;
	datosParaTransformacion->cantidadDeBytesOcupados = ((transformacionDatos*)paquete->Payload)->bytesOcupados;


	datosParaTransformacion->header = REDUCCIONLOCAL;
		pthread_create(&(itemNuevo->hilo),NULL,(void*)accionHilo,datosParaTransformacion);
		list_add(listaHilos, itemNuevo);

}



void realizarReduccionLocal(Paquete* paquete, char* programaR){
	solicitudPrograma* datosParaReducLocal = malloc(sizeof(solicitudPrograma));
	hiloWorker* itemNuevo = malloc(sizeof(hiloWorker));
	itemNuevo->worker = ((reduccionLocalDatos*)paquete->Payload)->worker;

	datosParaReducLocal->programa = programaR;
	datosParaReducLocal->worker = ((reduccionLocalDatos*)paquete->Payload)->worker;
	datosParaReducLocal->ListaArchivosTemporales = ((reduccionLocalDatos*)paquete->Payload)->listaArchivosTemps;
	datosParaReducLocal->archivoTemporal = ((reduccionLocalDatos*)paquete->Payload)->archTemp;

	datosParaReducLocal->header = REDUCCIONLOCAL;
	pthread_create(&(itemNuevo->hilo),NULL,(void*)accionHilo,datosParaReducLocal);
	list_add(listaHilos, itemNuevo);


}

void realizarReduccionGlobal(Paquete* paquete, char* programaR){

	hiloWorker* itemNuevo = malloc(sizeof(hiloWorker));
	//*itemNuevo->worker =

	/*itemNuevo->worker = ((reduccionGlobalDatos*)paquete->Payload)->worker;
	solicitudPrograma* datosParaReducGlobal = malloc(sizeof(solicitudPrograma));

	datosParaReducGlobal->programa = programaR;
	datosParaReducGlobal->worker = ((reduccionGlobalDatos*)paquete->Payload)->worker;
	datosParaReducGlobal->workerEncargado = ((reduccionGlobalDatos*)paquete->Payload)->WorkerEncargado;
	datosParaReducGlobal->ListaArchivosTemporales = ((reduccionGlobalDatos*)paquete->Payload)->listaArchivosTemps;
	datosParaReducGlobal->archivoTemporal = ((reduccionGlobalDatos*)paquete->Payload)->archTemp;

	datosParaReducGlobal->header = REDUCCIONGLOBAL;
	pthread_create(&(itemNuevo->hilo),NULL,(void*)accionHilo,datosParaReducGlobal);
	list_add(listaHilos, itemNuevo);*/

}

int main(int argc, char* argv[]){
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();
	listaHilos = list_create();
	char* programaTrans = argv[1];
	char* programaReduc = argv[2];

	//FALTA: Mandar mensaje a Yama de que comience transformacion

	socketYAMA = ConectarAServidor(YAMA_PUERTO, YAMA_IP, YAMA, MASTER, RecibirHandshake);
	Paquete* paquete = malloc(sizeof(Paquete));

	while(true){
		while (RecibirPaqueteCliente(socketYAMA, MASTER, paquete)){
			switch(paquete->header.tipoMensaje){
			case NUEVOWORKER:
				realizarTransformacion(paquete, programaTrans);
				break;
			case REDLOCALWORKER:
				realizarReduccionLocal(paquete, programaReduc);
				break;
			case REDGLOBALWORKER:
				realizarReduccionGlobal(paquete, programaReduc);
				break;
			}
		}
	}

	list_destroy_and_destroy_elements(listaHilos, LAMBDA(void _(void* elem) {
		pthread_join(((hiloWorker*)elem)->hilo, NULL);
		free(elem); }));


	return 0;
}
