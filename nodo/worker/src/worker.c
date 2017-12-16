#include "sockets.h"

#define TAMANIODEBLOQUE 1024*1024
#define BUFFERSIZE 1024*1024

char *IP_NODO, *IP_FILESYSTEM,*NOMBRE_NODO,*RUTA_DATABIN;
int PUERTO_FILESYSTEM, PUERTO_WORKER, PUERTO_DATANODE;
t_list* listaDeProcesos;
bool end;

nodoRG* workerEncargado;

pthread_mutex_t mutex_mmap;
pthread_mutex_t mutex_archivo;

void obtenerValoresArchivoConfiguracion(char* valor) {
	char* a = string_from_format("/home/utnso/workspace/tp-2017-2c-Yama-Que-Yama/nodo/nodo%sCFG.txt", valor);
	t_config* arch = config_create(a);
	//t_config* arch = config_create("/home/utnso/workspace/tp-2017-2c-Yama-Que-Yama/nodo/nodoCFG.txt");
	IP_FILESYSTEM = string_duplicate(config_get_string_value(arch, "IP_FILESYSTEM"));
	PUERTO_FILESYSTEM = config_get_int_value(arch, "PUERTO_FILESYSTEM");
	NOMBRE_NODO = string_duplicate(config_get_string_value(arch, "NOMBRE_NODO"));
	IP_NODO = string_duplicate(config_get_string_value(arch, "IP_NODO"));
	PUERTO_WORKER = config_get_int_value(arch, "PUERTO_WORKER");
	RUTA_DATABIN = string_duplicate(config_get_string_value(arch, "RUTA_DATABIN"));
	config_destroy(arch);
}

