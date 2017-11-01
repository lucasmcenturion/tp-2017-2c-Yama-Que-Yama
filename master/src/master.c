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
	datosWorker* worker;
	int bloque;
	char* archivoTemporal;
	char* programa;
	int cantidadDeBytesOcupados;
	char** ListaArchivosTemporales;
	datosWorker workerEncargado;
}solicitudPrograma;


void accionHilo(solicitudPrograma* solicitud){ //Revisar si se envia bien por nodo TODO
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
				bool* ok = true;
				EnviarDatosTipo(socketYAMA, "MASTER" ,ok, sizeof(bool), VALIDACIONYAMA);
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
		EnviarDatosTipo(socketWorker, "MASTER" , solicitud, sizeof(solicitud), REDLOCALWORKER);
		RecibirPaqueteCliente(socketWorker, "MASTER", paqueteArecibir);
		if(paqueteArecibir->header.tipoMensaje == VALIDACIONWORKER){
			if((bool*)paqueteArecibir->Payload) {
				bool* ok = true;
				EnviarDatosTipo(socketYAMA, "MASTER" ,ok, sizeof(bool), VALIDACIONYAMA);
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
		EnviarDatosTipo(socketWorker, "MASTER" , solicitud, sizeof(solicitud), REDGLOBALWORKER);
		RecibirPaqueteCliente(socketWorker, "MASTER", paqueteArecibir);
		if(paqueteArecibir->header.tipoMensaje == VALIDACIONWORKER){
			if((bool*)paqueteArecibir->Payload) {
				bool* ok = true;
				EnviarDatosTipo(socketYAMA, "MASTER" ,ok, sizeof(bool), VALIDACIONYAMA);
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

	//int cantPaquetes = sizeof(paquete->Payload)/sizeof(transformacionDatos);
	hiloWorker* itemNuevo = malloc(sizeof(hiloWorker));
	itemNuevo->worker = ((transformacionDatos*)paquete->Payload)->worker;
	solicitudPrograma* datosParaTransformacion = malloc(sizeof(solicitudPrograma));

	datosParaTransformacion->programa = programaT;
	datosParaTransformacion->worker = &((transformacionDatos*)paquete->Payload)->worker;
	datosParaTransformacion->archivoTemporal = ((transformacionDatos*)paquete->Payload)->archTemp;
	datosParaTransformacion->bloque = ((transformacionDatos*)paquete->Payload)->bloque;
	datosParaTransformacion->cantidadDeBytesOcupados = ((transformacionDatos*)paquete->Payload)->bytesOcupados;


	datosParaTransformacion->header = REDUCCIONLOCAL;
		pthread_create(&(itemNuevo->hilo),NULL,(void*)accionHilo,datosParaTransformacion);
		list_add(listaHilos, itemNuevo);

}








/*	solicitudPrograma* datosParaTransformacion = malloc(sizeof(solicitudPrograma)*cantPaquetes);
	int i;
	for(i=0;i<cantPaquetes;i++){
		datosParaTransformacion[i].worker = &((transformacionDatos*)paquete->Payload)->worker;
		datosParaTransformacion[i].archivoTemporal = ((transformacionDatos*)paquete->Payload)->archTemp;
		datosParaTransformacion[i].bloque = ((transformacionDatos*)paquete->Payload)->bloque;
		datosParaTransformacion[i].cantidadDeBytesOcupados = ((transformacionDatos*)paquete->Payload)->bytesOcupados;
	}


	int j=1;
	int cont=0;
	for(i=0;i<cantPaquetes-1;i++){
		 //PREREQUISITO: Yama nos lo manda ordenado por Nodos
		if(datosParaTransformacion[i].worker->nodo!=datosParaTransformacion[i+1].worker->nodo){ //Nos fijamos si es el mismo nodo
			j++;

		}
		else { //Si dejan de ser iguales, es que ya tenemos 1 nodo terminado
			datosParaTransPorNodo = malloc(sizeof(solicitudPrograma)*j); //Hacemos un malloc por la cantidad de elementos
			int y;
			for(y=cont;y<j;y++){
				datosParaTransPorNodo[y]=datosParaTransformacion[y]; //Asignamos los que encontramos
			}
			datosParaTransPorNodo->header = TRANSFORMACION;
			pthread_create(&(itemNuevo->hilo),NULL,(void*)accionHilo,datosParaTransPorNodo);
			cont = i;
			j=1;
		}
	}
	//Hacemos una vez porque el ultimo siempre nos va a quedar colgado, mas en el caso de que puede ser que los ultimos 3 sean iguales
	datosParaTransPorNodo = malloc(sizeof(solicitudPrograma)*j); //Hacemos un malloc por la cantidad de elementos
	int y;
	for(y=cont;y<j;y++){
		datosParaTransPorNodo[y]=datosParaTransformacion[y]; //Asignamos los que encontramos
	}
	datosParaTransPorNodo[0]->header = TRANSFORMACION;
	pthread_create(&(itemNuevo->hilo),NULL,(void*)accionHilo,datosParaTransPorNodo);
	list_add(listaHilos, itemNuevo);


}
*/

void realizarReduccionLocal(Paquete* paquete, char* programaR){
	solicitudPrograma* datosParaReducLocal = malloc(sizeof(solicitudPrograma));
	hiloWorker* itemNuevo = malloc(sizeof(hiloWorker));
	itemNuevo->worker = *((reduccionLocalDatos*)paquete->Payload)->worker;

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
	itemNuevo->worker = *((reduccionGlobalDatos*)paquete->Payload)->worker;
	solicitudPrograma* datosParaReducGlobal = malloc(sizeof(solicitudPrograma));

	datosParaReducGlobal->programa = programaR;
	datosParaReducGlobal->worker = ((reduccionGlobalDatos*)paquete->Payload)->worker;
	datosParaReducGlobal->workerEncargado = ((reduccionGlobalDatos*)paquete->Payload)->WorkerEncargado;
	datosParaReducGlobal->ListaArchivosTemporales = ((reduccionGlobalDatos*)paquete->Payload)->listaArchivosTemps;
	datosParaReducGlobal->archivoTemporal = ((reduccionGlobalDatos*)paquete->Payload)->archTemp;

	datosParaReducGlobal->header = REDUCCIONGLOBAL;
	pthread_create(&(itemNuevo->hilo),NULL,(void*)accionHilo,datosParaReducGlobal);
	list_add(listaHilos, itemNuevo);

}

int main(int argc, char* argv[]){
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();
	listaHilos = list_create();
	char* programaTrans = argv[1];
	char* programaReduc = argv[2];
	//datosParaTransformacion.progTrans = asdasd; Asignar el programa de transformacion.

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
