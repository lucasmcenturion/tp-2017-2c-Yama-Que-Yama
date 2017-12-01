#include "sockets.h"

char *IP, *FS_IP, *ALGORITMO_BALANCEO;
int PUERTO, FS_PUERTO, RETARDO_PLANIFICACION, BASE;
int socketFS;
t_list* listaMasters;
t_list* listaWorkers;
t_list* listaHilos;
t_list* tablaDeEstados;
datosWorker* punteroClock;
bool end;
int idsMaster=0;
int indiceWorker=0;

void obtenerValoresArchivoConfiguracion() {
	t_config* arch = config_create("yamaCFG.txt");
	IP = string_duplicate(config_get_string_value(arch, "IP"));
	PUERTO = config_get_int_value(arch, "PUERTO");
	FS_IP = string_duplicate(config_get_string_value(arch, "FS_IP"));
	FS_PUERTO = config_get_int_value(arch, "FS_PUERTO");
	RETARDO_PLANIFICACION = config_get_int_value(arch, "RETARDO_PLANIFICACION");
	ALGORITMO_BALANCEO = string_duplicate(config_get_string_value(arch, "ALGORITMO_BALANCEO"));
	BASE = config_get_int_value(arch, "BASE");
	config_destroy(arch);
}

void imprimirArchivoConfiguracion(){
	printf(
				"Configuración:\n"
				"IP=%s\n"
				"PUERTO=%d\n"
				"FS_IP=%s\n"
				"FS_PUERTO=%d\n"
				"RETARDO_PLANIFICACION=%d\n"
				"ALGORITMO_BALANCEO=%s\n"
				"BASE=%d\n",
				IP,
				PUERTO,
				FS_IP,
				FS_PUERTO,
				RETARDO_PLANIFICACION,
				ALGORITMO_BALANCEO,
				BASE
				);
}

//TRUE SI NO ES WORKER, FALSE SI ES WORKER
void* RecibirPaqueteFilesystem(Paquete* paquete){
	while(1)
	{
		RecibirPaqueteCliente(socketFS,YAMA,paquete);
		if(paquete->header.tipoMensaje == NUEVOWORKER){
			//TODO: GUARDAR Y ENVIAR A MASTER
			void* datos = paquete->Payload;
			int tamanio = paquete->header.tamPayload;
			datosWorker* worker = malloc(tamanio);
			uint32_t largo;
			largo = *((uint32_t*)datos);
			datos += sizeof(uint32_t);
			worker->nodo = string_new();
			strcpy(worker->nodo, datos);
			datos+=largo + 1;
			worker->puerto = *((uint32_t*)datos);
			datos += sizeof(uint32_t);
			largo = *((uint32_t*)datos);
			datos += sizeof(uint32_t);
			worker->ip = string_new();
			strcpy(worker->ip, datos);
			datos += largo + 1;
			worker->cargaDeTrabajo = *((uint32_t*)datos);
			datos += sizeof(uint32_t);
			worker->disponibilidad = *((uint32_t*)datos);
			datos += sizeof(uint32_t);
			worker->contTareasRealizadas = *((uint32_t*)datos);
			datos += sizeof(uint32_t);
			datos -= tamanio;
			worker->indice = indiceWorker;
			indiceWorker++;
			list_add(listaWorkers,worker);
		}
		else if (paquete->header.tipoMensaje == NODODESCONECTADO)
		{
			char* nodoAEliminar = string_new();
			strcpy(nodoAEliminar,paquete->Payload);
			int indiceEliminado = ((datosWorker*)list_find(listaWorkers,LAMBDA(bool _(void* item1) { return !strcmp(((datosWorker*)item1)->nodo,nodoAEliminar);})))->indice;
			list_remove_and_destroy_by_condition(listaWorkers, LAMBDA(bool _(void* item1) { return !strcmp(((datosWorker*)item1)->nodo,nodoAEliminar);}), free);
			void actualizarIndice(datosWorker* worker){
				if (worker->indice > indiceEliminado)
					worker->indice--;
			}
			list_iterate(listaWorkers,actualizarIndice);
			indiceWorker--;


		}
	}
}

void CrearListas() {
	listaWorkers = list_create();
	listaHilos = list_create();
	listaMasters = list_create();
	tablaDeEstados = list_create();
}

void LiberarListas() {
	list_destroy_and_destroy_elements(listaWorkers, free);
	list_destroy_and_destroy_elements(listaHilos, free);
	list_destroy_and_destroy_elements(listaMasters, free);
	list_destroy_and_destroy_elements(tablaDeEstados, free);
}

