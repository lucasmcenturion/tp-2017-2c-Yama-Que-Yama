#include "sockets.h"
#include <time.h>

char *YAMA_IP;
int YAMA_PUERTO;
int socketYAMA;

int contTRealizadas;
int contRLRealizadas;
int contRGRealizadas;

int contTMaxP;
int contRLMaxP;

int contTActuales;
int contRLActuales;
int contRGActuales;

int contTfallos;
int contRLfallos;
int contRGfallos;

pthread_mutex_t mutex_Tfallos;
pthread_mutex_t mutex_TActuales;
pthread_mutex_t mutex_TRealizadas;
pthread_mutex_t mutex_RLfallos;
pthread_mutex_t mutex_RLActuales;
pthread_mutex_t mutex_RLRealizadas;
pthread_mutex_t mutex_RGfallos;
pthread_mutex_t mutex_RGRealizadas;
pthread_mutex_t mutex_listaJobs;

pthread_mutex_t mutex_log;

t_list* duracionesJob;
t_list* listaHilos;

typedef struct{
	etapa header;
	double time;
} duracionJob;

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

double sumListaDuraciones(t_list* duraciones){
	double suma = 0;
	int tamList = list_size(duraciones);

	int i;
	for(i=0;i<tamList;i++){
		duracionJob* duracion = list_get(duraciones,i);
		suma += duracion->time;
	}
	return suma;
}

bool metricas(int tiempoInicioPrograma){


	pthread_mutex_lock(&mutex_log);
	escribir_log("log_master","master","se imprimen metricas","info");
	pthread_mutex_unlock(&mutex_log);

	double tiempoDemoradoJobCompleto = difftime(time(0),tiempoInicioPrograma);

	printf("\nSe procede a mostrar las metricas del Job:\n\n");
	printf("1.Tiempo total de ejecucion del Job (Minutos): %f\n", tiempoDemoradoJobCompleto);

	double tiempoPromedioT;
	double tiempoPromedioRL;
	double tiempoPromedioRG;
	//T
	if(contTRealizadas!=0){
		bool esTransformacion(duracionJob* elem){
			return (elem->header==TRANSFORMACION);
		}
		t_list* //listaTransformacion = list_create();   --- si vas a llenar la lista a partir de un list_filter no hagas un create
		listaTransformacion = list_filter(duracionesJob,(void*)esTransformacion);
		double tiempoTotalT = sumListaDuraciones(listaTransformacion);

		tiempoPromedioT = tiempoTotalT/contTRealizadas;
	}
	else {
		tiempoPromedioT = 0;
	}
	//RL
	if(contRLRealizadas!=0){
		bool esReduccionL(duracionJob* elem){
			return (elem->header==REDUCCIONLOCAL);
		}
		t_list* //listaReduccionLocal = list_create(); --- si vas a llenar la lista a partir de un list_filter no hagas un create
		listaReduccionLocal = list_filter(duracionesJob,(void*)esReduccionL);
		double tiempoTotalRL = sumListaDuraciones(listaReduccionLocal);

		tiempoPromedioRL = tiempoTotalRL / contRLRealizadas;
	}
	else {
		tiempoPromedioRL = 0;
	}
	//RG
	if(contRGRealizadas!=0){
		bool esReduccionG(duracionJob* elem){
			return (elem->header==REDUCCIONGLOBAL);
		}
		t_list* //listaReduccionGlobal = list_create();  --- si vas a llenar la lista a partir de un list_filter no hagas un create
		listaReduccionGlobal = list_filter(duracionesJob,(void*)esReduccionG);
		double tiempoTotalRG = sumListaDuraciones(listaReduccionGlobal);

		tiempoPromedioRG = tiempoTotalRG / contRGRealizadas;
	}
	else {
		tiempoPromedioRG = 0;
	}

	printf("2.Tiempo promedio de ejecucion de cada etapa principal del Job:\n Transformacion: %f\n Reduccion Local: %f\n Reduccion Global:%f\n\n",tiempoPromedioT,tiempoPromedioRL,tiempoPromedioRG );
	printf("3.Cantidad maxima de tareas de Transformacion y Reduccion Local ejecutadas de forma paralela \n Transformacion: %d \n Reduccion Local %d\n\n",contTMaxP,contRLMaxP);
	printf("4.Cantidad total de tareas realizadas en cada etapa principal del Job:\n Transformacion: %d\n Reduccion Local: %d\n Reduccion Global: %d\n\n",contTRealizadas,contRLRealizadas,contRGRealizadas);
	printf("5. Cantidad de fallos obtenidos en la realizacion de un Job:\n Transformacion: %d\n Reduccion Local: %d\n Reduccion Global: %d\n\n",contTfallos,contRLfallos,contRGfallos);

	return false;
}




