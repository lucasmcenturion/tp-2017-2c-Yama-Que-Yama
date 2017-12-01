#include "sockets.h"

char *YAMA_IP;
int YAMA_PUERTO;
int socketYAMA;
t_list* listaHilos;

typedef struct{
	pthread_t hilo;
	datosWorker worker;
} hiloWorker;

typedef struct{
	void* result;
	int size;
} serialized;

int obtenerSizeListdeString(t_list* lista){
	int size=0;
	int tamLista = list_size(lista);
	int i=0;
	for(i=0;i<tamLista;i++){
		size+= strlen(list_get(lista,i))+1;
	}
	return size;
}

void serializacionTyEnvio(nodoT* nodoDeT, int socketWorker){
//Bloque - BytesOcupados - tamStr - programaT - tamStr - archivoTemp
	int mov = 0;
	int sizeAux;
	int size = sizeof(int)*2+sizeof(int)+strlen(nodoDeT->programaT)+1+sizeof(int)+strlen(nodoDeT->archivoTemporal)+1;//+strlen(nodoDeT->worker.ip)+strlen(nodoDeT->worker.nodo)+sizeof(int);
	void* datos = malloc(size);

	memcpy(datos,&nodoDeT->bloque,sizeof(int));
	mov += sizeof(int);
	memcpy(datos+mov,&nodoDeT->bytesOcupados,sizeof(int));
	mov += sizeof(int);
	sizeAux = strlen(nodoDeT->programaT)+1;
	memcpy(datos+mov, &sizeAux,sizeof(int));
	mov += sizeof(int);
	memcpy(datos+mov,nodoDeT->programaT,strlen(nodoDeT->programaT)+1);
	mov+=strlen(nodoDeT->programaT)+1;
	sizeAux = strlen(nodoDeT->archivoTemporal)+1;
	memcpy(datos+mov,&sizeAux,sizeof(int));
	mov += sizeof(int);
	memcpy(datos+mov,nodoDeT->archivoTemporal,strlen(nodoDeT->archivoTemporal)+1);
	mov+=strlen(nodoDeT->archivoTemporal)+1;

	/*memcpy(datos+mov,nodoDeT->worker.ip,strlen(nodoDeT->worker.ip));
	mov+=strlen(nodoDeT->worker.ip);
	memcpy(datos+mov,nodoDeT->worker.nodo,strlen(nodoDeT->worker.nodo));
	mov+=strlen(nodoDeT->worker.nodo);
	memcpy(datos+mov,&nodoDeT->worker.puerto,sizeof(int));*/

	if(!(EnviarDatosTipo(socketWorker, MASTER , datos, size, TRANSFWORKER))) perror("Error al enviar datosT al worker");
	return;
}

void serializacionRLyEnvio(nodoRL* nodoDeRL, int socketWorker){
//tamStr - programaR - tamStr - archivoTemp - tamList - (tamStr - ArchivoTemp)xtamList
	int mov=0;
	int sizeAux;
	int sizeList = obtenerSizeListdeString(nodoDeRL->listaArchivosTemporales);
	int size = sizeof(int)+strlen(nodoDeRL->programaR)+1+sizeof(int)+strlen(nodoDeRL->archivoTemporal)+1+sizeof(int)+list_size(nodoDeRL->listaArchivosTemporales)*sizeof(int)+sizeList;

	void* datos = malloc(size);

	sizeAux = strlen(nodoDeRL->programaR)+1;
	memcpy(datos,&sizeAux,sizeof(int));
	mov+=sizeof(int);
	memcpy(datos+mov,nodoDeRL->programaR,strlen(nodoDeRL->programaR)+1);
	mov+=strlen(nodoDeRL->programaR)+1;
	sizeAux = strlen(nodoDeRL->archivoTemporal)+1;
	memcpy(datos+mov,&sizeAux,sizeof(int));
	mov+=sizeof(int);
	memcpy(datos+mov,nodoDeRL->archivoTemporal,strlen(nodoDeRL->archivoTemporal)+1);
	mov+=strlen(nodoDeRL->archivoTemporal)+1;
	sizeAux = list_size(nodoDeRL->listaArchivosTemporales);
	memcpy(datos+mov,&sizeAux,sizeof(int));
	mov += sizeof(int);


	int i;
	for(i=0;i<list_size(nodoDeRL->listaArchivosTemporales);i++){
		sizeAux = strlen(list_get(nodoDeRL->listaArchivosTemporales,i)+1);
		memcpy(datos+mov,&sizeAux,sizeof(int));
		mov+=sizeof(int);
		memcpy(datos+mov,list_get(nodoDeRL->listaArchivosTemporales,i),strlen(list_get(nodoDeRL->listaArchivosTemporales,i))+1);
		mov+=strlen(list_get(nodoDeRL->listaArchivosTemporales,i))+1;
	}

	if(!(EnviarDatosTipo(socketWorker, MASTER , datos, size, REDLOCALWORKER))) perror("Error al enviar datosRL al worker");
	return;
}

