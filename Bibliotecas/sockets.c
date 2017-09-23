#include "sockets.h"

void Servidor(char* ip, int puerto, char nombre[11],
		void (*accion)(Paquete* paquete, int socketFD),
		int (*RecibirPaquete)(int socketFD, char receptor[11], Paquete* paquete)) {
	printf("Iniciando Servidor %s\n", nombre);
	int SocketEscucha = StartServidor(ip, puerto);
	fd_set master; // conjunto maestro de descriptores de fichero
	fd_set read_fds; // conjunto temporal de descriptores de fichero para select()
	FD_ZERO(&master); // borra los conjuntos maestro y temporal
	FD_ZERO(&read_fds);
	FD_SET(SocketEscucha, &master); // añadir listener al conjunto maestro
	int fdmax = SocketEscucha; // seguir la pista del descriptor de fichero mayor, por ahora es éste
	struct sockaddr_in remoteaddr; // dirección del cliente

	for (;;) {	// bucle principal
		read_fds = master; // cópialo
		if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
			perror("select");
			exit(1);
		}
		// explorar conexiones existentes en busca de datos que leer
		int i;
		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) { // ¡¡tenemos datos!!
				if (i == SocketEscucha) { // gestionar nuevas conexiones
					socklen_t addrlen = sizeof(remoteaddr);
					int nuevoSocket = accept(SocketEscucha,
							(struct sockaddr*) &remoteaddr, &addrlen);
					if (nuevoSocket == -1)
						perror("accept");
					else {
						FD_SET(nuevoSocket, &master); // añadir al conjunto maestro
						if (nuevoSocket > fdmax)
							fdmax = nuevoSocket; // actualizar el máximo
						printf("\nConectando con %s en " "socket %d\n",
								inet_ntoa(remoteaddr.sin_addr), nuevoSocket);
					}
				} else {
					Paquete paquete;
					int result = RecibirPaquete(i, nombre, &paquete);
					if (result > 0)
						accion(&paquete, i); //>>>>Esto hace el servidor cuando recibe algo<<<<
					else
						FD_CLR(i, &master); // eliminar del conjunto maestro si falla
					if (paquete.Payload != NULL)
						free(paquete.Payload); //Y finalmente, no puede faltar hacer el free
				}
			}
		}
	}
}

