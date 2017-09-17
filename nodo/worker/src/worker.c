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
	char *NOMBRE_NODO,*PUERTO_WORKER,*RUTA_DATABIN;
	t_config *CFG;
	CFG = config_create("../nodoCFG.txt");   // Defino de dónde voy a leer la config.
	PUERTO_WORKER= config_get_string_value(CFG ,"PUERTO_WORKER");  // Obtengo el valor de puerto.
	NOMBRE_NODO= config_get_string_value(CFG ,"NOMBRE_NODO");  // Obtengo el valor de puerto.
	RUTA_DATABIN= config_get_string_value(CFG ,"RUTA_DATABIN");  // Obtengo el valor de puerto.

	printf("Configuración:\nPUERTO_WORKER = %s\nNOMBRE_NODO= %s\nRUTA_DATABIN= %s\n\n",PUERTO_WORKER,NOMBRE_NODO,RUTA_DATABIN);

	struct sockaddr_in addr;  //  usamos sockaddr_in para manipular los elementos de la dirección del socket.
	int addrlen= sizeof(addr);
	struct addrinfo hints;
	struct addrinfo *server_info;
	int listenning_socket, rv,socket_master,socket_nueva_conexion;

	memset(&hints, 0, sizeof(hints));  // Relleno con ceros el resto de la estructura.
	hints.ai_family = AF_INET;         // Seteo la familia de direcciones en inet.
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;  // Usamos sockets de flujo, bidireccionales.

	if ((rv =getaddrinfo(NULL, PUERTO_WORKER, &hints, &server_info)) != 0) {
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

	if( rv = listen( listenning_socket , BACKLOG )== -1){
		perror("Error en el listen");  // listen(descriptor de fichero, n conexiones permitidas).
	}
	printf("%s \n", "El Servidor se encuentra OK para escuchar conexiones.");

	char *un_buffer=calloc(sizeof(char),20);
	if((socket_nueva_conexion = accept(listenning_socket,(struct sockaddr *)&addr,&addrlen)) == -1){
			perror("Error con conexion entrante");
	}else{
		 recv(socket_nueva_conexion,un_buffer,20*sizeof(char),0);
	}
	printf("%s\n",un_buffer);



}