int obtenerSizeListNodoRG(t_list* lista){
	int size = 0;
	int tamList = list_size(lista);
	int i = 0;
	for(i=0;i<tamList;i++){
		nodoRG* nodoEvaluado = list_get(lista,i);
		size += strlen(nodoEvaluado->archTempRL)+1;
		size += sizeof(bool);
		size += strlen(nodoEvaluado->worker.ip)+1;
		size += strlen(nodoEvaluado->worker.nodo)+1;
		size += sizeof(int);
	}
	return size;
}

void serializacionRGyEnvio(solicitudRG* solRG,int socketWorker){
//tamStr - archRG - tamStr - programaRG - tamList - (tamStr - archTempRL - bool - tamStr -nodo - tamStr -ip - puerto)xtamList
	int mov = 0;
	int sizeAux;
	bool boolAux;
	int sizeList = obtenerSizeListNodoRG(solRG->nodos);
	int size = sizeof(int)+strlen(solRG->archRG)+1+sizeof(int)+strlen(solRG->programaR)+1+sizeof(int)+sizeof(int)*3*list_size(solRG->nodos)+sizeList;

	void* datos = malloc(size);

	sizeAux = strlen(solRG->archRG)+1;
	memcpy(datos+mov,&sizeAux,sizeof(int));
	mov += sizeof(int);
	memcpy(datos+mov,solRG->archRG,strlen(solRG->archRG)+1);
	mov += strlen(solRG->archRG)+1;
	sizeAux = strlen(solRG->programaR)+1;
	memcpy(datos+mov,&sizeAux,sizeof(int));
	mov += sizeof(int);
	memcpy(datos+mov,solRG->programaR,strlen(solRG->programaR)+1);
	mov += strlen(solRG->programaR)+1;
	sizeAux = list_size(solRG->nodos);
	memcpy(datos+mov,&sizeAux,sizeof(int));
	mov += sizeof(int);

	int i;
	for(i=0;i<list_size(solRG->nodos);i++){
		nodoRG* nodoAcopiar = list_get(solRG->nodos,i);

		sizeAux = strlen(nodoAcopiar->archTempRL)+1;
		memcpy(datos+mov,&sizeAux,sizeof(int));
		mov += sizeof(int);
		memcpy(datos+mov,nodoAcopiar->archTempRL,strlen(nodoAcopiar->archTempRL)+1);
		mov += strlen(nodoAcopiar->archTempRL)+1;
		boolAux = nodoAcopiar->encargado;
		memcpy(datos+mov,&boolAux,sizeof(bool));
		mov += sizeof(bool);
		sizeAux = strlen(nodoAcopiar->worker.nodo)+1;
		memcpy(datos+mov,&sizeAux,sizeof(int));
		mov += sizeof(int);
		memcpy(datos+mov,nodoAcopiar->worker.nodo,strlen(nodoAcopiar->worker.nodo)+1);
		mov += strlen(nodoAcopiar->worker.nodo)+1;
		sizeAux = strlen(nodoAcopiar->worker.ip)+1;
		memcpy(datos+mov,&sizeAux,sizeof(int));
		mov += sizeof(int);
		memcpy(datos+mov,nodoAcopiar->worker.ip,strlen(nodoAcopiar->worker.ip)+1);
		mov += strlen(nodoAcopiar->worker.ip)+1;
		sizeAux = nodoAcopiar->worker.puerto;
		memcpy(datos+mov,&sizeAux,sizeof(int));
		mov += sizeof(int);
	}

}



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


