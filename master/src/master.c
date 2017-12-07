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

pthread_mutex_t mutex_listaJobs;

pthread_mutex_t mutex_RLfallos;
pthread_mutex_t mutex_RGfallos;



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
	if(!(EnviarDatosTipo(socketWorker,MASTER,datos,size,REDGLOBALWORKER)))perror("Error al enviar datosRG al Worker");
}

void serializacionAFyEnvio(char* resultRG, char* rutaArchivoF, int socketWorker){//TODO terminar el camino de AF
// tamString - ResultRG - tamString - rutaArchivoF
	int mov = 0;
	int sizeAux;
	int size = sizeof(int)+strlen(resultRG)+1+sizeof(int);
	void* datos = malloc(size);

	sizeAux = strlen(resultRG)+1;
	memcpy(datos+mov,&sizeAux,sizeof(int));
	mov += sizeof(int);
	memcpy(datos+mov,resultRG,strlen(resultRG)+1);
	mov += strlen(resultRG)+1;
	sizeAux = strlen(rutaArchivoF)+1;
	memcpy(datos+mov,&sizeAux,sizeof(int));
	mov += sizeof(int);
	memcpy(datos+mov,rutaArchivoF,strlen(rutaArchivoF)+1);


	//if(!(EnviarDatosTipo(socketWorker,MASTER,datos,size,FINALIZAR))) perror("Error al enviar datosAF al worker");


}