void accion(void* socket){
	int socketFD = *(int*)socket;
	Paquete paquete;
	while (RecibirPaqueteServidor(socketFD, YAMA, &paquete) > 0)
	{
		if(!strcmp(paquete.header.emisor, MASTER))
		{
			switch (paquete.header.tipoMensaje)
			{
				case ESHANDSHAKE:
				{
					master* m = malloc(sizeof(master));
					m->id = idsMaster;
					m->socket = socketFD;
					list_add(listaMasters, m);
					idsMaster++;
				}
					break;
				case SOLICITUDTRANSFORMACION:
				{
					EnviarDatosTipo(socketFS, YAMA, paquete.Payload, paquete.header.tamPayload, SOLICITUDBLOQUESYAMA);
					//AGARRO LOS DATOS
					//LE PIDO A FILESYSTEM LOS BLOQUES
				}
				break;
				case SOLICITUDBLOQUESYAMA:
				{
					//Respuesta de FS
					//Hay que deserializar los datos y guardarla en la lista de t_bloque_yama
					void* datos = paquete.Payload;
					t_list* listaBloques = list_create();
					int tamanioLista = *((uint32_t*)datos);
					datos += sizeof(uint32_t);
					int i;
					for (i=0; i < tamanioLista; i++){
						t_bloque_yama* bloque = malloc(sizeof(t_bloque_yama));
						bloque->numero_bloque = ((uint32_t*)datos)[0];
						bloque->tamanio = ((uint32_t*)datos)[1];
						bloque->primera.bloque_nodo = ((uint32_t*)datos)[2];
						bloque->segunda.bloque_nodo = ((uint32_t*)datos)[3];
						datos += sizeof(uint32_t) * 4;
						bloque->primera.nombre_nodo = string_new();
						strcpy(bloque->primera.nombre_nodo, datos);
						datos += strlen(datos) + 1;
						bloque->segunda.nombre_nodo = string_new();
						strcpy(bloque->segunda.nombre_nodo, datos);
						datos += strlen(datos) + 1;
						list_add(listaBloques,bloque);
					}
					list_size(listaBloques);
					planificacion(listaBloques);

				}
				break;
			}

		}
		else
			perror("No es MASTER");

		if (paquete.Payload != NULL)
			free(paquete.Payload);

	}
	close(socketFD);

}

void availability(datosWorker* worker){
	if (!strcmp(ALGORITMO_BALANCEO, "C"))
	{
		 worker->disponibilidad = BASE;
	}
	else if(!strcmp(ALGORITMO_BALANCEO, "WC"))
	{
		//ordeno la lista por carga de trabajo de mayor a menor
		list_sort(listaWorkers, LAMBDA(bool _(void* item1, void* item2) { return ((datosWorker*)item1)->cargaDeTrabajo >= ((datosWorker*)item2)->cargaDeTrabajo;}));
		//obtengo el que mayor carga de trabajo tiene
		uint32_t WLMax = ((datosWorker*)list_get(listaWorkers, 0))->cargaDeTrabajo;
		 worker->disponibilidad = BASE + WLMax - worker->cargaDeTrabajo;
	}
	else
		perror("El algortimo no es ni Clock ni W-Clock");
}


void calcularDisponibilidad(t_list* list){
	list_iterate(list,availability);
}

bool bloqueAAsignarEsta(datosWorker* w, t_bloque_yama* bloque){
	if (w->disponibilidad > 0)
	{
		if (!strcmp(bloque->primera.nombre_nodo, w->nodo)){
			return true;
		}
		else if (!strcmp(bloque->segunda.nombre_nodo, w->nodo)){
			return true;
		}
		else{
			return false;
		}
	}
	else{
		return false;
	}

}
datosWorker* proximoWorkerDisponible(datosWorker* actual){
	void* worker = list_find(listaWorkers, LAMBDA(bool _(void* item1) { return ((datosWorker*)item1)->disponibilidad > 0 && ((datosWorker*)item1)->indice > actual->indice ;}));
	if (worker == NULL)
		return list_find(listaWorkers, LAMBDA(bool _(void* item1) { return ((datosWorker*)item1)->disponibilidad > 0 && ((datosWorker*)item1)->indice < actual->indice ;}));
	else
		return worker;
}