void accionHilo(void* solicitud){
	Paquete* paquete = malloc(sizeof(Paquete));
	switch (((nodoT*)solicitud)->header)
	{

	case TRANSFORMACION:{
		nodoT* datosT = malloc(sizeof(nodoT));
		datosT->programaT = malloc(strlen(((nodoT*)solicitud)->programaT));
		datosT->archivoTemporal = malloc(strlen(((nodoT*)solicitud)->archivoTemporal));
		*datosT = *(nodoT*)solicitud;

		solicitud=NULL; //solicitud ya no la vamos a usar, para simplificar uso de tipos

		int socketWorker = ConectarAServidor(datosT->worker.puerto, datosT->worker.ip, WORKER, MASTER, RecibirHandshake);
		serializacionTyEnvio(datosT,socketWorker);

		if(RecibirPaqueteCliente(socketWorker, MASTER, paquete)< 0) perror("Error: No se recibio validacion del worker en T");//TODO manejo desconexion
		if(paquete->header.tipoMensaje == VALIDACIONWORKER){
			if((bool*)paquete->Payload) {
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
	}

	case REDUCCIONLOCAL:{
		nodoRL* datosRL = malloc(sizeof(nodoRL));
		datosRL->archivoTemporal = malloc(strlen(((nodoRL*)solicitud)->archivoTemporal));
		datosRL->programaR = malloc(strlen(((nodoRL*)solicitud)->programaR));
		datosRL->listaArchivosTemporales = list_create();

		*datosRL = *(nodoRL*)solicitud;

		solicitud=NULL;

		int socketWorker = ConectarAServidor(datosRL->worker.puerto, datosRL->worker.ip, WORKER, MASTER, RecibirHandshake);

		serializacionRLyEnvio(datosRL,socketWorker);

		if(RecibirPaqueteCliente(socketWorker, MASTER, paquete)<0)perror("Error: No se recibio validacion del worker en RL"); //TODO manejo desconexion
		if(paquete->header.tipoMensaje == VALIDACIONWORKER){
			if((bool*)paquete->Payload) {
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
	}

	case REDUCCIONGLOBAL:{
		solicitudRG* datosRG = malloc(sizeof(solicitudRG));
		datosRG->programaR = malloc(strlen(((solicitudRG*)solicitud)->programaR));
		datosRG->archRG = malloc(strlen(((solicitudRG*)solicitud)->archRG));
		datosRG->nodos = list_create();

		*datosRG = *(solicitudRG*)solicitud;

		solicitud=NULL;

		bool obtenerEncargado(nodoRG* elemento){
			return elemento->encargado;
		}
		nodoRG* workerEncargado = list_find(datosRG->nodos,(void*)obtenerEncargado);

		int socketWorker = ConectarAServidor(workerEncargado->worker.puerto, workerEncargado->worker.ip, WORKER, MASTER, RecibirHandshake);

		serializacionRGyEnvio(datosRG,socketWorker);

		if(RecibirPaqueteCliente(socketWorker, MASTER, paquete)<0)perror("Error: no se recibio validacion del worker en RG");//TODO manejo desconexion
		if(paquete->header.tipoMensaje == VALIDACIONWORKER){
			if((bool*)paquete->Payload) {
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

}


void realizarTransformacion(Paquete* paquete, char* programaT){

	hiloWorker* itemNuevo = malloc(sizeof(hiloWorker));

	itemNuevo->worker = ((nodoT*)paquete->Payload)->worker;
	nodoT* datosParaTransformacion = malloc(sizeof(paquete->header.tamPayload));


	//Aca vendria la deserializacion de lo que me da YAMA
	datosParaTransformacion->programaT = programaT;
	datosParaTransformacion->worker = ((nodoT*)paquete->Payload)->worker;
	datosParaTransformacion->archivoTemporal = ((nodoT*)paquete->Payload)->archivoTemporal;
	datosParaTransformacion->bloque = ((nodoT*)paquete->Payload)->bloque;
	datosParaTransformacion->bytesOcupados = ((nodoT*)paquete->Payload)->bytesOcupados;

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
	char* archivoParaYAMA = argv[3];
	char* archivoFinal = argv[4];

	//FALTA: Mandar mensaje a Yama de que comience transformacion

	socketYAMA = ConectarAServidor(YAMA_PUERTO, YAMA_IP, YAMA, MASTER, RecibirHandshake);
	if(!(EnviarDatosTipo(socketYAMA, MASTER ,archivoParaYAMA, strlen(archivoParaYAMA)+1, TRANSFORMACION))) perror("Error al enviar el archivoParaYAMA a YAMA");
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