bool serializacionTyEnvio(nodoT* nodoDeT, int socketWorker){
	//Bloque - BytesOcupados - tamStr - programaT - tamStr - archivoTemp
	int mov = 0;
	int sizeAux;
	int size = sizeof(int)*2+sizeof(int)+strlen(nodoDeT->programaT)+1+sizeof(int)+strlen(nodoDeT->archivoTemporal)+1;//+strlen(nodoDeT->worker.nodo)+1;
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
	/*sizeAux = strlen(nodoDeT->worker.nodo) +1;
	memcpy(datos+mov,&sizeAux,sizeof(int));
	mov += sizeof(int);
	memcpy(datos+mov,nodoDeT->worker.nodo,strlen(nodoDeT->worker.nodo)+1);
	mov += strlen(nodoDeT->worker.nodo)+1;*/

	if(!(EnviarDatosTipo(socketWorker, MASTER , datos, size, TRANSFWORKER))) {
		perror("Error al enviar datosT al worker");
		free(datos);
		return false;
	}
	free(datos);
	return true;
}

bool serializacionRLyEnvio(nodoRL* nodoDeRL, int socketWorker){
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
		sizeAux = strlen(list_get(nodoDeRL->listaArchivosTemporales,i))+1;
		memcpy(datos+mov,&sizeAux,sizeof(int));
		mov+=sizeof(int);
		memcpy(datos+mov,list_get(nodoDeRL->listaArchivosTemporales,i),strlen(list_get(nodoDeRL->listaArchivosTemporales,i))+1);
		mov+=strlen(list_get(nodoDeRL->listaArchivosTemporales,i))+1;
	}

	if(!(EnviarDatosTipo(socketWorker, MASTER , datos, size, REDLOCALWORKER))){
		perror("Error al enviar datosRL al worker");
		free(datos);
		return false;
	}
	free(datos);
	return true;
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

bool serializacionRGyEnvio(solicitudRG* solRG,int socketWorker){
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
	if(!(EnviarDatosTipo(socketWorker,MASTER,datos,size,REDGLOBALWORKER))){
		perror("Error al enviar datosRG al Worker");
		free(datos);
		return false;
	}
	free(datos);
	return true;
}

bool serializacionAFyEnvio(datoAF* datosAF, int socketWorker){//TODO terminar el camino de AF
	// tamString - ResultRG - tamString - rutaArchivoF
	int mov = 0;
	int sizeAux;
	int size = sizeof(int)+strlen(datosAF->resultRG)+1+sizeof(int)+strlen(datosAF->rutaArchivoF)+1;
	void* datos = malloc(size);

	sizeAux = strlen(datosAF->resultRG)+1;
	memcpy(datos+mov,&sizeAux,sizeof(int));
	mov += sizeof(int);

	memcpy(datos+mov,datosAF->resultRG,strlen(datosAF->resultRG)+1);
	mov += strlen(datosAF->resultRG)+1;

	sizeAux = strlen(datosAF->rutaArchivoF)+1;
	memcpy(datos+mov,&sizeAux,sizeof(int));
	mov += sizeof(int);
	memcpy(datos+mov,datosAF->rutaArchivoF,strlen(datosAF->rutaArchivoF)+1);


	if(!(EnviarDatosTipo(socketWorker,MASTER,datos,size,SOLICITUDALMACENADOFINAL))){
		perror("Error al enviar datosAF al worker");
		free(datos);
		return false;
	}
	free(datos);
	return true;

}

bool serializacionDeRTAyEnvio(rtaEstado* resultado,int socketYAMA, tipo header){
	// bllque - idJob - noodo - bool //TODO
	int mov = 0;
	int size;
	void* datos;
	if(resultado->bloque != -1){//Para chequear si es transformacion
		size = sizeof(int)*2 + strlen(resultado->nodo)+1+sizeof(bool);
		datos = malloc(size);

		memcpy(datos+mov,&(resultado->bloque),sizeof(int));
		mov += sizeof(int);
	}
	else{ //No es transformacion
		size = sizeof(int) + strlen(resultado->nodo)+1+sizeof(bool);
		datos = malloc(size);
	}
	memcpy(datos+mov,&(resultado->idJob),sizeof(int));
	mov += sizeof(int);
	strcpy(datos+mov,resultado->nodo);
	mov += strlen(resultado->nodo)+1;
	memcpy(datos+mov,&(resultado->exito),sizeof(bool));
	mov += sizeof(bool);

	if(!(EnviarDatosTipo(socketYAMA, MASTER ,datos, size, header))){
		perror("No se puedo enviar validacion del Worker a YAMA");
		free(datos);
		return false;
	}
	free(datos);
	return true;
}


