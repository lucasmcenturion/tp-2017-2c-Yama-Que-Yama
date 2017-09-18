#include "sockets.h"

char *IP;
int PUERTO;
int socketFS;

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

void accion(){

}

int main(){
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();

	Servidor(IP, PUERTO, FILESYSTEM, accion, RecibirPaqueteCliente);

	/*
	char * linea;
	while(1) {
		linea = readline(">> ");
		if(linea)
		  add_history(linea);
		if(!strcmp(linea, "format"))
			printf("logica de format\n");
		else if(!strncmp(linea, "rm", 2))
		{
			linea += 2;
			if(!strncmp(linea, " -d", 3))
				printf("remover directorio\n");
			else if(!strncmp(linea, " -b", 3))
				printf("remover bloque\n");
			else
				printf("no existe ese comando\n");
			linea -= 2;
		}
		else if(!strncmp(linea, "rename", 6))
			printf("renombar\n");
		else if(!strncmp(linea, "mv", 2))
			printf("mover\n");
		else if(!strncmp(linea, "cat", 3))
			printf("muestra archivo\n");
		else if(!strncmp(linea, "mkdir", 5))
			printf("crea directorio\n");
		else if(!strncmp(linea, "cpfrom", 6))
			printf("copiando desde\n");
		else if(!strncmp(linea, "cpto", 4))
			printf("copiando a\n");
		else if(!strncmp(linea, "cpblock", 7))
			printf("copia de bloque\n");
		else if(!strncmp(linea, "md5", 3))
			printf("MD5\n");
		else if(!strncmp(linea, "ls", 2))
			printf("lista de archivos\n");
		else if(!strncmp(linea, "info", 4))
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
	  }*/
	return 0;
}
