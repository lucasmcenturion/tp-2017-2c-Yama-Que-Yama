#include "sockets.h"

char *IP;
int PUERTO;
int socketFS;
int socketYAMA;

void obtenerValoresArchivoConfiguracion() {
	t_config* arch = config_create("filesystemCFG.txt");
	IP = string_duplicate(config_get_string_value(arch, "IP"));
	PUERTO = config_get_int_value(arch, "PUERTO");
	config_destroy(arch);
}

void imprimirArchivoConfiguracion(){
	printf(
				"Configuración:\n"
				"IP=%s\n"
				"PUERTO=%d\n",
				IP,
				PUERTO
				);
}

void accion(Paquete* paquete, int socketCliente){

	if(!strcmp(paquete->header.emisor, YAMA))
	{
		socketYAMA = socketCliente;
	}
	if(socketYAMA != 0)
	{
		switch(paquete->header.tipoMensaje)
		{
			case ESDATOS:
				{
					uint32_t* datos = paquete->Payload;
					switch (*datos)
					{
						case FORMATEAR:
							break;
						case REMOVER:
							break;
						case RENOMBRAR:
							break;
						case MOVER:
							break;
						case MOSTRAR:
							break;
						case CREARDIR:
							break;
						case CPFROM:
							break;
						case CPTO:
							break;
						case CPBLOCK:
							break;
						case MD5:
							break;
						case LISTAR:
							break;
						case INFO:
							break;
					}
				}
				break;

			case NUEVOWORKER:
				{
					datosWorker* worker = paquete->Payload;
					EnviarDatosTipo(socketYAMA, DATANODE, worker, sizeof(datosWorker), NUEVOWORKER);
				}
				break;
		}
	}
}

void userInterfaceHandler() {
	char * linea;
	while(true) {
		linea = readline(">> ");
		if(linea)
		  add_history(linea);
		if(!strcmp(linea, "format"))
			printf("logica de format\n");
		else if(!strncmp(linea, "rm ", 3))
		{
			linea += 2;
			if(!strncmp(linea, "-d", 2))
				printf("remover directorio\n");
			else if(!strncmp(linea, "-b", 2))
				printf("remover bloque\n");
			else
				printf("remover archivo\n");
			linea -= 2;
		}
		else if(!strncmp(linea, "rename ", 7))
			printf("renombar\n");
		else if(!strncmp(linea, "mv ", 3))
			printf("mover\n");
		else if(!strncmp(linea, "cat ", 4))
			printf("muestra archivo\n");
		else if(!strncmp(linea, "mkdir ", 6))
			printf("crea directorio\n");
		else if(!strncmp(linea, "cpfrom ", 7))
			printf("copiando desde\n");
		else if(!strncmp(linea, "cpto ", 5))
			printf("copiando a\n");
		else if(!strncmp(linea, "cpblock ", 8))
			printf("copia de bloque\n");
		else if(!strncmp(linea, "md5 ", 4))
			printf("MD5\n");
		else if(!strncmp(linea, "ls ", 3))
			printf("lista de archivos\n");
		else if(!strncmp(linea, "info ", 5))
			printf("info archivo\n");
		else if(!strcmp(linea, "exit"))
		{
			   printf("salio\n");
			   free(linea);
			   break;
		}
		else
			printf("no se conoce el comando\n");
		free(linea);
	}
}

int main(){
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();
	pthread_t userInterface;
	pthread_create(&userInterface, NULL, (void*)userInterfaceHandler,NULL);
	Servidor(IP, PUERTO, FILESYSTEM, accion, RecibirPaqueteServidor);
	pthread_join(userInterface, NULL);
	return 0;
}
