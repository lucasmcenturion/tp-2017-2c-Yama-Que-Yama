#include "sockets.h"

#define TAMANIODEBLOQUE 1024;
#define BUFFERSIZE 1024

char *IP_NODO, *IP_FILESYSTEM,*NOMBRE_NODO,*RUTA_DATABIN;
int PUERTO_FILESYSTEM, PUERTO_WORKER, PUERTO_DATANODE;
int socketFS;
t_list* listaDeProcesos;
bool end;
t_list* listaDeArchivosTempRG;


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

void accionSelect(Paquete* paquete, int socketFD){
	char* bufferTexto = malloc(sizeof(paquete->header.tamPayload));
	strcpy(bufferTexto,paquete->Payload);
	list_add(listaDeArchivosTempRG,bufferTexto);
	return;
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
	bool boolAux;
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


void realizarTransformacion(nodoT* data){
	FILE* dataBin = fopen(RUTA_DATABIN,"r");
	char* bufferTexto = malloc(data->bytesOcupados);
	int mov = data->bloque * TAMANIODEBLOQUE;
	fseek(dataBin,mov,SEEK_SET);
	fread(bufferTexto,data->bytesOcupados,1,dataBin);

	//char* bufferReal = strcat(bufferTexto,"\n"); en caso de necesitar esto, debo hacer +1 al malloc
	chmod(data->programaT, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH);
	char* strToSys = string_from_format("echo %s | .%s | sort -d - > %s", bufferTexto,data->programaT,data->archivoTemporal);
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
	char* strArchivosTemporales = listaAstringRG(data->nodos);
	chmod(data->programaR, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH);
	char* strToSys = string_from_format("sort -m %s | .%s > %s",strArchivosTemporales, data->programaR, data->archRG);
	system(strToSys);
	return;
}

void accionPadre(void* socketMaster){
}

void accionHijo(void* socketM){

	int socketMaster = *(int*) socketM;
	Paquete* paquete = malloc(sizeof(Paquete));

	if(RecibirPaqueteCliente(socketMaster, WORKER, paquete)<0) perror("Error: No se recibieron los datos de Master");
	if(!strcmp(paquete->header.emisor, MASTER)){ //Posible error en el !

		switch(paquete->header.tipoMensaje){
		case TRANSFWORKER:{
			nodoT* datosT = deserializacionT(paquete->Payload);
			realizarTransformacion(datosT);
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
				if (strcmp(elemento->worker.nodo,NOMBRE_NODO)) return true;
				else false;
			}
			nodoRG* workerEncargado = list_find(datosRG->nodos,(void*)obtenerEncargado);
			nodoRG* workerActual = list_find(datosRG->nodos,(void*)obtenerActual);
			//if es encargado, se conecta a cada uno para obtener los archivos temporales
			if(strcmp(workerEncargado->worker.nodo,NOMBRE_NODO)){

				//Servidor(IP_NODO,PUERTO_WORKER,WORKER,accionSelect,RecibirHandshake); preguntar centu
				realizarReduccionGlobal(datosRG);
			}
			else{//else espera conexion y manda archivo temporal
				int socketWorkerEncargado = ConectarAServidor(workerEncargado->worker.puerto, workerEncargado->worker.ip, WORKER,WORKER, RecibirHandshake);
				FILE* archTemp = fopen(workerActual->archTempRL,"r");
				char str[BUFFERSIZE];

				while( fgets(str, BUFFERSIZE, archTemp)!=NULL ) {
					if(!(EnviarDatosTipo(socketWorkerEncargado, WORKER ,str, strlen(str)+1, ARCHIVOTEMPRL))) perror("Error al enviar archRLTemp al worker encargado");
				   } else {
					   if(!(EnviarDatosTipo(socketWorkerEncargado, WORKER ,str, strlen(str)+1, ARCHIVOTEMPRL))) perror("Error al enviar confirmacion al worker encargado");
				   }
				   fclose(archTemp);
			}
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
