#include "sockets.h"

#define TAMANIODEBLOQUE 1024;

char *IP_NODO, *IP_FILESYSTEM,*NOMBRE_NODO,*RUTA_DATABIN;
int PUERTO_FILESYSTEM, PUERTO_WORKER, PUERTO_DATANODE;
int socketFS;
t_list* listaDeProcesos;
bool end;

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

char* listaAstring(char** string, int n ){
	char* resultado = string_new();
	int x;
	for(x=0;x<n-1;x++){
	string_append(&resultado, string[x]);
	string_append(&resultado," ");
	}
	string_append(&resultado,string[n-1]);
	return resultado;
}

void realizarTransformacion(solicitudPrograma* programa){
	FILE* dataBin = fopen("/home/utnso/workspace/nodo/archivos/nodo1/data.bin","r");

	/*struct stat dataBinStat;
	stat("/home/utnso/workspace/nodo/archivos/nodo1/data.bin",&dataBinStat);
	int sizeOfFile = dataBinStat.st_size;*/

	char* bufferTexto = malloc(programa->cantidadDeBytesOcupados);
	int mov = programa->bloque * TAMANIODEBLOQUE;
	fseek(dataBin,mov,SEEK_SET);
	fread(bufferTexto,programa->cantidadDeBytesOcupados,1,dataBin);
	//char* bufferReal = strcat(bufferTexto,"\n"); en caso de necesitar esto, debo hacer +1 al malloc
	char* strToSys = string_from_format("echo %s | .%s | sort -d - > %s", bufferTexto,programa->programa,programa->archivoTemporal);
	system(strToSys);
	return;
}

void realizarReduccionLocal(solicitudPrograma* programa){
	char* strArchTemp = string_new();//string con archivos temporales
	listaAstring(programa->ListaArchivosTemporales,programa->cantArchTempRL);
	char* strToSys = string_from_format("sort -m %s | .%s > %s",strArchTemp, programa->programa, programa->archivoTemporal);
	system(strToSys);
	return;
}

void realizarReduccionGlobal(solicitudPrograma* programa){

}

void accionPadre(void* socketMaster){
}

void accionHijo(void* socketM){

	int socketMaster = *(int*) socketM;
	Paquete* paqueteArecibir = malloc(sizeof(Paquete));
	RecibirPaqueteCliente(socketMaster, WORKER, paqueteArecibir);
	if(!strcmp(paqueteArecibir->header.emisor, MASTER)){ //Posible error en el !

		solicitudPrograma* programa = malloc(paqueteArecibir->header.tamPayload);
		switch(paqueteArecibir->header.tipoMensaje){
		case TRANSFWORKER:{
			programa->programa = ((solicitudPrograma*)paqueteArecibir->Payload)->programa;
			programa->archivoTemporal = ((solicitudPrograma*)paqueteArecibir->Payload)->archivoTemporal;
			programa->bloque = ((solicitudPrograma*)paqueteArecibir->Payload)->bloque;
			programa->cantidadDeBytesOcupados = ((solicitudPrograma*)paqueteArecibir->Payload)->cantidadDeBytesOcupados;
			realizarTransformacion(programa);
			break;
		}
		case REDLOCALWORKER:{
			programa->programa = ((solicitudPrograma*)paqueteArecibir->Payload)->programa;
			programa->archivoTemporal = ((solicitudPrograma*)paqueteArecibir->Payload)->archivoTemporal;
			programa->ListaArchivosTemporales = ((solicitudPrograma*)paqueteArecibir->Payload)->ListaArchivosTemporales;
			programa->cantArchTempRL = ((solicitudPrograma*)paqueteArecibir->Payload)->cantArchTempRL; //TODO
			realizarReduccionLocal(programa);
			break;
		}
		case REDGLOBALWORKER:{
/*
			programa->programa = ((solicitudPrograma*)paqueteArecibir->Payload)->programa;
			programa->archivoTemporal = ((solicitudPrograma*)paqueteArecibir->Payload)->archivoTemporal;
			programa->workerEncargado = ((solicitudPrograma*)paqueteArecibir->Payload)->workerEncargado;
			if(string_equals_ignore_case(NOMBRE_NODO, programa->workerEncargado.nodo));
			programa->ListaArchivosTemporales = ((solicitudPrograma*)paqueteArecibir->Payload)->ListaArchivosTemporales;*/



			realizarReduccionGlobal(programa);
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