void obtenerValoresArchivoConfiguracion() {
	t_config* arch = config_create("/home/utnso/workspace/tp-2017-2c-Yama-Que-Yama/master/masterCFG.txt");
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

int obtenerIdJobDeRuta(char* rutaTemporal){
	// /tmp/jXnybz
	// 0123456
	char** array  = string_split(rutaTemporal, "j");
	char** arrayAux = string_split(array[1], "n");
	int resultado = atoi(arrayAux[0]);
	string_iterate_lines(array, free);
	string_iterate_lines(arrayAux, free);
	free(array);
	free(arrayAux);
	return resultado;
}


int obtenerBloqueDeRuta(char* rutaTemporal){

	// /tmp/jXnybz
	// 0123456
	char** array  = string_split(rutaTemporal, "b");
	int resultado = atoi(array[1]);
	string_iterate_lines(array, free);
	free(array);
	return resultado;
}


void accionHilo(void* solicitud){
	Paquete* paquete = malloc(sizeof(Paquete));
	switch (((nodoT*)solicitud)->header){

	case TRANSFORMACION:{
		duracionJob* duracionDelJob = malloc(sizeof(duracionJob));
		duracionDelJob->header = TRANSFORMACION;
		time_t inicio = time(0);
		pthread_mutex_lock(&mutex_TActuales);
		contTActuales++;
		if(contTActuales>contTMaxP) contTMaxP = contTActuales;
		pthread_mutex_unlock(&mutex_TActuales);

		nodoT* datosT = (nodoT*)solicitud;
		/*malloc(sizeof(nodoT));
		datosT->programaT = malloc(strlen(((nodoT*)solicitud)->programaT));
		datosT->archivoTemporal = malloc(strlen(((nodoT*)solicitud)->archivoTemporal));
		 *datosT = *(nodoT*)solicitud;*/

		solicitud=NULL;

		rtaEstado* resultado = malloc(sizeof(rtaEstado));
		resultado->bloque = obtenerBloqueDeRuta(datosT->archivoTemporal);
		resultado->nodo = malloc(strlen(datosT->worker.nodo)+1);
		strcpy(resultado->nodo,datosT->worker.nodo);
		printf("nodo:%s y bloque %i\n", resultado->nodo, resultado->bloque);
		resultado->idJob = obtenerIdJobDeRuta(datosT->archivoTemporal);

		pthread_mutex_lock(&mutex_log);
		escribir_log("log_master","master","Se envia la transformacion a Worker","info");
		pthread_mutex_unlock(&mutex_log);

		int socketWorker = ConectarAServidor(datosT->worker.puerto, datosT->worker.ip, WORKER, MASTER, RecibirHandshake);
		resultado->exito = serializacionTyEnvio(datosT,socketWorker);

		if(RecibirPaqueteCliente(socketWorker, MASTER, paquete)<= 0) {
			perror("Error: No se recibio validacion del worker en T"); //manejo desconexion

			pthread_mutex_lock(&mutex_log);
			escribir_log("log_master","master","Fallo recibir validacion del Worker en el caso de Transformacion, se envia a Yama para replanificar","error");
			pthread_mutex_unlock(&mutex_log);

			resultado->exito = false;
			serializacionDeRTAyEnvio(resultado,socketYAMA,FINTRANSFORMACION);

			pthread_mutex_lock(&mutex_Tfallos);
			contTfallos++;
			pthread_mutex_unlock(&mutex_Tfallos);
			pthread_mutex_lock(&mutex_TActuales);
			contTActuales--;
			pthread_mutex_unlock(&mutex_TActuales);
		}
		else {
			if(paquete->header.tipoMensaje == VALIDACIONWORKER){
				if((bool*)paquete->Payload) {

					duracionDelJob->time = difftime(time(0),inicio);

					pthread_mutex_lock(&mutex_log);
					escribir_log("log_master","master","Se realizo exitosamente la transformacion, se envia notificacion a Yama","info");
					pthread_mutex_unlock(&mutex_log);

					pthread_mutex_lock(&mutex_listaJobs);
					list_add(duracionesJob,duracionDelJob);
					pthread_mutex_unlock(&mutex_listaJobs);
					pthread_mutex_lock(&mutex_TActuales);
					contTActuales--;
					pthread_mutex_unlock(&mutex_TActuales);
					pthread_mutex_lock(&mutex_TRealizadas);
					contTRealizadas++;
					pthread_mutex_unlock(&mutex_TRealizadas);
					serializacionDeRTAyEnvio(resultado,socketYAMA,FINTRANSFORMACION);
				}
				else {
					perror("Worker notifico que hubo una falla"); //no contemplado en Worker
				}
			}
			else {
				perror("Error en el header, se esperaba VALIDACIONWORKER");//Error nuestro, no contemplado para informar a YAMA
			}
			free(resultado);
			free(datosT);
		}
		break;
	}

	case REDUCCIONLOCAL:{
		duracionJob* duracionDelJob = malloc(sizeof(duracionJob));
		duracionDelJob->header = REDUCCIONLOCAL;
		time_t inicio = time(0);
		pthread_mutex_lock(&mutex_RLActuales);
		contRLActuales++;
		if(contRLActuales>contRLMaxP) contRLMaxP = contRLActuales;
		pthread_mutex_unlock(&mutex_RLActuales);

		/*nodoRL* datosRL = malloc(sizeof(nodoRL));
		datosRL->archivoTemporal = malloc(strlen(((nodoRL*)solicitud)->archivoTemporal));
		datosRL->programaR = malloc(strlen(((nodoRL*)solicitud)->programaR));
		datosRL->listaArchivosTemporales = list_create();*/

		nodoRL* datosRL = (nodoRL*)solicitud;

		solicitud=NULL;

		rtaEstado* resultado = malloc(sizeof(rtaEstado));
		resultado->bloque = -1;
		resultado->nodo = malloc(strlen(datosRL->worker.nodo)+1);
		strcpy(resultado->nodo,datosRL->worker.nodo);
		resultado->idJob = datosRL->idJob;


		pthread_mutex_lock(&mutex_log);
		escribir_log("log_master","master","Se envia la reduccionLocal a Worker","info");
		pthread_mutex_unlock(&mutex_log);

		int socketWorker = ConectarAServidor(datosRL->worker.puerto, datosRL->worker.ip, WORKER, MASTER, RecibirHandshake);

		resultado->exito = serializacionRLyEnvio(datosRL,socketWorker);

		if(RecibirPaqueteCliente(socketWorker, MASTER, paquete)<=0){
			perror("Error: No se recibio validacion del worker en RL");//manejo desconexion

			pthread_mutex_lock(&mutex_log);
			escribir_log("log_master","master","Fallo recibir validacion del Worker en el caso de Reduccion Local, se envia a Yama para replanificar","error");
			pthread_mutex_unlock(&mutex_log);


			resultado->exito = false;
			serializacionDeRTAyEnvio(resultado,socketYAMA,FINREDUCCIONLOCAL);

			pthread_mutex_lock(&mutex_RLfallos);
			contRLfallos++;
			pthread_mutex_unlock(&mutex_RLfallos);
			pthread_mutex_lock(&mutex_RLActuales);
			contRLActuales--;
			pthread_mutex_unlock(&mutex_RLActuales);

		}
		else {
			if(paquete->header.tipoMensaje == VALIDACIONWORKER){
				if((bool*)paquete->Payload) {
					duracionDelJob->time = difftime(time(0),inicio);


					pthread_mutex_lock(&mutex_log);
					escribir_log("log_master","master","Se realizo exitosamente la reduccion Local, se envia notificacion a Yama","info");
					pthread_mutex_unlock(&mutex_log);

					pthread_mutex_lock(&mutex_listaJobs);
					list_add(duracionesJob,duracionDelJob);
					pthread_mutex_unlock(&mutex_listaJobs);
					pthread_mutex_lock(&mutex_RLActuales);
					contRLActuales--;
					pthread_mutex_unlock(&mutex_RLActuales);
					pthread_mutex_lock(&mutex_RLRealizadas);
					contRLRealizadas++;
					pthread_mutex_unlock(&mutex_RLRealizadas);
					serializacionDeRTAyEnvio(resultado,socketYAMA,FINREDUCCIONLOCAL);
				}
				else {
					perror("No se puedo realizar la operacion de Reduccion Local"); //Este caso  no se contempla en Worker, no se como puede ocurrir
				}
			}
			else{
				perror("Error en el header, se esperaba VALIDACIONWORKER"); //Este caso ocurriria por error de programacion, no se contempla para notificar a YAMA
			}
			free(resultado);
			free(datosRL);
		}
		break;

	}

	case REDUCCIONGLOBAL:{
		duracionJob* duracionDelJob = malloc(sizeof(duracionJob));
		duracionDelJob->header = REDUCCIONGLOBAL;
		time_t inicio = time(0);
		/*solicitudRG* datosRG = malloc(sizeof(solicitudRG));
		datosRG->programaR = malloc(strlen(((solicitudRG*)solicitud)->programaR));
		datosRG->archRG = malloc(strlen(((solicitudRG*)solicitud)->archRG));
		datosRG->nodos = list_create();*/

		solicitudRG* datosRG = (solicitudRG*)solicitud;

		solicitud=NULL;

		bool obtenerEncargado(nodoRG* elemento){
			return elemento->encargado;
		}
		nodoRG* workerEncargado = list_find(datosRG->nodos,(void*)obtenerEncargado);

		rtaEstado* resultado = malloc(sizeof(rtaEstado));
		resultado->bloque = -1;
		resultado->nodo = malloc(strlen(workerEncargado->worker.nodo)+1);
		strcpy(resultado->nodo,workerEncargado->worker.nodo);

		resultado->idJob = datosRG->idJob;

		pthread_mutex_lock(&mutex_log);
		escribir_log("log_master","master","Se envia la Reduccion Global a Worker","info");
		pthread_mutex_unlock(&mutex_log);

		int socketWorker = ConectarAServidor(workerEncargado->worker.puerto, workerEncargado->worker.ip, WORKER, MASTER, RecibirHandshake);

		resultado->exito = serializacionRGyEnvio(datosRG,socketWorker);

		if(RecibirPaqueteCliente(socketWorker, MASTER, paquete)<=0){
			perror("Error: no se recibio validacion del worker en RG");//manejo desconexion

			pthread_mutex_lock(&mutex_log);
			escribir_log("log_master","master","Fallo recibir validacion del Worker en el caso de Reduccion Global, se envia a Yama para replanificar","error");
			pthread_mutex_unlock(&mutex_log);

			resultado->exito = false;
			serializacionDeRTAyEnvio(resultado,socketYAMA,FINREDUCCIONGLOBAL);
			pthread_mutex_lock(&mutex_RGfallos);
			contRGfallos++;
			pthread_mutex_unlock(&mutex_RGfallos);

		} else {
			if(paquete->header.tipoMensaje == VALIDACIONWORKER){
				if((bool*)paquete->Payload) {
					duracionDelJob->time = difftime(time(0),inicio);

					pthread_mutex_lock(&mutex_log);
					escribir_log("log_master","master","Se realizo exitosamente la Reduccion Global, se envia notificacion a Yama","info");
					pthread_mutex_unlock(&mutex_log);

					pthread_mutex_lock(&mutex_listaJobs);
					list_add(duracionesJob,duracionDelJob);
					pthread_mutex_unlock(&mutex_listaJobs);
					pthread_mutex_lock(&mutex_RGRealizadas);
					contRGRealizadas++;
					pthread_mutex_unlock(&mutex_RGRealizadas);
					serializacionDeRTAyEnvio(resultado,socketYAMA,FINREDUCCIONGLOBAL);
				}
				else {
					perror("No se puedo realizar la operacion de Reduccion Global");
				}
			}else{
				perror("Error en el header, se esperaba VALIDACIONWORKER");
			}
			free(resultado);
			free(datosRG);
		}
		break;
	}

	}
	free(paquete);
}



void realizarTransformacion(Paquete* paquete, char* programaT){

	pthread_mutex_lock(&mutex_log);
	escribir_log("log_master","master","Se recibe respuesta de Transformacion de Yama, comienza Deserializado...","info");
	pthread_mutex_unlock(&mutex_log);

	void* datos = paquete->Payload;

	int cantBloques = ((uint32_t*)datos)[0];
	datos += sizeof(uint32_t);

	int i;
	for (i=0; i < cantBloques; i++){
		nodoT* datosParaT = malloc(sizeof(nodoT));
		//numero de bloque
		datosParaT->bloque = ((uint32_t*)datos)[0];
		//cantbytesocupados
		datosParaT->bytesOcupados = ((uint32_t*)datos)[1];
		//puerto worker
		datosParaT->worker.puerto = ((uint32_t*)datos)[2];
		datos+=sizeof(uint32_t) * 3;
		//ip worker
		datosParaT->worker.ip = malloc(strlen(datos)+1);
		strcpy(datosParaT->worker.ip, datos);
		datos += strlen(datos)+1;
		//nombre nodo del worker
		datosParaT->worker.nodo = malloc(strlen(datos)+1);
		strcpy(datosParaT->worker.nodo, datos);
		datos += strlen(datos)+1;
		//ruta temporal
		datosParaT->archivoTemporal = malloc(strlen(datos)+1);
		strcpy(datosParaT->archivoTemporal, datos);
		datos += strlen(datos)+1;

		hiloWorker* itemNuevo = malloc(sizeof(hiloWorker));
		itemNuevo->worker = datosParaT->worker;

		datosParaT->programaT = malloc(strlen(programaT)+1);
		strcpy(datosParaT->programaT,programaT);
		datosParaT->header = TRANSFORMACION;


		pthread_mutex_lock(&mutex_log);
		escribir_log("log_master","master","Se deserializo correctamente:","info");
		char* forLog = string_from_format("Nodo: %s   Puerto: %d   IP: %s   Bloque: %d   BytesOcupados: %d",datosParaT->worker.nodo,datosParaT->worker.puerto,datosParaT->worker.ip,datosParaT->bloque,datosParaT->bytesOcupados);
		escribir_log("log_master","master",forLog,"info");
		free(forLog);
		pthread_mutex_unlock(&mutex_log);

		pthread_create(&(itemNuevo->hilo),NULL,(void*)accionHilo,datosParaT);
		list_add(listaHilos, itemNuevo);

	}
	datos -= paquete->header.tamPayload;

	if (paquete->Payload != NULL)
		free(paquete->Payload);
	free(paquete);
}



void realizarReduccionLocal(Paquete* paquete, char* programaR){

	pthread_mutex_lock(&mutex_log);
	escribir_log("log_master","master","Se recibe solicitud de Reduccion Local de Yama, comienza Deserializado...","info");
	pthread_mutex_unlock(&mutex_log);

	nodoRL* datosParaRL = malloc(sizeof(nodoRL));
	void* datos = paquete->Payload;
	int cantArchTemp = ((int*)datos)[0];
	datosParaRL->worker.puerto = ((int*)datos)[1];
	datos += sizeof(int)*2;
	datosParaRL->worker.ip = malloc(strlen(datos) +1);
	strcpy(datosParaRL->worker.ip,datos);
	datos += strlen(datos) +1;
	datosParaRL->worker.nodo = malloc(strlen(datos) +1);
	strcpy(datosParaRL->worker.nodo,datos);
	datos += strlen(datos) +1;
	datosParaRL->archivoTemporal = malloc(strlen(datos) +1);
	strcpy(datosParaRL->archivoTemporal,datos);
	datos += strlen(datos) +1;
	int idJob;
	datosParaRL->listaArchivosTemporales = list_create();
	int i;
	for(i=0;i<cantArchTemp;i++){
		char* bufferAux = malloc(strlen(datos)+1);
		strcpy(bufferAux,datos);
		if(i==0)
			idJob = obtenerIdJobDeRuta(datos);
		datos += strlen(datos)+1;
		list_add(datosParaRL->listaArchivosTemporales,bufferAux);
	}
	datosParaRL->idJob = idJob;

	pthread_mutex_lock(&mutex_log);
	char* forLog = string_from_format("Deserializado Exitoso: Puerto: %d IP: %s Nodo: %s ArchivoTemporal: %s",datosParaRL->worker.puerto,datosParaRL->worker.ip,datosParaRL->worker.nodo,datosParaRL->archivoTemporal);
	escribir_log("log_master","master",forLog,"info");
	free(forLog);
	pthread_mutex_unlock(&mutex_log);


	hiloWorker* itemNuevo = malloc(sizeof(hiloWorker));
	itemNuevo->worker = datosParaRL->worker;

	datosParaRL->programaR = malloc(strlen(programaR)+1);
	strcpy(datosParaRL->programaR,programaR);
	datosParaRL->header = REDUCCIONLOCAL;

	pthread_create(&(itemNuevo->hilo),NULL,(void*)accionHilo,datosParaRL);
	list_add(listaHilos, itemNuevo);

	if (paquete->Payload != NULL)
		free(paquete->Payload);
	free(paquete);
}

void realizarReduccionGlobal(Paquete* paquete, char* programaR){

	pthread_mutex_lock(&mutex_log);
	escribir_log("log_master","master","Se recibe solicitud de Reduccion Global de Yama, comienza Deserializado...","info");
	pthread_mutex_unlock(&mutex_log);

	void* datos = paquete->Payload;
	solicitudRG* datosParaRG = malloc(sizeof(solicitudRG));

	datosParaRG->idJob = *((uint32_t*)datos);
	datos += sizeof(uint32_t);

	char* nodoEncargado = malloc(strlen(datos)+1);
	strcpy(nodoEncargado,datos);
	datos += strlen(datos)+1;

	datosParaRG->archRG = malloc(strlen(datos)+1);
	strcpy(datosParaRG->archRG,datos);
	datos += strlen(datos)+1;

	datosParaRG->nodos = list_create();

	datosParaRG->cantNodos = ((int*)datos)[0];
	datos += sizeof(int);

	int i;
	for(i=0;i<(datosParaRG->cantNodos);i++){
		nodoRG* nodo = malloc(sizeof(nodoRG));
		nodo->worker.nodo = malloc(strlen(datos)+1);
		strcpy(nodo->worker.nodo,datos);
		datos += strlen(nodo->worker.nodo)+1;

		nodo->worker.ip = malloc(strlen(datos)+1);
		strcpy(nodo->worker.ip,datos);
		datos += strlen(nodo->worker.ip)+1;

		nodo->worker.puerto = ((int*)datos)[0];
		datos += sizeof(int);

		nodo->archTempRL = malloc(strlen(datos)+1);
		strcpy(nodo->archTempRL,datos);
		datos += strlen(nodo->archTempRL)+1;

		if(!strcmp(nodo->worker.nodo,nodoEncargado)){
			nodo->encargado = true;
		}
		else {
			nodo->encargado = false;
		}
		list_add(datosParaRG->nodos,nodo);
	}

	bool buscarEncargado(nodoRG* elem){
		return elem->encargado;
	}

	datosParaRG->programaR = malloc(strlen(programaR)+1);
	strcpy(datosParaRG->programaR,programaR);
	datosParaRG->header = REDUCCIONGLOBAL;

	hiloWorker* itemNuevo = malloc(sizeof(hiloWorker));
	nodoRG* workerEncargado = list_find(datosParaRG->nodos,(void*)buscarEncargado);


	pthread_mutex_lock(&mutex_log);
	char* forLog = string_from_format("Deserializado Exitoso: Datos del Worker Encargado Puerto: %d IP: %s Nodo: %s ArchivoTemporal: %s",workerEncargado->worker.puerto,workerEncargado->worker.ip,workerEncargado->worker.nodo,datosParaRG->archRG);
	escribir_log("log_master","master",forLog,"info");
	free(forLog);
	pthread_mutex_unlock(&mutex_log);

	itemNuevo->worker = workerEncargado->worker;
	pthread_create(&(itemNuevo->hilo),NULL,(void*)accionHilo,datosParaRG);
	list_add(listaHilos, itemNuevo);

	if (paquete->Payload != NULL)
		free(paquete->Payload);
	free(paquete);
}

void realizarAlmacenamientoFinal(Paquete* paquete,char* rutaArchivoF){ //TODO terminar el camino de AF
	//YAMA me da IP y Puerto de Worker --- temp final noodo ip puerto
	pthread_mutex_lock(&mutex_log);
	escribir_log("log_master","master","Se recibe solicitud de Almacenado Final de Yama, comienza Deserializado...","info");
	pthread_mutex_unlock(&mutex_log);

	void* datos = paquete->Payload;
	datoAF* datosAF = malloc(sizeof(datoAF));

	datosAF->resultRG = malloc(strlen(datos)+1);
	strcpy(datosAF->resultRG,datos);
	datos += strlen(datos)+1;

	datosAF->rutaArchivoF = malloc(strlen(datos)+1);
	strcpy(datosAF->rutaArchivoF,datos);
	datos += strlen(datos)+1;

	datosAF->worker.nodo = malloc(strlen(datos)+1);
	strcpy(datosAF->worker.nodo,datos);
	datos += strlen(datos)+1;

	datosAF->worker.ip = malloc(strlen(datos)+1);
	strcpy(datosAF->worker.ip,datos);
	datos += strlen(datos)+1;


	datosAF->worker.puerto = ((int*)(datos))[0];

	printf("Se envia a Worker:: %d :: %s \n",datosAF->worker.puerto,datosAF->worker.ip);

	pthread_mutex_lock(&mutex_log);
	char* forLog = string_from_format("Deserializado Exitoso: Puerto: %d Nodo: %s RutaArchivoFinal: %s",datosAF->worker.puerto,datosAF->worker.nodo,datosAF->rutaArchivoF);
	escribir_log("log_master","master",forLog,"info");
	free(forLog);
	pthread_mutex_unlock(&mutex_log);

	pthread_mutex_lock(&mutex_log);
	escribir_log("log_master","master","Se envia el Almacenado Final a Worker","info");
	pthread_mutex_unlock(&mutex_log);

	int socketWorker = ConectarAServidor(datosAF->worker.puerto,datosAF->worker.ip, WORKER, MASTER, RecibirHandshake);
	serializacionAFyEnvio(datosAF,socketWorker);
	//Espero conf de worker

	if(RecibirPaqueteCliente(socketWorker, MASTER, paquete)<= 0) {
		perror("Error: No se recibio validacion del worker en Almacenado Final"); //manejo desconexion

		pthread_mutex_lock(&mutex_log);
		escribir_log("log_master","master","Fallo recibir validacion del Worker en el caso de Almacenado Final, se envia a Yama para replanificar","error");
		pthread_mutex_unlock(&mutex_log);

	}
	else {
		if(paquete->header.tipoMensaje == VALIDACIONWORKER){
			if((bool*)paquete->Payload) {
				pthread_mutex_lock(&mutex_log);
				escribir_log("log_master","master","Se realizo exitosamente el Almacenado Final, se envia notificacion a Yama","info");
				pthread_mutex_unlock(&mutex_log);

			}
			else {
				perror("Worker notifico que hubo una falla"); //no contemplado en Worker
			}
		}
	}
	/*
	pthread_mutex_lock(&mutex_log);
	escribir_log("log_master","master","Fallo recibir validacion del Worker en el caso de Transformacion, se envia a Yama para replanificar","error");
	pthread_mutex_unlock(&mutex_log);*/


	pthread_mutex_lock(&mutex_log);
	escribir_log("log_master","master","Se realizo exitosamente el Almacenado Final, se envia notificacion a Yama","info");
	pthread_mutex_unlock(&mutex_log);
	//Mando conf a YAMA

}

void inicializarSemaforos(){
	pthread_mutex_init(&mutex_Tfallos, NULL);
	pthread_mutex_init(&mutex_TActuales, NULL);
	pthread_mutex_init(&mutex_TRealizadas, NULL);

	pthread_mutex_init(&mutex_RLfallos,NULL);
	pthread_mutex_init(&mutex_RLActuales,NULL);
	pthread_mutex_init(&mutex_RLRealizadas,NULL);

	pthread_mutex_init(&mutex_RGfallos,NULL);
	pthread_mutex_init(&mutex_RGRealizadas,NULL);

	pthread_mutex_init(&mutex_RLfallos, NULL);
	pthread_mutex_init(&mutex_RGfallos, NULL);
	pthread_mutex_init(&mutex_listaJobs,NULL);

	pthread_mutex_init(&mutex_log,NULL);
}

int main(int argc, char* argv[]){
	time_t tiempoInicioPrograma = time(0);
	inicializarSemaforos();
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();
	listaHilos = list_create();
	duracionesJob = list_create();
	bool finalizado = false;
	char* programaTrans = argv[1];
	char* programaReduc = argv[2];
	char* archivoParaYAMA = argv[3];
	char* archivoFinal = argv[4];

	char* forLog = string_from_format("Se comienza el Master con parametros: %s :: %s :: %s :: %s",programaTrans,programaReduc,archivoParaYAMA,archivoFinal);
	escribir_log("log_master","master",forLog,"info");
	free(forLog);
	socketYAMA = ConectarAServidor(YAMA_PUERTO, YAMA_IP, YAMA, MASTER, RecibirHandshake);

	void* datos = malloc(strlen(archivoParaYAMA)+strlen(archivoFinal)+2);
	strcpy(datos,archivoParaYAMA);
	datos += strlen(archivoParaYAMA)+1;
	strcpy(datos,archivoFinal);
	datos += strlen(archivoFinal)+1;
	datos-= strlen(archivoParaYAMA)+strlen(archivoFinal)+2;

	escribir_log("log_master","master","Se envia solicitud de transformacion a Yama","info");

	if(!(EnviarDatosTipo(socketYAMA, MASTER ,datos, strlen(archivoParaYAMA)+strlen(archivoFinal)+2, SOLICITUDTRANSFORMACION))) perror("Error al enviar el archivoParaYAMA a YAMA");
	free(datos);
	while(finalizado!=true){
		Paquete* paquete = malloc(sizeof(Paquete));
		if (RecibirPaqueteCliente(socketYAMA, MASTER, paquete)<=0) {
			perror("Error al recibir respuesta de YAMA");
			exit(-1);
		}
		{
			switch(paquete->header.tipoMensaje){
			case SOLICITUDTRANSFORMACION:{
				realizarTransformacion(paquete, programaTrans);
				break;
			}
			case SOLICITUDREDUCCIONLOCAL:{
				realizarReduccionLocal(paquete, programaReduc);
				break;
			}
			case SOLICITUDREDUCCIONGLOBAL:{
				realizarReduccionGlobal(paquete, programaReduc);
				break;
			}
			case SOLICITUDALMACENADOFINAL:{
				realizarAlmacenamientoFinal(paquete,archivoFinal);
				finalizado = metricas(tiempoInicioPrograma);
				break;
			}
			}
		}
	}
	list_destroy_and_destroy_elements(listaHilos, LAMBDA(void _(void* elem) {
		pthread_join(((hiloWorker*)elem)->hilo, NULL);
		free(elem); }));
	list_destroy_and_destroy_elements(duracionesJob,free);
	free(YAMA_IP);

	return 0;
}

