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
			list_add(listaWorkers,worker);
		}
		else
		{
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
				case ESHANDSHAKE://TRANSFORMACION:
					//AGARRO LOS DATOS
					//LE PIDO A FILESYSTEM LOS BLOQUES
					//LOS RECIBO Y GUARDO
				{
					list_add(listaMasters, idsMaster);
					idsMaster++;
					t_list* listaBloques = list_create();
					t_bloque_yama* bloque_0 = malloc(sizeof(t_bloque_yama));
					bloque_0->primera.bloque_nodo = 3;
					bloque_0->primera.nombre_nodo = "NODO1";
					bloque_0->segunda.bloque_nodo = 4;
					bloque_0->segunda.nombre_nodo="NODO2";
					bloque_0->tamanio=1024;
					bloque_0->numero_bloque= 0;
					t_bloque_yama* bloque_1 = malloc(sizeof(t_bloque_yama));
					bloque_1->primera.bloque_nodo = 3;
					bloque_1->primera.nombre_nodo = "NODO2";
					bloque_1->segunda.bloque_nodo = 4;
					bloque_1->segunda.nombre_nodo="NODO1";
					bloque_1->tamanio=1024;
					bloque_1->numero_bloque= 1;
					t_bloque_yama* bloque_2 = malloc(sizeof(t_bloque_yama));
					bloque_2->primera.bloque_nodo = 1;
					bloque_2->primera.nombre_nodo = "NODO1";
					bloque_2->segunda.bloque_nodo = 2;
					bloque_2->segunda.nombre_nodo="NODO2";
					bloque_2->tamanio=1024;
					bloque_2->numero_bloque= 2;
					list_add(listaBloques, bloque_0);
					list_add(listaBloques, bloque_1);
					list_add(listaBloques, bloque_2);

					planificacion(listaBloques);
				}
					break;
				//case TERMINOWORKER
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


void calcularDisponibilidad(){
	list_iterate(listaWorkers,LAMBDA(void _(void* item) { availability(item);}));
}

bool bloqueAAsignarEsta(datosWorker* w){

	if (w->disponibilidad > 0)
		return true;
	else
		return false;
}


void planificacion(t_list* bloques){
	//calcula la disponibilidad por cada worker y la actualiza
	calcularDisponibilidad();
	//ordena por disponibilidad de mayor a menor
	list_sort(listaWorkers, LAMBDA(bool _(void* item1, void* item2) { return ((datosWorker*)item1)->disponibilidad >= ((datosWorker*)item2)->disponibilidad;}));
	//obtiene la mayor disponibilidad
	uint32_t disp = ((datosWorker*)list_get(listaWorkers, 0))->disponibilidad;
	//obtiene los elementos que poseen la mayor disponibilidad
	t_list* disponibles = list_filter(listaWorkers,LAMBDA(bool _(void* item1) { return ((datosWorker*)item1)->disponibilidad == disp;}));
	//ordena por cantidad tareas realizadas de menor a mayor
	list_sort(disponibles, LAMBDA(bool _(void* item1, void* item2) { return ((datosWorker*)item1)->contTareasRealizadas <= ((datosWorker*)item2)->contTareasRealizadas;}));
	//el clock ahora apunta al worker que tenga mayor disponibilidad y menor carga de trabajo
	punteroClock = list_get(disponibles, 0);
	list_destroy(disponibles);


	//HASTA ACA TIENE SENTIDO
	bloqueAAsignarEsta(punteroClock);

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