void planificacion(t_list* bloques){
	//calcula la disponibilidad por cada worker y la actualiza
	calcularDisponibilidad(listaWorkers);
	t_list* lista = list_take(listaWorkers, list_size(listaWorkers));
	//ordena por disponibilidad de mayor a menor
	list_sort(lista, LAMBDA(bool _(void* item1, void* item2) { return ((datosWorker*)item1)->disponibilidad >= ((datosWorker*)item2)->disponibilidad;}));
	//obtiene la mayor disponibilidad
	uint32_t disp = ((datosWorker*)list_get(lista, 0))->disponibilidad;
	//obtiene los elementos que poseen la mayor disponibilidad
	t_list* disponibles = list_filter(lista,LAMBDA(bool _(void* item1) { return ((datosWorker*)item1)->disponibilidad == disp;}));
	//ordena por cantidad tareas realizadas de menor a mayor
	list_sort(disponibles, LAMBDA(bool _(void* item1, void* item2) { return ((datosWorker*)item1)->contTareasRealizadas <= ((datosWorker*)item2)->contTareasRealizadas;}));
	//el clock ahora apunta al worker que tenga mayor disponibilidad y menor carga de trabajo
	punteroClock = list_get(disponibles, 0);
	list_destroy(disponibles);
	int i;
	for(i = 0; i < list_size(bloques); i++)
	{
		//Se obtiene el bloque actual
		t_bloque_yama* bloqueAAsignar = list_get(bloques, i);

		//loopea hasta que se logre asignar el bloque y recien ahi avanza al proximo
		bool salio = false;
		do
		{
			//se fija si está el bloque a asignar
			if (bloqueAAsignarEsta(punteroClock, bloqueAAsignar))
			{
				//si está se reduce el valor de disponibilidad
				punteroClock->disponibilidad--;
				//y se avanza el clock al siguiente worker
				avanzarPuntero(punteroClock);
				//si el valor del punteroClock ahora es 0, se debe restaurar su disponibilidad a la base y avanzar el puntero
				if (punteroClock->disponibilidad == 0){
					punteroClock->disponibilidad = BASE;
					avanzarPuntero(punteroClock);
				}
				salio = true;
			}
			//si no está
			else
			{
				//se queda loopeando hasta encontrar el proximo worker con disponibilidad mayor a 0 que posea el bloque a asignar
				//o hasta haber dado una vuelta sin encontrar uno disponible
				datosWorker* punteroAux = proximoWorkerDisponible(punteroClock);
				do {
					if (bloqueAAsignarEsta(punteroAux, bloqueAAsignar))
					{
						//si está se reduce el valor de disponibilidad
						punteroAux->disponibilidad--;
						break;
					}
					punteroAux = proximoWorkerDisponible(punteroClock);
				}
				while(punteroAux != punteroClock);
				//si dio la vuelta y no encontró ninguno se re-calculan los valores de disponibilidad de los que tienen 0 como al principio
				if (punteroAux == punteroClock)
				{
					t_list* listaAux = list_filter(listaWorkers, LAMBDA(bool _(void* item1) { return ((datosWorker*)item1)->disponibilidad == 0; }));
					calcularDisponibilidad(listaAux);
				}
				else
				{
					salio = true;
					break;
				}
			}
		}while(!salio);
	}

}


void avanzarPuntero(datosWorker* puntero){
	//si el puntero está en el ultimo, lo pongo en el primero. Sino, en el siguiente
	if (puntero->indice == (list_size(listaWorkers) - 1))
		puntero = list_find(listaWorkers, LAMBDA(bool _(void* item1) { return ((datosWorker*)item1)->indice == 0;}));
	else
		puntero = list_find(listaWorkers, LAMBDA(bool _(void* item1) { return ((datosWorker*)item1)->indice == (puntero->indice + 1) ;}));
}

int main(){
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();

	CrearListas();
	socketFS = ConectarAServidor(FS_PUERTO, FS_IP, FILESYSTEM, YAMA, RecibirHandshake);
	Paquete* paquete = malloc(sizeof(Paquete));
	pthread_t conexionFilesystem;
	pthread_create(&conexionFilesystem, NULL, (void*)RecibirPaqueteFilesystem,paquete);
	ServidorConcurrente(IP, PUERTO, YAMA, &listaHilos, &end ,accion);
	//Servidor(IP, PUERTO, YAMA, accion, RecibirPaqueteServidor);
	pthread_join(conexionFilesystem, NULL);
	free(paquete);
	LiberarListas();
	return 0;
} 