void realizarReduccionGlobal(solicitudRG* data, nodoRG* encargado){
	//char* strArchivosTemporales = listaAstringRG(data->nodos);
	chmod(data->programaR, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH);
	char* strToSys = string_from_format("cat %s | /home/utnso/Escritorio%s > %s",encargado->archTempRL, data->programaR, data->archRG);
	system(strToSys);
	return;
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


char* listAsString(t_list* lista){
	char* resultado = string_new();
	int tamLista = list_size(lista);
	int i;
	for(i=0;i<tamLista-1;i++){
		string_append(&resultado,(char*)list_get(lista,i));
		string_append(&resultado, " ");
	}
	string_append(&resultado,(char*) list_get(lista,tamLista-1));
	return resultado;
}

char* listaAstringRG(t_list* lista){
	char* resultado = string_new();
	int tamLista = list_size(lista);
	int i;
	for(i=0;i<tamLista;i++){
		nodoRG* elem = list_get(lista,i);
		string_append(&resultado,elem->archTempRL);
		string_append(&resultado," ");
	}
	nodoRG* elem = list_get(lista, tamLista-1);
	string_append(&resultado,elem->archTempRL);
	return resultado;
}

nodoT* deserializacionT(void* payload){
	//Bloque - BytesOcupados - tamStr - programaT - tamStr - archivoTemp
	int mov = 0;
	int aux;
	nodoT* datosT = malloc(sizeof(nodoT));

	memcpy(&datosT->bloque,payload+mov,sizeof(int));
	mov += sizeof(int);
	memcpy(&datosT->bytesOcupados,payload+mov,sizeof(int));
	mov += sizeof(int);
	memcpy(&aux,payload+mov,sizeof(int)); //Aux contiene el tamanio del string de programaT
	mov += sizeof(int);
	datosT->programaT = malloc(aux);
	memcpy(datosT->programaT,payload+mov,aux);
	mov += aux;
	memcpy(&aux,payload+mov,sizeof(int)); //Aux contiene el tamanio del string de archivoTemp
	mov += sizeof(int);
	datosT->archivoTemporal = malloc(aux);
	memcpy(datosT->archivoTemporal,payload+mov,aux);

	free(payload);

	return datosT;
}

nodoRL* deserializacionRL(void* payload){
	//tamStr - programaR - tamStr - archivoTemp - tamList - (tamStr - ArchivoTemp)xtamList
	int mov = 0;
	int tamList;
	int aux;
	nodoRL* datosRL = malloc(sizeof(nodoRL));

	memcpy(&aux,payload+mov,sizeof(int));
	mov += sizeof(int);
	datosRL->programaR = malloc(aux); //Aux contiene el tamanio de string de programaR
	memcpy(datosRL->programaR,payload+mov,aux);
	mov += aux;
	memcpy(&aux,payload+mov,sizeof(int)); //Aux contiene el tamanio de string de archivoTemporal
	mov += sizeof(int);
	datosRL->archivoTemporal = malloc(aux);
	memcpy(datosRL->archivoTemporal,payload+mov,aux);
	mov += aux;
	memcpy(&tamList,payload+mov,sizeof(int));
	mov += sizeof(int);

	datosRL->listaArchivosTemporales = list_create();
	int i;
	for(i=0;i<tamList;i++){
		memcpy(&aux,payload+mov,sizeof(int));
		mov += sizeof(int);
		char* elemento = malloc(aux);
		memcpy(elemento,payload+mov,aux);
		mov += aux;
		list_add(datosRL->listaArchivosTemporales,elemento);
	}

	free(payload);
	return datosRL;

}

solicitudRG* deserializacionRG(void* payload){
	//tamStr - archRG - tamStr - programaRG - tamList - (tamStr - archTempRL - bool - tamStr -nodo - tamStr -ip - puerto)xtamList
	int mov = 0;
	int aux;
	solicitudRG* datosRG = malloc(sizeof(solicitudRG));

	memcpy(&aux,payload+mov,sizeof(int));
	mov += sizeof(int);
	datosRG->archRG = malloc(aux); //Aux contiene el tamanio de ArchRG
	memcpy(datosRG->archRG,payload+mov,aux);
	mov += aux;
	memcpy(&aux,payload+mov,sizeof(int));
	mov += sizeof(int);
	datosRG->programaR = malloc(aux); //Aux contiene el tamanio de programaRG
	memcpy(datosRG->programaR,payload+mov,aux);
	mov += aux;
	memcpy(&(datosRG->cantNodos),payload+mov,sizeof(int));
	mov += sizeof(int);

	datosRG->nodos = list_create();
	int i;
	for(i=0;i<(datosRG->cantNodos);i++){
		nodoRG* nodoAgregar = malloc(sizeof(nodoRG));

		memcpy(&aux,payload+mov,sizeof(int));
		mov += sizeof(int);

		nodoAgregar->archTempRL = malloc(aux); //Aux contiene tamanio ArchTempRL
		memcpy(nodoAgregar->archTempRL,payload+mov,aux);
		mov += aux;

		memcpy(&nodoAgregar->encargado,payload+mov,sizeof(bool));
		mov += sizeof(bool);

		memcpy(&aux,payload+mov,sizeof(int));
		mov += sizeof(int);

		nodoAgregar->worker.nodo = malloc(aux); //Aux contiene tamanio nodo
		memcpy(nodoAgregar->worker.nodo,payload+mov,aux);
		mov += aux;

		memcpy(&aux,payload+mov,sizeof(int));
		mov += sizeof(int);

		nodoAgregar->worker.ip = malloc(aux); //Aux contiene tamanio ip
		memcpy(nodoAgregar->worker.ip,payload+mov,aux);
		mov += aux;

		memcpy(&(nodoAgregar->worker.puerto),payload+mov,sizeof(int));
		mov += sizeof(int);

		list_add(datosRG->nodos,nodoAgregar);

	}

	free(payload);
	return datosRG;
}

datoAF* deserializacionAF(void* payload){ //TODO terminar camino de AF
	// tamString - ResultRG - tamString - rutaArchivoF
	int mov = 0;
	int aux;
	datoAF* datos = malloc(sizeof(datoAF));

	memcpy(&aux,payload+mov,sizeof(int));
	mov += sizeof(int);
	datos->resultRG = malloc(aux);
	memcpy(datos->resultRG,payload+mov,aux);
	mov += aux;
	memcpy(&aux,payload+mov,sizeof(int));
	mov += sizeof(int);
	datos->rutaArchivoF = malloc(aux);
	memcpy(datos->rutaArchivoF,payload+mov,aux);

	free(payload);
	return datos;
}


void serializacionAFyEnvioFS(nodoAF* nodo,int socketFS, int tamArchivo){
	// tamStr + buffer + rutaARchivoF

	int mov = 0;
	int sizeAux;
	int size = sizeof(int)+tamArchivo+strlen(nodo->ruta)+1;
	void* datos = malloc(size);

	sizeAux = tamArchivo;
	memcpy(datos+mov,&sizeAux,sizeof(int));
	mov += sizeof(int);
	memcpy(datos+mov,nodo->buffer,tamArchivo);
	mov += tamArchivo;
	memcpy(datos+mov,nodo->ruta,strlen(nodo->ruta)+1);

	if(!(EnviarDatosTipo(socketFS,WORKER,datos,size,ESDATOS))) perror("Error al datos al FS para almacenado final");
	free(datos);

}

void* getBloque(int numeroDeBloque,int tamanio){
	struct stat st;
	int tamanioDataBin;
	if(stat(RUTA_DATABIN, &st) == -1) printf("Error en Stat()\n");
	else {
		tamanioDataBin = st.st_size;
	}
	int unFileDescriptor;
	void* punteroDataBin;
	void* datos_a_enviar=malloc(tamanio);
	if ((unFileDescriptor = open(RUTA_DATABIN, O_RDONLY)) == -1) {
		char* error = string_from_format("ERROR en el Open() de getBloque() :: error: %d\n",unFileDescriptor);
		printf("%s",error);
	}else{
		if ((punteroDataBin = mmap ( NULL , tamanioDataBin, PROT_READ , MAP_SHARED, unFileDescriptor , 0)) == (caddr_t)(-1)){
			char* error = string_from_format("ERROR en el mmap() de getBloque() :: error: %d\n",punteroDataBin);
			printf("%s",error);
		}else{
			memmove(datos_a_enviar,punteroDataBin+(numeroDeBloque*TAMANIODEBLOQUE),tamanio);
		}
	}
	int unmap=munmap(punteroDataBin,tamanioDataBin);
	if(unmap==-1){
		char* error = string_from_format("ERROR en el unmap() de getBloque() :: error: %d\n",unmap);
		printf("%s",error);
		fflush(stdout);
	}else{
		return datos_a_enviar;
	}
}

int obtenerBloqueDeRuta(char* rutaTemporal){

	// /tmp/jXnybz
	// 0123456
	char** array  = string_split(rutaTemporal, "b");
	int resultado = atoi(array[1]);
	return resultado;
}

void realizarTransformacion(nodoT* data){
	/*FILE* dataBin = fopen(RUTA_DATABIN,"r");
	char* bufferTexto = malloc(data->bytesOcupados);
	int mov = data->bloque * TAMANIODEBLOQUE;

	fseek(dataBin,mov,SEEK_SET);
	fread(bufferTexto,data->bytesOcupados,1,dataBin);
	fclose(dataBin);*/

	pthread_mutex_lock(&mutex_mmap);
	printf("Se comienza mmap\n");
	char* bufferTexto = calloc((data->bytesOcupados), sizeof(char));

	memmove(bufferTexto,((char*)getBloque(data->bloque,data->bytesOcupados)),(data->bytesOcupados));
	pthread_mutex_unlock(&mutex_mmap);

	int bloqueAtrabajar = obtenerBloqueDeRuta(data->archivoTemporal);
	printf("Se comienza transformacion para bloque :: %d bytes::%d\n",bloqueAtrabajar,data->bytesOcupados);
	char* pathPrograma = string_from_format("/home/utnso/Escritorio%s",data->programaT);
	char* path = string_from_format("/home/utnso/Escritorio/%s/temporales/tmpWorkerN%d",NOMBRE_NODO,bloqueAtrabajar);
	FILE* tempFD = fopen(path,"w");
	fwrite(bufferTexto,sizeof(char),data->bytesOcupados,tempFD);
	fclose(tempFD);


	char* strToSys = string_from_format("cat %s | %s | sort > %s",path,pathPrograma,data->archivoTemporal); //puede ser que sort necesite -d


	//char* bufferTexto = getBloque(data->bloque,data->bytesOcupados);
	//char* bufferReal = strcat(bufferTexto,"\n"); en caso de necesitar esto, debo hacer +1 al malloc
	chmod(pathPrograma, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH);
	//char* strToSys = string_from_format("printf \"%s\" | .%s | sort -d - > %s", bufferTexto,data->programaT,data->archivoTemporal);
	int a = system(strToSys);
	printf("%d\n",a);
	free(pathPrograma);
	free(bufferTexto);
	free(strToSys);
	free(path);
	//fclose(dataBin);
	return;
}

void realizarReduccionLocal(nodoRL* data){
	char* strArchTemp = listAsString(data->listaArchivosTemporales);
	char* pathPrograma = string_from_format("/home/utnso/Escritorio%s",data->programaR);


	chmod(pathPrograma, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH);
	char* strToSys = string_from_format("cat %s | sort | %s > %s",strArchTemp, pathPrograma, data->archivoTemporal);
	system(strToSys);
	printf("Fin RL\n");

	free(strArchTemp);
	free(strToSys);
	free(pathPrograma);
	return;
}



void accionPadre(void* socketMaster){
}

void accionHijo(void* socketM){
	bool boolAux = false;
	int socketMaster = (int) socketM;
	Paquete* paquete = malloc(sizeof(Paquete));

	if(RecibirPaqueteServidor(socketMaster, WORKER, paquete)<0) perror("Error: No se recibieron los datos de Master");
	if(RecibirPaqueteCliente(socketMaster, WORKER, paquete)<0) perror("Error: No se recibieron los datos de Master");
	switch((paquete->header.emisor)[0]){
	case 'M': {
		switch(paquete->header.tipoMensaje){
		case TRANSFWORKER:{
			nodoT* datosT = deserializacionT(paquete->Payload);
			realizarTransformacion(datosT);
			boolAux =true;
			if(!(EnviarDatosTipo(socketMaster,WORKER,&boolAux,sizeof(bool),VALIDACIONWORKER))) perror("Error al enviar OK a master en etapa de T");
			break;
		}
		case REDLOCALWORKER:{
			nodoRL* datosRL = deserializacionRL(paquete->Payload);
			realizarReduccionLocal(datosRL);
			boolAux =true;
			if(!(EnviarDatosTipo(socketMaster,WORKER,&boolAux,sizeof(bool),VALIDACIONWORKER))) perror("Error al enviar OK a master en etapa de T");
			break;
		}
		case REDGLOBALWORKER:{
			solicitudRG* datosRG = deserializacionRG(paquete->Payload);

			//FUNCIONES PARA LISTAS
			bool obtenerEncargado(nodoRG* elemento){
				return elemento->encargado;
			}
			bool obtenerActual(nodoRG* elemento){
				if (!strcmp(elemento->worker.nodo,NOMBRE_NODO)) return true;
				else false;
			}
			//FUNCIONES PARA LISTAS

			workerEncargado = list_find(datosRG->nodos,(void*)obtenerEncargado);

			printf("Soy el encargado\n");
			int i=0;
			int tamList = list_size(datosRG->nodos);
			printf("Encargado:: Puerto:%d Nodo:%s \n",workerEncargado->worker.puerto,workerEncargado->worker.nodo);



			void funcHijo(nodoRG* nodo){
				printf("Tratando de conectar a Port::%d\n",nodo->worker.puerto);
				fflush(stdout);
				int socketWorker = ConectarAServidor(nodo->worker.puerto,nodo->worker.ip,WORKER,WORKER,RecibirHandshake);
				char* buffer = malloc(strlen(nodo->archTempRL)+1);
				strcpy(buffer,nodo->archTempRL);
				printf("Se envian datos al worker conectado\n");
				if(!(EnviarDatosTipo(socketWorker, WORKER ,buffer, strlen(nodo->archTempRL)+1, SOLICITUDARCHTEMP))) perror("Error al enviar archRLTemp al worker encargado");
				free(buffer);

				Paquete* paquetito = malloc(sizeof(Paquete));

				RecibirPaqueteCliente(socketWorker,WORKER,paquetito);

				switch(paquetito->header.tipoMensaje){

				case ARCHIVOTEMPRL:{
					void* datos = paquetito->Payload;
					buffer = malloc(paquetito->header.tamPayload);
					strcpy(buffer,datos);
					pthread_mutex_lock(&mutex_archivo);
					//char* strToSys = string_from_format("echo \"%s\" | sort - %s -o %s",buffer,workerEncargado->archTempRL,workerEncargado->archTempRL);
					FILE* fd = txt_open_for_append(workerEncargado->archTempRL);
					txt_write_in_file(fd,buffer);
					txt_close_file(fd);
					char* strToSys = string_from_format("sort %s -o %s",workerEncargado->archTempRL,workerEncargado->archTempRL);
					system(strToSys);
					pthread_mutex_unlock(&mutex_archivo);

					free(buffer);
					free(strToSys);
					free(datos);
					free(paquetito);
					break;
				}
				}
			}


			pid_t pids[tamList];
			pid_t wpid;
			int status = 0;
			for(i=0;i<tamList;i++){
				nodoRG* nodo = list_get(datosRG->nodos,i);
				if(!(strcmp(nodo->worker.nodo,workerEncargado->worker.nodo)==0)){
					if((pids[i] = fork())<0){
						perror("fork");
					}
					else if (pids[i]==0){
						funcHijo(nodo);
						exit(0);
					}
				}
			}
			while ((wpid = wait(&status)) > 0);
			realizarReduccionGlobal(datosRG, workerEncargado);
			boolAux =true;
			if(!(EnviarDatosTipo(socketMaster,WORKER,&boolAux,sizeof(bool),VALIDACIONWORKER))) perror("Error al enviar OK a master en etapa de T");
			free(paquete);
			break;


		}

		case SOLICITUDALMACENADOFINAL:{ //FINALIZAR TODO terminar camino de AF
			datoAF* datosAF = deserializacionAF(paquete->Payload);
			nodoAF* nodoAF = malloc(sizeof(nodoAF));

			int archTemp = open(datosAF->resultRG,O_RDONLY);
			struct stat st;
			stat(datosAF->resultRG,&st);
			int tamArchivo;
			tamArchivo = st.st_size;
			void* puntero;

			nodoAF->buffer = malloc(tamArchivo);
			if ((puntero = mmap(NULL , tamArchivo, PROT_READ , MAP_SHARED, archTemp , 0)) == (caddr_t)(-1)){
				printf("ERROR en el mmap()\n");
			}else{
				memcpy(nodoAF->buffer,puntero,tamArchivo);
			}
			munmap(puntero,tamArchivo);
			nodoAF->ruta = malloc(strlen(datosAF->rutaArchivoF)+1);
			strcpy(nodoAF->ruta,datosAF->rutaArchivoF);

			int socketFS = ConectarAServidor(PUERTO_FILESYSTEM, IP_FILESYSTEM, FILESYSTEM, WORKER, RecibirHandshake);
			serializacionAFyEnvioFS(nodoAF,socketFS,tamArchivo);
			if(RecibirPaqueteServidor(socketFS, WORKER, paquete)<=0) perror("Error: No se recibieron los datos de Master");

			/*FILE* fdRedGlobal = fopen(datosAF->resultRG,"r");
			struct stat st;
			stat(datosAF->resultRG,&st);
			nodoAF->buffer = malloc((st.st_size)+1);
			nodoAF->ruta = malloc(strlen(datosAF->rutaArchivoF)+1);
			strcpy(nodoAF->ruta,datosAF->rutaArchivoF);
			fread(nodoAF->buffer,st.st_size,1,fdRedGlobal);*/

			//Manejo la validacion de FS
			// Mando conf a Master

			free(paquete);
			free(nodoAF->buffer);
			free(nodoAF->ruta);
			free(datosAF->resultRG);
			free(datosAF->rutaArchivoF);
			free(datosAF);
			free(nodoAF);
			close(archTemp);
			break;
		}
		default:{
			perror("No se recibio un header de las etapas");
			break;
		}
		}
		break;
	}


	case 'W': {
		switch(paquete->header.tipoMensaje){
		case SOLICITUDARCHTEMP:{

			void* datosRecibidos = paquete->Payload;
			char *buffer = malloc(strlen(datosRecibidos)+1);
			strcpy(buffer,datosRecibidos);

			int archTemp = open(buffer,O_RDONLY);
			struct stat st;
			int tamArchivo;
			stat(buffer,&st);
			tamArchivo = st.st_size;
			void* puntero;

			char* bufferTexto = malloc(tamArchivo);
			if ((puntero = mmap(NULL , tamArchivo, PROT_READ , MAP_SHARED, archTemp , 0)) == (caddr_t)(-1)){
				printf("ERROR en el mmap()\n");
			}else{
				memcpy(bufferTexto,puntero,tamArchivo);
			}
			munmap(puntero,tamArchivo);
			if(!(EnviarDatosTipo(socketMaster, WORKER ,bufferTexto, st.st_size, ARCHIVOTEMPRL))) perror("Error al enviar archRLTemp al worker encargado");

			free(datosRecibidos);
			free(buffer);
			close(archTemp);
			break;
		}
		}
		break;
	}
}
}


int main(int argc, char* argv[]){
	pthread_mutex_init(&mutex_mmap, NULL);
	pthread_mutex_init(&mutex_archivo,NULL);
	obtenerValoresArchivoConfiguracion(argv[1]);
	imprimirArchivoConfiguracion();
	//socketFS = ConectarAServidor(PUERTO_FILESYSTEM, IP_FILESYSTEM, FILESYSTEM, WORKER, RecibirHandshake); Estaba rompiendo
	//if(socketFS <= 0) perror("No se pudo conectar con FS");
	ServidorConcurrenteForks(IP_NODO, PUERTO_WORKER, WORKER, &listaDeProcesos, &end, accionPadre, accionHijo);

	return 0;
}
