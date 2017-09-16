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
	char *YAMA_IP,*YAMA_PUERTO;
	t_config *CFG;
	CFG = config_create("masterCFG.txt");   // Defino de dónde voy a leer la config.
	YAMA_IP= config_get_string_value(CFG ,"PUERTO");  // Obtengo el valor de puerto.
	YAMA_PUERTO=config_get_string_value(CFG,"YAMA_PUERTO");

	printf("Configuración: YAMA_PUERTO = %s\n,YAMA_IP=%s\n",YAMA_PUERTO,YAMA_IP);

	struct sockaddr_in addr;  //  usamos sockaddr_in para manipular los elementos de la dirección del socket.
	int addrlen= sizeof(addr);
	struct addrinfo hints;
	struct addrinfo *server_info;
	int rv,rv_yama,socket_yama;
  
	memset(&hints, 0, sizeof(hints));  // Relleno con ceros el resto de la estructura. 
	hints.ai_family = AF_INET;         // Seteo la familia de direcciones en inet. 
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;  // Usamos sockets de flujo, bidireccionales. 

	if ((rv =getaddrinfo(YAMA_IP, YAMA_PUERTO, &hints, &server_info)) != 0) {
		printf("%i\n",rv );
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
	}
	socket_yama=socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
	printf("%s%i\n","El valor del Socket Yama es: ",socket_yama);
	printf("%s \n", "Socket OK");

	char *buffer=calloc(sizeof(char),20);
	strcpy(buffer,"Hola soy master");

	if ((rv_yama=connect(socket_yama, server_info->ai_addr, server_info->ai_addrlen)) ==-1)
	 {
	 	perror("No se pudo conectar con yama");
	 }else{
	 	printf("Conexión establecida con Yama\n");

	 	send(socket_yama,buffer,20*sizeof(char),0);
	 } 


	
}