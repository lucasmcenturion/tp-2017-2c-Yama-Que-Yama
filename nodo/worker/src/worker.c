#include "sockets.h"

#define TAMANIODEBLOQUE 1024;
#define BUFFERSIZE 1024

char *IP_NODO, *IP_FILESYSTEM,*NOMBRE_NODO,*RUTA_DATABIN;
int PUERTO_FILESYSTEM, PUERTO_WORKER, PUERTO_DATANODE;
int socketFS;
t_list* listaDeProcesos;
bool end;
nodoRG* workerEncargado;

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

void accionSelect(Paquete* paquete, int socketFD){ //TODO
	if(!strcmp(paquete->header.emisor,WORKER)){

		if(paquete->header.tipoMensaje == ARCHIVOTEMPRL){

			char* bufferTexto = malloc(paquete->header.tamPayload);
			strcpy(bufferTexto,paquete->Payload);
			char* strToSys = string_from_format("echo \"%s\" | sort -o %s -m %s -",bufferTexto,workerEncargado->archTempRL,workerEncargado->archTempRL);
			system(strToSys);

		}
		else perror("Header incorrecto, se esperaba ARCHIVOTEMPRL");
		return;
	}
	else perror("Se conecto algo que no es WORKER en ReduccionGlobal");
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

	return datosRL;

}

solicitudRG* deserializacionRG(void* payload){
//tamStr - archRG - tamStr - programaRG - tamList - (tamStr - archTempRL - bool - tamStr -nodo - tamStr -ip - puerto)xtamList
	int mov = 0;
	int aux;
	int tamList;
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
	memcpy(&tamList,payload+mov,sizeof(int));
	mov += sizeof(int);

	datosRG->nodos = list_create();
	int i;
	for(i=0;i<tamList;i++){
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
return datosRG;

}

typedef struct{
	char* rutaArchivoF;
	char* resultRG;
} datoAF; //mover a sockets y poner packed

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

	return datos;
}

void serializacionAFyEnvioFS(char* buffer, char* rutaArchivoF,int socketFS){//bufferTexto,datosAF.rutaArchivoF,socketFS
// tamStr + buffer + tamStr + rutaARchivoF
	int mov = 0;
	int sizeAux;
	int size = sizeof(int)+strlen(buffer)+1+sizeof(int)+strlen(rutaArchivoF)+1;
	void* datos = malloc(size);

	sizeAux = strlen(buffer)+1;
	memcpy(datos+mov,&sizeAux,sizeof(int));
	mov += sizeof(int);
	memcpy(datos+mov,buffer,strlen(buffer)+1);
	mov += strlen(buffer)+1;
	sizeAux = strlen(rutaArchivoF)+1;
	memcpy(datos,&sizeAux,sizeof(int));
	mov += sizeof(int);
	memcpy(datos,rutaArchivoF,strlen(rutaArchivoF)+1);

	if(!(EnviarDatosTipo(socketFS,WORKER,datos,size,1))) perror("Error al datos al FS para almacenado final");


}

void realizarTransformacion(nodoT* data){
	FILE* dataBin = fopen(RUTA_DATABIN,"r");
	char* bufferTexto = malloc(data->bytesOcupados);
	int mov = data->bloque * TAMANIODEBLOQUE;

	fseek(dataBin,mov,SEEK_SET);
	fread(bufferTexto,data->bytesOcupados,1,dataBin);

	//char* bufferReal = strcat(bufferTexto,"\n"); en caso de necesitar esto, debo hacer +1 al malloc
	chmod(data->programaT, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH);
	char* strToSys = string_from_format("echo \"%s\" | .%s | sort -d - > %s", bufferTexto,data->programaT,data->archivoTemporal);
	system(strToSys);

	fclose(dataBin);
	return;
}

void realizarReduccionLocal(nodoRL* data){
	char* strArchTemp = listAsString(data->listaArchivosTemporales);
	chmod(data->programaR, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH);
	char* strToSys = string_from_format("sort -m %s | .%s > %s",strArchTemp, data->programaR, data->archivoTemporal);
	system(strToSys);
	return;
}

void realizarReduccionGlobal(solicitudRG* data){
	//char* strArchivosTemporales = listaAstringRG(data->nodos);
	chmod(data->programaR, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH);
	char* strToSys = string_from_format("echo \"%s\" | .%s > %s",workerEncargado->archTempRL, data->programaR, data->archRG);
	system(strToSys);
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
	if(!strcmp(paquete->header.emisor, MASTER)){

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

			break;
		}
		case REDGLOBALWORKER:{
			solicitudRG* datosRG = deserializacionRG(paquete->Payload);
			bool obtenerEncargado(nodoRG* elemento){
				return elemento->encargado;
			}
			bool obtenerActual(nodoRG* elemento){
				if (!strcmp(elemento->worker.nodo,NOMBRE_NODO)) return true;
				else false;
			}

			workerEncargado = list_find(datosRG->nodos,(void*)obtenerEncargado); //no hice malloc de workerEncargado, falla?
			nodoRG* workerActual = list_find(datosRG->nodos,(void*)obtenerActual);

			if(!strcmp(workerEncargado->worker.nodo,NOMBRE_NODO)){

				//Servidor(IP_NODO,PUERTO_WORKER,WORKER,accionSelect,RecibirHandshake); preguntar centu
				realizarReduccionGlobal(datosRG);
				boolAux =true;
				if(!(EnviarDatosTipo(socketMaster,WORKER,&boolAux,sizeof(bool),VALIDACIONWORKER))) perror("Error al enviar OK a master en etapa de RG2");
			}
			else{
				int socketWorkerEncargado = ConectarAServidor(workerEncargado->worker.puerto, workerEncargado->worker.ip, WORKER,WORKER, RecibirHandshake);
				FILE* archTemp = fopen(workerActual->archTempRL,"r");
				char str[BUFFERSIZE];

				while( fgets(str, BUFFERSIZE, archTemp)!=NULL ) {
					if(!(EnviarDatosTipo(socketWorkerEncargado, WORKER ,str, strlen(str)+1, ARCHIVOTEMPRL))) perror("Error al enviar archRLTemp al worker encargado");
				   }
				bool* ok = true;
				if(!(EnviarDatosTipo(socketWorkerEncargado, WORKER ,ok,sizeof(bool), ARCHIVOTEMPRL))) perror("Error al enviar confirmacion al worker encargado");

				fclose(archTemp);
			}
			break;
		}

		case 2:{ //FINALIZAR TODO terminar camino de AF
			//Deserializo lo que me mando Master
			datoAF* datosAF = deserializacionAF(paquete->Payload);
			// Abro el archivo de Reduc Global y copio su contenido a buffer
			FILE* fdRedGlobal = fopen(datosAF->resultRG,"r");
			struct stat st;
			stat(datosAF->resultRG,&st);
			char* bufferTexto = malloc((st.st_size)+1);
			fread(bufferTexto,st.st_size,1,fdRedGlobal);
			// Le mando a FS el buffer con el path
			socketFS = ConectarAServidor(PUERTO_FILESYSTEM, IP_FILESYSTEM, FILESYSTEM, WORKER, RecibirHandshake);
			serializacionAFyEnvioFS(bufferTexto,datosAF->rutaArchivoF,socketFS);
			//ESpero conf de FS
			// Mando conf a Master
			break;
		}
		default:{
			perror("No se recibio un header de las etapas");
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

	return 0;
}
