#include <commons/collections/list.h>
#include <commons/string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>  // Para sockets.
#include <fcntl.h>
#include <sys/socket.h>  // Para sockets.
#include <netdb.h>
#include <unistd.h>
#include <commons/txt.h>
#include <errno.h>
#include <sys/time.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stddef.h>
#include <math.h>
#include <stdint.h>
#include <sys/stat.h>
#include <commons/config.h>
#include <commons/bitarray.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <readline/readline.h>
#include <readline/history.h>
#define BACKLOG 5  // Número de conexiones permitidas en la cola de entrada.




void consola(){
	char * linea;
		while(1) {
			linea = readline(">>");
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
				   printf("Fin de la consola\n");
				   free(linea);
				   break;
			}
			else
				printf("no se conoce el comando\n");
			free(linea);
		  }
}
void aceptar(int* listenning_socket){
	struct sockaddr_in addr;  //  usamos sockaddr_in para manipular los elementos de la dirección del socket.
	int addrlen= sizeof(addr);
	char *un_buffer=calloc(sizeof(char),20);
	int socket_nueva_conexion;

	
	if((socket_nueva_conexion = accept(*listenning_socket,(struct sockaddr *)&addr,&addrlen)) == -1){
			perror("Error con conexion entrante");
	}else{
		 recv(socket_nueva_conexion,un_buffer,20*sizeof(char),0);
	}
	printf("%s\n",un_buffer);

	char *otro_buffer=calloc(sizeof(char),18);
	if ((socket_nueva_conexion = accept(*listenning_socket,(struct sockaddr *)&addr,&addrlen)) == -1){
			perror("Error con conexion entrante");
	}else{
		 recv(socket_nueva_conexion,otro_buffer,18*sizeof(char),0);
	}
	printf("%s\n",otro_buffer);

}

int main(){
	char* PUERTO;
	pthread_t hilo_consola;
	pthread_t hilo_aceptador;
	t_config *CFG;
	CFG = config_create("/home/utnso/Escritorio/tp-2017-2c-Yama-Que-Yama/fs/filesystemCFG.txt");   // Defino de dónde voy a leer la config.
	PUERTO= config_get_string_value(CFG ,"PUERTO");  // Obtengo el valor de puerto.
	printf("Configuración: PUERTO = %s\n",PUERTO);

	struct addrinfo hints;
	struct addrinfo *server_info;
	int listenning_socket, rv;
  
	memset(&hints, 0, sizeof(hints));  // Relleno con ceros el resto de la estructura. 
	hints.ai_family = AF_INET;         // Seteo la familia de direcciones en inet. 
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;  // Usamos sockets de flujo, bidireccionales. 

	if ((rv =getaddrinfo(NULL, PUERTO, &hints, &server_info)) != 0) {
		printf("%i\n",rv );
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
	}
	listenning_socket = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);  // Obtengo el descriptor de fichero.
	printf("%s%i\n","El valor del Listenning Socket es: ",listenning_socket );
	printf("%s \n", "Socket OK");

	if(bind(listenning_socket,server_info->ai_addr, server_info->ai_addrlen)==-1) {  
		perror("Error en el bind."); exit(1);
	}
	
	printf("%s \n", "Bind OK\n");
	freeaddrinfo(server_info);

	if( rv = listen(listenning_socket , BACKLOG )== -1) perror("Error en el listen");  // listen(descriptor de fichero, n conexiones permitidas).
	printf("%s \n", "El Servidor se encuentra OK para escuchar conexiones.");

	pthread_create(&hilo_consola,NULL,(void*)consola,NULL);
	pthread_create(&hilo_aceptador,NULL,(void*)aceptar,&listenning_socket);

	pthread_join(hilo_consola,NULL);
	pthread_join(hilo_aceptador,NULL);
	
}
