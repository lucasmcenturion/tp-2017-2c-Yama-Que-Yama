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
#define BACKLOG 5  // Número de conexiones permitidas en la cola de entrada.

int main(){
	char *IP_FILESYSTEM,*PUERTO_FILESYSTEM,*NOMBRE_NODO,*RUTA_DATABIN;
	t_config *CFG;
	CFG = config_create("../nodoCFG.txt");   // Defino de dónde voy a leer la config.
	IP_FILESYSTEM= config_get_string_value(CFG ,"IP_FILESYSTEM");  // Obtengo el valor de puerto.
	PUERTO_FILESYSTEM=config_get_string_value(CFG,"PUERTO_FILESYSTEM");
	NOMBRE_NODO=config_get_string_value(CFG,"NOMBRE_NODO");
	RUTA_DATABIN=config_get_string_value(CFG,"RUTA_DATABIN");


	printf("Configuración:\n IP_FILESYSTEM = %s\nPUERTO_FILESYSTEM=%s\nNOMBRE_NODO= %s\nRUTA_DATABIN=%s\n",IP_FILESYSTEM,PUERTO_FILESYSTEM,NOMBRE_NODO,RUTA_DATABIN);

	struct sockaddr_in addr;  //  usamos sockaddr_in para manipular los elementos de la dirección del socket.
	int addrlen= sizeof(addr);
	struct addrinfo hints;
	struct addrinfo *server_info;
	int rv,rv_fs,socket_fs;
  
	memset(&hints, 0, sizeof(hints));  // Relleno con ceros el resto de la estructura. 
	hints.ai_family = AF_INET;         // Seteo la familia de direcciones en inet. 
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;  // Usamos sockets de flujo, bidireccionales. 

	if ((rv =getaddrinfo(IP_FILESYSTEM, PUERTO_FILESYSTEM, &hints, &server_info)) != 0) {
		printf("%i\n",rv );
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
	}
	socket_fs=socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
	printf("%s%i\n","El valor del Socket fs es: ",socket_fs);
	printf("%s \n", "Socket OK");

	char *nombre=calloc(sizeof(char),18);
	strcat(nombre,"Hola soy el ");
	strcat(nombre,NOMBRE_NODO);

	if ((rv_fs=connect(socket_fs, server_info->ai_addr, server_info->ai_addrlen)) ==-1)
	 {
	 	perror("No se pudo conectar con filesystem");
	 }else{
	 	printf("Conexión establecida con filesystem\n");

	 	send(socket_fs,nombre,20*sizeof(char),0);
	 } 


	
}