void ServidorConcurrente(char* ip, int puerto, char nombre[11], t_list** listaDeHilos,
		bool* terminar, void (*accionHilo)(void* socketFD)) {
	printf("Iniciando Servidor %s\n", nombre);
	*terminar = false;
	*listaDeHilos = list_create();
	int socketFD = StartServidor(ip, puerto);
	struct sockaddr_in their_addr; // información sobre la dirección del cliente
	int new_fd;
	socklen_t sin_size;

	while(!*terminar) { // Loop Principal
		sin_size = sizeof(struct sockaddr_in);
		if ((new_fd = accept(socketFD, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
			perror("accept");
			continue;
		}
		printf("\nNueva conexion de %s en " "socket %d\n", inet_ntoa(their_addr.sin_addr), new_fd);
		//pthread_t hilo = agregarAListaHiloSiNoEsta(listaHilos, socketFD);
		structHilo* itemNuevo = malloc(sizeof(structHilo));
		itemNuevo->socket = new_fd;
		pthread_create(&(itemNuevo->hilo), NULL, (void*)accionHilo, &new_fd);
		list_add(*listaDeHilos, itemNuevo);
	}
	printf("Apagando Servidor");
	close(socketFD);
	//libera los items de lista de hilos , destruye la lista y espera a que termine cada hilo.
	list_destroy_and_destroy_elements(*listaDeHilos, LAMBDA(void _(void* elem) {
			pthread_join(((structHilo*)elem)->hilo, NULL);
			free(elem); }));

}

void ServidorConcurrenteForks(char* ip, int puerto, char nombre[11], t_list** listaDeProcesos,
		bool* terminar, void (*accionPadre)(void* socketFD), void (*accionHijo) (void* socketFD)) {
	printf("Iniciando Servidor %s\n", nombre);
	*terminar = false;
	*listaDeProcesos = list_create();
	int socketFD = StartServidor(ip, puerto);
	struct sockaddr_in their_addr; // información sobre la dirección del cliente
	int new_fd;
	socklen_t sin_size;

	while(!*terminar) { // Loop Principal
		sin_size = sizeof(struct sockaddr_in);
		if ((new_fd = accept(socketFD, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
			perror("accept");
			continue;
		}
		printf("\nNueva conexion de %s en " "socket %d\n", inet_ntoa(their_addr.sin_addr), new_fd);
		structProceso* itemNuevo = malloc(sizeof(structProceso));
		itemNuevo->socket = new_fd;
		list_add(*listaDeProcesos, itemNuevo);
		pid_t pid = fork();
		if(pid >= 0) //fork exitoso
		{
			if (pid > 0) //padre
			{
				accionPadre(itemNuevo->socket);
			}
			else //hijo
			{
				accionHijo(itemNuevo->socket);
			}
		}
		else
			perror("error en el fork");
	}
	printf("Apagando Servidor");
	close(socketFD);
	//libera los items de lista de hilos , destruye la lista y espera a que termine cada hilo.
	list_destroy_and_destroy_elements(*listaDeProcesos, LAMBDA(void _(void* elem) {
			free(elem); }));

}



int ConectarAServidor(int puertoAConectar, char* ipAConectar, char servidor[11],
		char cliente[11], void RecibirElHandshake(int socketFD, char emisor[11])) {
	int socketFD = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in direccion;

	direccion.sin_family = AF_INET;
	direccion.sin_port = htons(puertoAConectar);
	direccion.sin_addr.s_addr = inet_addr(ipAConectar);
	memset(&(direccion.sin_zero), '\0', 8);

	while (connect(socketFD, (struct sockaddr *) &direccion, sizeof(struct sockaddr))<0)
		sleep(1); //Espera un segundo y se vuelve a tratar de conectar.
	EnviarHandshake(socketFD, cliente);
	RecibirElHandshake(socketFD, servidor);
	return socketFD;

}

int StartServidor(char* MyIP, int MyPort) // obtener socket a la escucha
{
	struct sockaddr_in myaddr; // dirección del servidor
	int yes = 1; // para setsockopt() SO_REUSEADDR, más abajo
	int SocketEscucha;

	if ((SocketEscucha = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	if (setsockopt(SocketEscucha, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))
			== -1) // obviar el mensaje "address already in use" (la dirección ya se está 	usando)
			{
		perror("setsockopt");
		exit(1);
	}

	// enlazar
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = inet_addr(MyIP);
	myaddr.sin_port = htons(MyPort);
	memset(&(myaddr.sin_zero), '\0', 8);

	if (bind(SocketEscucha, (struct sockaddr*) &myaddr, sizeof(myaddr)) == -1) {
		perror("bind");
		exit(1);
	}

	// escuchar
	if (listen(SocketEscucha, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	return SocketEscucha;
}

void EnviarPaquete(int socketCliente, Paquete* paquete) {
	int cantAEnviar = sizeof(Header) + paquete->header.tamPayload;
	void* datos = malloc(cantAEnviar);
	memcpy(datos, &(paquete->header), TAMANIOHEADER);
	if (paquete->header.tamPayload > 0) //No sea handshake
		memcpy(datos + TAMANIOHEADER, (paquete->Payload),
				paquete->header.tamPayload);

	//Paquete* punteroMsg = datos;
	int enviado = 0; //bytes enviados
	int totalEnviado = 0;

	do {
		enviado = send(socketCliente, datos + totalEnviado,
				cantAEnviar - totalEnviado, 0);
		//largo -= totalEnviado;
		totalEnviado += enviado;
		//punteroMsg += enviado; //avanza la cant de bytes que ya mando
	} while (totalEnviado != cantAEnviar);
	free(datos);
}

void EnviarDatosTipo(int socketFD, char emisor[11], void* datos, int tamDatos, tipo tipoMensaje){
	Paquete* paquete = malloc(sizeof(Paquete));
	paquete->header.tipoMensaje = tipoMensaje;
	strcpy(paquete->header.emisor, emisor);
	uint32_t r = 0;
	if(tamDatos<=0 || datos==NULL){
		paquete->header.tamPayload = sizeof(uint32_t);
		paquete->Payload = &r;
	} else {
		paquete->header.tamPayload = tamDatos;
		paquete->Payload = datos;
	}
	EnviarPaquete(socketFD, paquete);

	free(paquete);
}
void EnviarMensaje(int socketFD, char* msg, char emisor[11]) {
	Paquete paquete;
	strcpy(paquete.header.emisor, emisor);
	paquete.header.tipoMensaje = ESSTRING;
	paquete.header.tamPayload = string_length(msg) + 1;
	paquete.Payload = msg;
	EnviarPaquete(socketFD, &paquete);

}

void EnviarHandshake(int socketFD, char emisor[11]) {
	Paquete* paquete = malloc(TAMANIOHEADER);
	Header header;
	header.tipoMensaje = ESHANDSHAKE;
	header.tamPayload = 0;
	strcpy(header.emisor, emisor);
	paquete->header = header;
	EnviarPaquete(socketFD, paquete);

	free(paquete);
}

void EnviarDatos(int socketFD, char emisor[11], void* datos, int tamDatos) {
	EnviarDatosTipo(socketFD, emisor, datos, tamDatos, ESDATOS);
}

void RecibirHandshake(int socketFD, char emisor[11]) {
	Header header;
	int resul = RecibirDatos(&header, socketFD, TAMANIOHEADER);
	if (resul > 0) { // si no hubo error en la recepcion
		if (strcmp(header.emisor, emisor) == 0) {
			if (header.tipoMensaje == ESHANDSHAKE)
				printf("\nConectado con el servidor %s\n", emisor);
			else
				perror("Error de Conexion, no se recibio un handshake\n");
		} else
			perror("Error, no se recibio un handshake del servidor esperado\n");
	}
}

int RecibirDatos(void* paquete, int socketFD, uint32_t cantARecibir) {
	void* datos = malloc(cantARecibir);
	int recibido = 0;
	int totalRecibido = 0;

	do {
		recibido = recv(socketFD, datos + totalRecibido, cantARecibir - totalRecibido, 0);
		totalRecibido += recibido;
	} while (totalRecibido != cantARecibir && recibido > 0);
	memcpy(paquete, datos, cantARecibir);
	free(datos);

	if (recibido < 0) {
		printf("Cliente Desconectado\n");
		close(socketFD); // ¡Hasta luego!
		//exit(1);
	} else if (recibido == 0) {
		printf("Fin de Conexion en socket %d\n", socketFD);
		close(socketFD); // ¡Hasta luego!
	}

	return recibido;
}

int RecibirPaqueteServidor(int socketFD, char receptor[11], Paquete* paquete) {
	paquete->Payload = NULL;
	int resul = RecibirDatos(&(paquete->header), socketFD, TAMANIOHEADER);
	if (resul > 0) { //si no hubo error
		if (paquete->header.tipoMensaje == ESHANDSHAKE) { //vemos si es un handshake
			printf("Se establecio conexion con %s\n", paquete->header.emisor);
			EnviarHandshake(socketFD, receptor); // paquete->header.emisor
		} else if (paquete->header.tamPayload > 0){ //recibimos un payload y lo procesamos (por ej, puede mostrarlo)
			paquete->Payload = malloc(paquete->header.tamPayload);
			resul = RecibirDatos(paquete->Payload, socketFD, paquete->header.tamPayload);
		}
	}
	return resul;
}

int RecibirPaqueteCliente(int socketFD, char receptor[11], Paquete* paquete) {
	paquete->Payload = NULL;
	int resul = RecibirDatos(&(paquete->header), socketFD, TAMANIOHEADER);
	if (resul > 0 && paquete->header.tipoMensaje != ESHANDSHAKE && paquete->header.tamPayload > 0) { //si no hubo error ni es un handshake
		paquete->Payload = malloc(paquete->header.tamPayload);
		resul = RecibirDatos(paquete->Payload, socketFD, paquete->header.tamPayload);
	}
	return resul;
}