void serializacionDeRTAyEnvio(rtaEstado* resultado,int socketYAMA){
	// bllque - idJob - strLen - noodo - bool
	int mov = 0;
	bool boolAux;
	int size = sizeof(int)*2 + strlen(resultado->nodo)+1+sizeof(bool);
	void* datos = malloc(size);

	memcpy(datos+mov,resultado->bloque,sizeof(int));
	mov += sizeof(int);
	memcpy(datos+mov,resultado->idJob,sizeof(int));
	mov += sizeof(int);
	strcpy(datos+mov,resultado->nodo);
	mov += strlen(resultado->nodo)+1;
	boolAux = resultado->exito;
	memcpy(datos+mov,&boolAux,sizeof(bool));
	mov += sizeof(bool);

	if(!(EnviarDatosTipo(socketYAMA, MASTER ,datos, size, VALIDACIONYAMA))) perror("No se puedo enviar mensaje de que Worker fallo a YAMA en el caso de T");
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

int obtenerIdJobDeRuta(char* rutaTemporal){
	// /tmp/jXnybz
	// 0123456
	int resultado = rutaTemporal[6]-'0';
	return resultado;
}


void accionHilo(void* solicitud){
	Paquete* paquete = malloc(sizeof(Paquete));
	switch (((nodoT*)solicitud)->header){

	case TRANSFORMACION:{ //TODO recibir de YAMA y deserializar, arreglar los manejo de errores con mismo formato que tranfs.
		duracionJob* duracionDelJob = malloc(sizeof(duracionJob));
		duracionDelJob->header = TRANSFORMACION;
		time_t inicio = time(0);
		pthread_mutex_lock(&mutex_TActuales);
		contTActuales++;
		if(contTActuales>contTMaxP) contTMaxP = contTActuales;
		pthread_mutex_unlock(&mutex_TActuales);

		nodoT* datosT = malloc(sizeof(nodoT));
		datosT->programaT = malloc(strlen(((nodoT*)solicitud)->programaT));
		datosT->archivoTemporal = malloc(strlen(((nodoT*)solicitud)->archivoTemporal));
		*datosT = *(nodoT*)solicitud;

		solicitud=NULL;

		rtaEstado* resultado = malloc(sizeof(resultado));
		resultado->bloque = datosT->bloque; //TODO TRABAJANDO
		resultado->nodo = malloc(strlen(datosT->programaT)+1);
		strcpy(resultado->nodo,datosT->programaT);
		resultado->exito = true;
		resultado->idJob = obtenerIdJobDeRuta(datosT->archivoTemporal);

		int socketWorker = ConectarAServidor(datosT->worker.puerto, datosT->worker.ip, WORKER, MASTER, RecibirHandshake);
		serializacionTyEnvio(datosT,socketWorker);

		if(RecibirPaqueteCliente(socketWorker, MASTER, paquete)<= 0) {
			perror("Error: No se recibio validacion del worker en T"); //manejo desconexion

			resultado->exito = false;
			serializacionDeRTAyEnvio(resultado,socketYAMA);

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

					pthread_mutex_lock(&mutex_listaJobs);
					list_add(duracionesJob,duracionDelJob);
					pthread_mutex_unlock(&mutex_listaJobs);
					pthread_mutex_lock(&mutex_TActuales);
					contTActuales--;
					pthread_mutex_unlock(&mutex_TActuales);
					pthread_mutex_lock(&mutex_TRealizadas);
					contTRealizadas++;
					pthread_mutex_unlock(&mutex_TRealizadas);
					//Aca de deberia serializar y enviar resultado
					if(!(EnviarDatosTipo(socketYAMA, MASTER ,resultado, sizeof(bool), VALIDACIONYAMA))) perror("No se puedo enviar la verificacion a YAMA en el caso de T");
				}
				else {
					perror("Worker notifico que hubo una falla"); //no contemplado en Worker
				}
			}
			else {
				perror("Error en el header, se esperaba VALIDACIONWORKER");//Error nuestro, no contemplado para informar a YAMA
			}
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

		if(RecibirPaqueteCliente(socketWorker, MASTER, paquete)<0){
			perror("Error: No se recibio validacion del worker en RL");//manejo desconexion

			bool* ok = false;
			if(!(EnviarDatosTipo(socketYAMA, MASTER ,ok, sizeof(bool), VALIDACIONYAMA))) perror("No se puedo enviar mensaje de que Worker fallo a YAMA en el caso de RL");
			pthread_mutex_lock(&mutex_RLfallos);
			contRLfallos++;
			pthread_mutex_unlock(&mutex_RLfallos);

		}
		else {
		if(paquete->header.tipoMensaje == VALIDACIONWORKER){
			if((bool*)paquete->Payload) {
				bool* ok = true;
				if(!(EnviarDatosTipo(socketYAMA, MASTER ,ok, sizeof(bool), VALIDACIONYAMA))) perror("No se puedo enviar mensaje de que Worker fallo a YAMA en el caso de RL");
			}
			else {
				perror("No se puedo realizar la operacion de Reduccion Local"); //Este caso  no se contempla en Worker, no se como puede ocurrir
			}
		}
		else{
			perror("Error en el header, se esperaba VALIDACIONWORKER"); //Este caso ocurriria por error de programacion, no se contempla para notificar a YAMA
		}
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

		if(RecibirPaqueteCliente(socketWorker, MASTER, paquete)<0){
			perror("Error: no se recibio validacion del worker en RG");//manejo desconexion

			bool* ok = false;
			if(!(EnviarDatosTipo(socketYAMA, MASTER ,ok, sizeof(bool), VALIDACIONYAMA))) perror("No se puedo enviar mensaje de que Worker fallo a YAMA en el caso de RG");
			pthread_mutex_lock(&mutex_RGfallos);
			contRGfallos++;
			pthread_mutex_unlock(&mutex_RGfallos);

		} else {
			if(paquete->header.tipoMensaje == VALIDACIONWORKER){
				if((bool*)paquete->Payload) {
					bool* ok = true;
					EnviarDatosTipo(socketYAMA, MASTER ,ok, sizeof(bool), VALIDACIONYAMA);
				}
				else {
					perror("No se puedo realizar la operacion de Reduccion Global");
				}
			}else{
				perror("Error en el header, se esperaba VALIDACIONWORKER");
			}
		}
		break;
	}

	}
}



void realizarTransformacion(Paquete* paquete, char* programaT){ //TODO deserializar el resto

	nodoT* datosParaT = malloc(sizeof(nodoT));
	void* datos = paquete->Payload;
	//numero de bloque
	datosParaT->bloque = ((uint32_t*)datos)[0];
	//cantbytesocupados
	datosParaT->bytesOcupados = ((uint32_t*)datos)[1];
	//puerto worker
	datosParaT->worker.puerto = ((uint32_t*)datos)[2];
	datos+=sizeof(uint32_t) * 3;
	//ip worker
	datosParaT->worker.ip = string_new();
	strcpy(datosParaT->worker.ip, datos);
	datos += strlen(datos)+1;
	//nombre nodo del worker
	datosParaT->worker.nodo = string_new();
	strcpy(datosParaT->worker.nodo, datos);
	datos += strlen(datos)+1;
	//ruta temporal
	datosParaT->archivoTemporal = string_new();
	strcpy(datosParaT->archivoTemporal, datos);
	datos += strlen(datos)+1;
	datos -= paquete->header.tamPayload;

	hiloWorker* itemNuevo = malloc(sizeof(hiloWorker));
	itemNuevo->worker = datosParaT->worker;

	datosParaT->programaT = malloc(strlen(programaT)+1);
	strcpy(datosParaT->programaT,programaT);
	datosParaT->header = TRANSFORMACION;

	pthread_create(&(itemNuevo->hilo),NULL,(void*)accionHilo,datosParaT);
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

}
 void realizarAlmacenamientoFinal(Paquete* paquete,char* rutaArchivoF){ //TODO terminar el camino de AF
	 //YAMA me da IP y Puerto de Worker
	 char* ipWorker;
	 int puertoWorker;
	 //YAMA me da nombre de archivo resultado de la reduccion Global
	 char* resultRG;
	 //Yo tengo la ruta final del archivo


	 int socketWorker = ConectarAServidor(puertoWorker,ipWorker, WORKER, MASTER, RecibirHandshake);
	 serializacionAFyEnvio(resultRG,rutaArchivoF,socketWorker);
	 //Espero conf de worker
	 //Mando conf a YAMA

 }

 void inicializarSemaforos(){
	 pthread_mutex_init(&mutex_Tfallos, NULL);
	 pthread_mutex_init(&mutex_TActuales, NULL);
	 pthread_mutex_init(&mutex_TRealizadas, NULL);
	 pthread_mutex_init(&mutex_RLfallos, NULL);
	 pthread_mutex_init(&mutex_RGfallos, NULL);
	 pthread_mutex_init(&mutex_listaJobs,NULL);
 }

int main(int argc, char* argv[]){
	time_t tiempoInicioPrograma = time(0);
	clock_t tiempoSumaRL = clock();
	clock_t tiempoSumaRG = clock();
	inicializarSemaforos();
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();
	listaHilos = list_create();
	duracionesJob = list_create();
	bool finalizado = false;
	char* programaTrans = argv[2];
	char* programaReduc = argv[3];
	char* archivoParaYAMA = argv[4];
	char* archivoFinal = argv[5];


	socketYAMA = ConectarAServidor(YAMA_PUERTO, YAMA_IP, YAMA, MASTER, RecibirHandshake);

	if(!(EnviarDatosTipo(socketYAMA, MASTER ,archivoParaYAMA, strlen(archivoParaYAMA)+1, SOLICITUDTRANSFORMACION))) perror("Error al enviar el archivoParaYAMA a YAMA");
	Paquete* paquete = malloc(sizeof(Paquete));
	while(finalizado!=true){
		if (RecibirPaqueteCliente(socketYAMA, MASTER, paquete)<=0) perror("Error al recibir respuesta de YAMA");
		{
			switch(paquete->header.tipoMensaje){
			case SOLICITUDTRANSFORMACION:{
				realizarTransformacion(paquete, programaTrans);
				break;
			}
			case REDLOCALWORKER:{
				contRLActuales++;
				if(contRLActuales>contRLMaxP) contRLMaxP = contRLActuales;
				clock_t tiempoInicio = clock();
				realizarReduccionLocal(paquete, programaReduc);
				tiempoSumaRL += clock() - tiempoInicio;
				contRLActuales--;
				contRLRealizadas++;
				break;
			}
			case REDGLOBALWORKER:{
				clock_t tiempoInicio = clock();
				realizarReduccionGlobal(paquete, programaReduc);
				tiempoSumaRG += clock() - tiempoInicio;
				contRGRealizadas++;
				break;
			}
			//case FINALIZAR:

				realizarAlmacenamientoFinal(paquete,archivoFinal);

				//Aca deberia haber pthread join del AF

			 	 double tiempoDemoradoJobCompleto = difftime(time(0),tiempoInicioPrograma);

			 	 printf("\nSe procede a mostrar las metricas del Job:\n\n");
			 	 printf("1.Tiempo total de ejecucion del Job (Minutos): %f\n", tiempoDemoradoJobCompleto);

			 	 double tiempoPromedioT;
			 	 double tiempoPromedioRLMINUTOS;
			 	 double tiempoPromedioRGMINUTOS;

			 	 if(contTRealizadas!=0){
			 		bool esTransformacion(duracionJob* elem){
			 					return (elem->header==TRANSFORMACION);
			 				}
			 		t_list* listaTransformacion = list_create();
			 		listaTransformacion = list_filter(duracionesJob,(void*)esTransformacion);
			 		double tiempoTotalT = sumListaDuraciones(listaTransformacion);

			 		 tiempoPromedioT = tiempoTotalT/contTRealizadas;
				 }
				 else {
				 tiempoPromedioT = 0;
				 }

				  if(contRLRealizadas!=0){
					  tiempoPromedioRLMINUTOS = ((((double)tiempoSumaRL)/ CLOCKS_PER_SEC)/60)/contRLRealizadas;
				 }
				 else {
					 tiempoPromedioRLMINUTOS = 0;
				 }

				  if(contTRealizadas!=0){
					  tiempoPromedioRGMINUTOS = ((((double)tiempoSumaRG)/ CLOCKS_PER_SEC)/60)/contRGRealizadas;
				 }
				 else {
					 tiempoPromedioRGMINUTOS = 0;
				 }

			 	 printf("2.Tiempo promedio de ejecucion de cada etapa principal del Job:\n Transformacion: %f\n Reduccion Local: %f\n Reduccion Global:%f\n\n",tiempoPromedioT,tiempoPromedioRLMINUTOS,tiempoPromedioRGMINUTOS );
			 	 printf("3.Cantidad maxima de tareas de Transformacion y Reduccion Local ejecutadas de forma paralela \n Transformacion: %d \n Reduccion Local %d\n\n",contTMaxP,contRLMaxP);
			 	 printf("4.Cantidad total de tareas realizadas en cada etapa principal del Job:\n Transformacion: %d\n Reduccion Local: %d\n Reduccion Global: %d\n\n",contTRealizadas,contRLRealizadas,contRGRealizadas);
			 	 printf("5. Cantidad de fallos obtenidos en la realizacion de un Job:\n Transformacion: %d\n Reduccion Local: %d\n Reduccion Global: %d\n\n",contTfallos,contRLfallos,contRGfallos);
			 	 finalizado = true;
			}
		}
	}

	list_destroy_and_destroy_elements(listaHilos, LAMBDA(void _(void* elem) {
		pthread_join(((hiloWorker*)elem)->hilo, NULL);
		free(elem); }));
	list_destroy_and_destroy_elements(duracionesJob,free);

	return 0;
}
