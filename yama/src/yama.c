#include "sockets.h"


typedef struct
{
	datosWorker worker;
}
clock;

char *IP, *FS_IP, *ALGORITMO_BALANCEO;
int PUERTO, FS_PUERTO, RETARDO_PLANIFICACION, BASE;
int socketFS;
t_list* listaWorkers;
clock clock;

void obtenerValoresArchivoConfiguracion() {
	t_config* arch = config_create("yamaCFG.txt");
	IP = string_duplicate(config_get_string_value(arch, "IP"));
	PUERTO = config_get_int_value(arch, "PUERTO");
	FS_IP = string_duplicate(config_get_string_value(arch, "FS_IP"));
	FS_PUERTO = config_get_int_value(arch, "FS_PUERTO");
	RETARDO_PLANIFICACION = config_get_int_value(arch, "RETARDO_PLANIFICACION");
	ALGORITMO_BALANCEO = string_duplicate(config_get_string_value(arch, "ALGORITMO_BALANCEO"));
	PUERTO = config_get_int_value(arch, "BASE");
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
}

void LiberarListas() {
	list_destroy_and_destroy_elements(listaWorkers, free);
}

void accion(Paquete* paquete, int socket){
	if(!strcmp(paquete.header.emisor, MASTER))
	{
		switch (paquete.header.tipoMensaje)
		{
			case NUEVOWORKER:
			{

			}
				break;
			case ESDATOS:
				break;
		}

	}
	else
		perror("No es MASTER");

	if (paquete.Payload != NULL)
		free(paquete.Payload);

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

void planificacion(){
	calcularDisponibilidad();
	//ordena por disponibilidad de mayor a menor
	list_sort(listaWorkers, LAMBDA(bool _(void* item1, void* item2) { return ((datosWorker*)item1)->disponibilidad >= ((datosWorker*)item2)->disponibilidad;}));
	//obtiene la mayor disponibilidad
	uint32_t disp = ((datosWorker*)list_get(listaWorkers, 0))->disponibilidad;
	//obtiene los elementos que poseen la mayor disponibilidad
	t_list* disponibles = list_filter(listaWorkers,LAMBDA(bool _(void* item1) { return ((datosWorker*)item1)->disponibilidad == disp;}));
	//ordena por carga de trabajo de menor a mayor
	list_sort(disponibles, LAMBDA(bool _(void* item1, void* item2) { return ((datosWorker*)item1)->cargaDeTrabajo <= ((datosWorker*)item2)->cargaDeTrabajo;}));
	clock = list_get(disponibles, 0);
	list_destroy(disponibles);
}

void calcularDisponibilidad(){
	//calcula la disponibilidad por cada worker y la actualiza
	list_iterate(listaWorkers,LAMBDA(void _(void* item) { availability(item);}));
}


int main(){
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();
	CrearListas();
	socketFS = ConectarAServidor(FS_PUERTO, FS_IP, FILESYSTEM, YAMA, RecibirHandshake);
	Paquete* paquete = malloc(sizeof(Paquete));
	pthread_t conexionFilesystem;
	pthread_create(&conexionFilesystem, NULL, (void*)RecibirPaqueteFilesystem,paquete);
	Servidor(IP, PUERTO, YAMA, accion, RecibirPaqueteServidor);
	pthread_join(conexionFilesystem, NULL);
	free(paquete);
	LiberarListas();
	return 0;
} 
