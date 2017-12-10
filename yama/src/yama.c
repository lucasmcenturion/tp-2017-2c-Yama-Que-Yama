#include "sockets.h"

char *IP, *FS_IP, *ALGORITMO_BALANCEO;
int PUERTO, FS_PUERTO, RETARDO_PLANIFICACION, DISP_BASE;
int socketFS;
t_list* listaMasters;
t_list* listaWorkers;
t_list* listaHilos;
t_list* tablaDeEstados;
t_list* listaJobsxMasterxBloques;
datosWorker* punteroClock;
bool end;
pthread_mutex_t mutex_plani;
int idsMaster=0;
int indiceWorker=0;

void obtenerValoresArchivoConfiguracion() {
	t_config* arch = config_create("/home/utnso/workspace/tp-2017-2c-Yama-Que-Yama/yama/yamaCFG.txt");
	IP = string_duplicate(config_get_string_value(arch, "IP"));
	PUERTO = config_get_int_value(arch, "PUERTO");
	FS_IP = string_duplicate(config_get_string_value(arch, "FS_IP"));
	FS_PUERTO = config_get_int_value(arch, "FS_PUERTO");
	RETARDO_PLANIFICACION = config_get_int_value(arch, "RETARDO_PLANIFICACION");
	ALGORITMO_BALANCEO = string_duplicate(config_get_string_value(arch, "ALGORITMO_BALANCEO"));
	DISP_BASE = config_get_int_value(arch, "DISP_BASE");
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
				"DISP_BASE=%d\n",
				IP,
				PUERTO,
				FS_IP,
				FS_PUERTO,
				RETARDO_PLANIFICACION,
				ALGORITMO_BALANCEO,
				DISP_BASE
				);
}


void CrearListasEInicializarSemaforos() {
	listaWorkers = list_create();
	listaHilos = list_create();
	listaMasters = list_create();
	tablaDeEstados = list_create();
	listaJobsxMasterxBloques = list_create();
	pthread_mutex_init(&mutex_plani, NULL);
}

void LiberarListas() {
	list_destroy_and_destroy_elements(listaWorkers, free);
	list_destroy_and_destroy_elements(listaHilos, free);
	list_destroy_and_destroy_elements(listaMasters, free);
	list_destroy_and_destroy_elements(tablaDeEstados, free);
	list_destroy_and_destroy_elements(listaJobsxMasterxBloques, free);
	pthread_mutex_destroy(&mutex_plani);
}

void calcularDisponibilidad(t_list* list, char* ab, int db){
	void availability(void* w){
		datosWorker* worker = (datosWorker*)w;
		if (!strcmp(ab, "C"))
		{
			 worker->disponibilidad = db;
		}
		else if(!strcmp(ab, "WC"))
		{
			//ordeno la lista por carga de trabajo de mayor a menor
			list_sort(listaWorkers, LAMBDA(bool _(void* item1, void* item2) { return ((datosWorker*)item1)->cargaDeTrabajo >= ((datosWorker*)item2)->cargaDeTrabajo;}));
			//obtengo el que mayor carga de trabajo tiene
			uint32_t WLMax = ((datosWorker*)list_get(listaWorkers, 0))->cargaDeTrabajo;
			 worker->disponibilidad = db + WLMax - worker->cargaDeTrabajo;
		}
		else
			perror("El algortimo de balanceo no es ni Clock ni W-Clock");
	}
	list_iterate(list,availability);
}

//retorna el nro de copia en el que encontro o -1 si no está
int bloqueAAsignarEsta(datosWorker* w, t_bloque_yama* bloque){
	if (w->disponibilidad > 0)
	{
		if (!strcmp(bloque->primer_nombre_nodo, w->nodo)){
			return 1;
		}
		else if (!strcmp(bloque->segundo_nombre_nodo, w->nodo)){
			return 2;
		}
		else{
			return -1;
		}
	}
	else{
		return -1;
	}

}
datosWorker* proximoWorkerDisponible(datosWorker* actual){
	void* worker = list_find(listaWorkers, LAMBDA(bool _(void* item1) { return ((datosWorker*)item1)->disponibilidad > 0 && ((datosWorker*)item1)->indice > actual->indice ;}));
	if (worker == NULL)
		return list_find(listaWorkers, LAMBDA(bool _(void* item1) { return ((datosWorker*)item1)->disponibilidad > 0 && ((datosWorker*)item1)->indice < actual->indice ;}));
	else
		return worker;
}
datosWorker* avanzarPuntero(datosWorker* puntero){
	//si el puntero está en el ultimo, lo pongo en el primero. Sino, en el siguiente
	if (puntero->indice == (list_size(listaWorkers) - 1))
		return list_get(listaWorkers,0);//list_find(listaWorkers, LAMBDA(bool _(void* item1) { return ((datosWorker*)item1)->indice == 0;}));
	else
		return list_get(listaWorkers,(puntero->indice+1)); //list_find(listaWorkers, LAMBDA(bool _(void* item1) { return ((datosWorker*)item1)->indice == (puntero->indice + 1) ;}));
}

char* armarRutaTemporal(datosWorker* p, master* m, int nBloque){
	char* numeroNodo = string_substring_from(p->nodo,strlen("Nodo")); //Esto borra literalmente la cantidad de bytes que ocupe "nodo" por ende, borra "Nodo" y deja el numero como string
	char* strNJob = string_itoa(m->contJobs);
	char* strBloque = string_itoa(nBloque);
	char *ruta = string_from_format("/tmp/j%sn%sb%s",strNJob, numeroNodo, strBloque);

	free(numeroNodo);
	free(strNJob);
	free(strBloque);

	return ruta;
}

void MostrarRegistroTablaDeEstados(registroEstado* r){

	char* estado;
		switch(r->estado){
		case ENPROCESO:
			estado = "En proceso";
			break;
		case FINALIZADOOK:
			estado = "Finalizado OK";
			break;
		case ERROR:
			estado = "Error";
			break;
	}

	char* etapa;
	switch(r->etapa){
	case TRANSFORMACION:
		etapa = "Transformación";
		printf("|| Job = %i || Master = %i || %s || Bloque = %i || Etapa = %s || Archivo temporal = %s || Estado = %s ||\n\n",
				r->job, r->master, r->nodo, r->bloque, etapa, r->archivoTemporal, estado);
		break;
	case REDUCCIONLOCAL:
		etapa = "Reducción local";
		printf("|| Job = %i || Master = %i || %s || Etapa = %s || Archivo temporal = %s || Estado = %s ||\n\n",
				r->job, r->master, r->nodo, etapa, r->archivoTemporal, estado);
		break;
	case REDUCCIONGLOBAL:
			etapa = "Reducción global";
			break;
	case ALMACENAMIENTOFINAL:
			etapa = "Almacenamiento final";
			break;
	}

	fflush(stdout);
}

void EnviarBloqueAMaster(t_bloque_yama* b, datosWorker* p, master* m, int nroCopia){
	char* r = armarRutaTemporal(p,m,b->numero_bloque);
	int tamanioDatos = sizeof(uint32_t) * 3 + strlen(p->ip) + strlen(p->nodo) + strlen(r) + 3 ;
	void* datos = malloc(tamanioDatos);
	((uint32_t*)datos)[0] = nroCopia == 1 ? b->primer_bloque_nodo : b->segundo_bloque_nodo;
	((uint32_t*)datos)[1] = b->tamanio;
	((uint32_t*)datos)[2] = p->puerto;
	datos += sizeof(uint32_t) * 3;
	strcpy(datos, p->ip);
	datos += strlen(p->ip) + 1;
	strcpy(datos, p->nodo);
	datos += strlen(p->nodo) + 1;
	strcpy(datos, r);
	datos += strlen(r) + 1;
	datos -= tamanioDatos;
	EnviarDatosTipo(m->socket, YAMA, datos, tamanioDatos, SOLICITUDTRANSFORMACION);
	free(datos);
	registroEstado* registro = malloc(sizeof(registroEstado));
	registro->archivoTemporal = string_new();
	strcpy(registro->archivoTemporal, r);
	registro->bloque = b->numero_bloque;
	registro->estado = ENPROCESO;
	registro->etapa = TRANSFORMACION;
	registro->job = m->contJobs;
	registro->master = m->id;
	registro->nodo = string_new();
	registro->nodoConOtraCopia = string_new();
	strcpy(registro->nodo,p->nodo);
	strcpy(registro->nodoConOtraCopia, (nroCopia == 1? b->segundo_nombre_nodo : b->primer_nombre_nodo));
	registro->nroBloqueCopia =  (nroCopia == 1? b->segundo_bloque_nodo : b->primer_bloque_nodo);
	list_add(tablaDeEstados, registro);
	MostrarRegistroTablaDeEstados(registro);
	free(r);
	//actualizo la carga de trabajo
	p->cargaDeTrabajo++;

}

void planificacionT(t_list* bloques, master* elmaster){
	char* ALGORITMO_BALANCEO_ACTUAL = string_new();
	strcpy(ALGORITMO_BALANCEO_ACTUAL, ALGORITMO_BALANCEO);
	int RETARDO_PLANIFICACION_ACTUAL = RETARDO_PLANIFICACION;
	int DISP_BASE_ACTUAL = DISP_BASE;
	//calcula la disponibilidad por cada worker y la actualiza
	calcularDisponibilidad(listaWorkers, ALGORITMO_BALANCEO_ACTUAL, DISP_BASE_ACTUAL);
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
			int nroCopia=-1;
			if (nroCopia=(bloqueAAsignarEsta(punteroClock, bloqueAAsignar) != -1))
			{
				EnviarBloqueAMaster(bloqueAAsignar,punteroClock,elmaster, nroCopia);
				//si está se reduce el valor de disponibilidad
				punteroClock->disponibilidad--;
				//y se avanza el clock al siguiente worker
				punteroClock=avanzarPuntero(punteroClock);
				//avanzarPuntero(punteroClock);
				//si el valor del punteroClock ahora es 0, se debe restaurar su disponibilidad a la base y avanzar el puntero
				if (punteroClock->disponibilidad == 0){
					punteroClock->disponibilidad = DISP_BASE_ACTUAL;
					punteroClock=avanzarPuntero(punteroClock);
					//avanzarPuntero(punteroClock);
				}
				salio = true;
				usleep(RETARDO_PLANIFICACION_ACTUAL*1000000);
			}
			//si no está
			else
			{
				//se queda loopeando hasta encontrar el proximo worker con disponibilidad mayor a 0 que posea el bloque a asignar
				//o hasta haber dado una vuelta sin encontrar uno disponible
				datosWorker* punteroAux = proximoWorkerDisponible(punteroClock);
				do {
					if (nroCopia= (bloqueAAsignarEsta(punteroAux, bloqueAAsignar)))
					{
						EnviarBloqueAMaster(bloqueAAsignar,punteroAux,elmaster, nroCopia);
						//si está se reduce el valor de disponibilidad
						punteroAux->disponibilidad--;
						usleep(RETARDO_PLANIFICACION_ACTUAL*1000000);
						break;
					}
					punteroAux = proximoWorkerDisponible(punteroAux);
				}
				while(punteroAux != punteroClock);
				//si dio la vuelta y no encontró ninguno se re-calculan los valores de disponibilidad de los que tienen 0 como al principio
				if (punteroAux == punteroClock)
				{
					t_list* listaAux = list_filter(listaWorkers, LAMBDA(bool _(void* item1) { return ((datosWorker*)item1)->disponibilidad == 0; }));
					calcularDisponibilidad(listaAux, ALGORITMO_BALANCEO_ACTUAL, DISP_BASE_ACTUAL);
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

void RecibirPaqueteFilesystem(Paquete* paquete){
	while(RecibirPaqueteCliente(socketFS,YAMA,paquete) > 1)
	{
		if(paquete->header.tipoMensaje == NUEVOWORKER){
			void* datos = paquete->Payload;
			int tamanio = paquete->header.tamPayload;
			datosWorker* worker = malloc(sizeof(datosWorker));
			worker->puerto = *((uint32_t*)datos);
			datos += sizeof(uint32_t);
			worker->ip = string_new();
			strcpy(worker->ip, datos);
			datos+=strlen(datos) + 1;
			worker->nodo = string_new();
			strcpy(worker->nodo, datos);
			datos += strlen(datos) + 1;
			datos-=tamanio;
			worker->cargaDeTrabajo = 0;
			worker->disponibilidad = 0;
			worker->contTareasRealizadas = 0;
			worker->indice = indiceWorker;
			indiceWorker++;
			list_add(listaWorkers,worker);
		}
		if(paquete->header.tipoMensaje == LISTAWORKERS){
			void* datos = paquete->Payload;
			int listSize = *((uint32_t*)datos);
			datos += sizeof(uint32_t);
			int i;
			for (i=0; i < listSize; i++){
				datosWorker* worker = malloc(sizeof(datosWorker));
				worker->puerto = *((uint32_t*)datos);
				datos+=sizeof(uint32_t);
				worker->ip = string_new();
				strcpy(worker->ip, datos);
				datos += strlen(datos) +1;
				worker->nodo =  string_new();
				strcpy(worker->nodo, datos);
				datos += strlen(datos) +1;
				worker->cargaDeTrabajo=0;
				worker->contTareasRealizadas=0;
				worker->disponibilidad=0;
				worker->indice = i;
				list_add(listaWorkers, worker);
			}
			indiceWorker = listSize;
			datos -= paquete->header.tamPayload;
		}
		else if (paquete->header.tipoMensaje == NODODESCONECTADO)
		{
			char* nodoAEliminar = string_new();
			strcpy(nodoAEliminar,paquete->Payload);
			int indiceEliminado = ((datosWorker*)list_find(listaWorkers,LAMBDA(bool _(void* item1) { return !strcmp(((datosWorker*)item1)->nodo,nodoAEliminar);})))->indice;
			list_remove_and_destroy_by_condition(listaWorkers, LAMBDA(bool _(void* item1) { return !strcmp(((datosWorker*)item1)->nodo,nodoAEliminar);}), free);
			void actualizarIndice(void* w){
				datosWorker* worker = (datosWorker*)w;
				if (worker->indice > indiceEliminado)
					worker->indice--;
			}
			list_iterate(listaWorkers,actualizarIndice);
			indiceWorker--;


		}
		else if (paquete->header.tipoMensaje == SOLICITUDBLOQUESYAMA)
		{
			//Respuesta de FS
			//Hay que deserializar los datos y guardarla en la lista de t_bloque_yama
			void* datos = paquete->Payload;
			t_list* listaBloques = list_create();
			int tamanioLista = *((uint32_t*)datos);
			datos += sizeof(uint32_t);
			int i;
			for (i=0; i < tamanioLista; i++){
				t_bloque_yama* bloque = malloc(sizeof(t_bloque_yama));
				bloque->numero_bloque = ((uint32_t*)datos)[0];
				bloque->tamanio = ((uint32_t*)datos)[1];
				bloque->primer_bloque_nodo = ((uint32_t*)datos)[2];
				bloque->segundo_bloque_nodo = ((uint32_t*)datos)[3];
				datos += sizeof(uint32_t) * 4;
				bloque->primer_nombre_nodo = string_new();
				strcpy(bloque->primer_nombre_nodo, datos);
				datos += strlen(datos) + 1;
				bloque->segundo_nombre_nodo = string_new();
				strcpy(bloque->segundo_nombre_nodo, datos);
				datos += strlen(datos) + 1;
				list_add(listaBloques,bloque);
			}
			master* m = list_find(listaMasters,LAMBDA(bool _(void* item1) { return ((master*)item1)->socket == *((uint32_t*)datos) ;}) );
			m->contJobs++;
			pthread_mutex_lock(&mutex_plani);
			planificacionT(listaBloques, m);
			pthread_mutex_unlock(&mutex_plani);

		}
	}
	printf("La conexión de filesystem ha sido rechazada porque no se encontraba en un estado seguro.\nIntente nuevamente.");
	fflush(stdout);
	raise(SIGKILL);
}

void RealizarReplanificacion(registroEstado* reg, int socket){
	datosWorker* w = list_find(listaWorkers,LAMBDA(bool _(void* item1) { return !strcmp(((datosWorker*)item1)->nodo, reg->nodoConOtraCopia);}));
	if (w == NULL){
		printf("No ha sido posible replanificar debido a que el nodo que poseía la copia tampoco está conectado.");
		fflush(stdout);
	}
	else{
		int tamanioDatos = sizeof(uint32_t) * 3 + strlen(w->ip) + strlen(w->nodo) + strlen(reg->archivoTemporal) + 3 ;
		void* datos = malloc(tamanioDatos);
		((uint32_t*)datos)[0] = reg->nroBloqueCopia;
		((uint32_t*)datos)[1] = reg->tamanio;
		((uint32_t*)datos)[2] = w->puerto;
		datos += sizeof(uint32_t) * 3;
		strcpy(datos, w->ip);
		datos += strlen(w->ip) + 1;
		strcpy(datos, w->nodo);
		datos += strlen(w->nodo) + 1;
		strcpy(datos, reg->archivoTemporal);
		datos += strlen(reg->archivoTemporal) + 1;
		datos -= tamanioDatos;
		EnviarDatosTipo(socket, YAMA, datos, tamanioDatos, SOLICITUDTRANSFORMACION);
		free(datos);

		reg->estado = ENPROCESO;
		reg->etapa = TRANSFORMACION;
		char* aux = reg->nodo;
		reg->nodo = reg->nodoConOtraCopia;
		reg->nodoConOtraCopia = aux;
		int naux = reg->nroBloque;
		reg->nroBloque = reg->nroBloqueCopia;
		reg->nroBloqueCopia = naux;

		MostrarRegistroTablaDeEstados(reg);
		//actualizo la carga de trabajo
		w->cargaDeTrabajo++;
	}
}

void realizarRL(t_list* l, datosWorker* w, int idMaster, int idJob, int socketMaster){
	char* rutaTemporalRL = string_from_format("/tmp/Master%i-%s", idMaster,w->nodo);
	int tamanio = sizeof(uint32_t)*2 + strlen(w->nodo) + strlen(w->ip) + strlen(rutaTemporalRL) + 3 ;
	void * datos = malloc(tamanio);
	//cantidad de archivos temporales
	((uint32_t*)datos)[0] = list_size(l);
	//el puerto del worker
	((uint32_t*)datos)[1] = w->puerto;
	datos+=sizeof(uint32_t)*2;
	//ip worker
	strcpy(datos,w->ip);
	datos += strlen(w->ip) + 1;
	//nombre nodo
	strcpy(datos, w->nodo);
	datos += strlen(w->nodo)+1;
	//ruta temporal reduccion local
	strcpy(datos, rutaTemporalRL);
	datos += strlen(rutaTemporalRL) + 1;
	int i;
	for (i = 0; i < list_size(l); i++){
		registroEstado* r = list_get(l, i);
		datos -= tamanio;
		datos = realloc(datos, tamanio+strlen(r->archivoTemporal)+1);
		datos += tamanio;
		tamanio+=strlen(r->archivoTemporal)+1;
		strcpy(datos,r->archivoTemporal);
		datos+=strlen(r->archivoTemporal)+1;
	}
	datos-=tamanio;
	EnviarDatosTipo(socketMaster, YAMA, datos, tamanio, SOLICITUDREDUCCIONLOCAL);
	registroEstado* reg = malloc(sizeof(registroEstado));
	reg->archivoTemporal = string_new();
	strcpy(reg->archivoTemporal, rutaTemporalRL);
	reg->estado = ENPROCESO;
	reg->etapa = REDUCCIONLOCAL;
	reg->job = idJob;
	reg->master = idMaster;
	reg->nodo = string_new();
	strcpy(reg->nodo, w->nodo);
	list_add(tablaDeEstados, reg);
	MostrarRegistroTablaDeEstados(reg);
}

void realizarRG(registroEstado* r, int socketMaster){
	t_list* lista = list_take(listaWorkers, list_size(listaWorkers));
	list_sort(lista, LAMBDA(bool _(void* item1, void* item2) { return ((datosWorker*)item1)->cargaDeTrabajo <= ((datosWorker*)item2)->cargaDeTrabajo;}));
	datosWorker* workerEncargado = list_get(lista,0);
	char* rutaTemporalRG = string_from_format("/tmp/Master%i-final", r->master);
	t_list* nodos = list_filter(tablaDeEstados, LAMBDA(bool _(void* item1) { return ((registroEstado*)item1)->etapa == REDUCCIONLOCAL && ((registroEstado*)item1)->estado = FINALIZADOOK;}));
	int tamanio = sizeof(uint32_t) + strlen(workerEncargado->nodo) + strlen(rutaTemporalRG)+2 ;
	void * datos = malloc(tamanio);
	strcpy(datos, workerEncargado->nodo);
	datos += strlen(workerEncargado->nodo) +1;
	strcpy(datos, rutaTemporalRG);
	datos += strlen(rutaTemporalRG)+1;
	*((uint32_t*)datos) = list_size(nodos);
	datos += sizeof(uint32_t*);
	int i;
	for (i = 0; i < list_size(nodos); i++){
		registroEstado* r = list_get(nodos, i);
		datosWorker* w = list_find(listaWorkers, LAMBDA(bool _(void* item1) { return !strcmp(((datosWorker*)item1)->nodo,r->nodo); }));
		datos -= tamanio;
		datos = realloc(datos, tamanio+sizeof(uint32_t)+strlen(w->nodo) + strlen(w->ip) + strlen(r->archivoTemporal)+3);
		datos += tamanio;
		tamanio+=sizeof(uint32_t)+strlen(w->nodo) + strlen(w->ip)+strlen(r->archivoTemporal)+3;
		strcpy(datos,w->nodo);
		datos+=strlen(w->nodo)+1;
		strcpy(datos,w->ip);
		datos+=strlen(w->ip)+1;
		*((uint32_t*)datos) =  w->puerto;
		datos+=sizeof(uint32_t);
		strcpy(datos,r->archivoTemporal);
		datos+=strlen(r->archivoTemporal)+1;
	}
	datos-=tamanio;
	EnviarDatosTipo(socketMaster, YAMA, datos, tamanio, SOLICITUDREDUCCIONGLOBAL);
	/*registroEstado* reg = malloc(sizeof(registroEstado));
	reg->archivoTemporal = string_new();
	strcpy(reg->archivoTemporal, rutaTemporalRL);
	reg->estado = ENPROCESO;
	reg->etapa = REDUCCIONLOCAL;
	reg->job = idJob;
	reg->master = idMaster;
	reg->nodo = string_new();
	strcpy(reg->nodo, w->nodo);
	list_add(tablaDeEstados, reg);
	MostrarRegistroTablaDeEstados(reg);*/
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
					m->contJobs = 0;
					list_add(listaMasters, m);
					idsMaster++;
				}
					break;
				case SOLICITUDTRANSFORMACION:
				{
					paquete.Payload = realloc(paquete.Payload,paquete.header.tamPayload+sizeof(uint32_t));
					*((uint32_t*)(paquete.Payload+paquete.header.tamPayload)) = socketFD;
					EnviarDatosTipo(socketFS, YAMA, paquete.Payload, paquete.header.tamPayload+sizeof(uint32_t), SOLICITUDBLOQUESYAMA);
					//AGARRO LOS DATOS
					//LE PIDO A FILESYSTEM LOS BLOQUES
				}
				break;
				case FINTRANSFORMACION:
				{
					void* datos = paquete.Payload;
					//Recibir los datos de master:
					//Bloque
					//idJob
					//nodo
					//bool
					int bloque = ((uint32_t*)datos)[0];
					int idJob = ((uint32_t*)datos)[1];
					datos += sizeof(uint32_t*)*2;
					char* nodo = string_new();
					strcpy(nodo, datos);
					datos += strlen(datos)+1;
					bool exito = *((bool*)datos);
					datos += sizeof(bool);
					datos -= paquete.header.tamPayload;
					//actualizar el registro correspondiente
					registroEstado* r = list_find(tablaDeEstados,LAMBDA(bool _(void* item1) { return ((registroEstado*)item1)->bloque == bloque && ((registroEstado*)item1)->job == idJob;}));
					if (exito)
					{
						r->estado = FINALIZADOOK;
						datosWorker* w = list_find(listaWorkers,LAMBDA(bool _(void* item1) { return !strcmp(((datosWorker*)item1)->nodo, r->nodo);}));
						w->contTareasRealizadas++;
						MostrarRegistroTablaDeEstados(r);
						t_list* tablaFiltradaPorNodoYTransformacionYJob = list_filter(tablaDeEstados,LAMBDA(bool _(void* item1) { return !strcmp(((registroEstado*)item1)->nodo, nodo) && ((registroEstado*)item1)->etapa == TRANSFORMACION && ((registroEstado*)item1)->job == idJob;}));
						//filtra todos los registros de ese nodo que se hayen en transformacion
						//si todos están en estado finalizadook, puede hacer reducción local
						if (list_all_satisfy(tablaFiltradaPorNodoYTransformacionYJob,LAMBDA(bool _(void* item1) { return ((registroEstado*)item1)->estado == FINALIZADOOK;})))
						{
							realizarRL(tablaFiltradaPorNodoYTransformacionYJob, w, r->master, r->job, socketFD);
						}
					}
					else
					{
						r->estado = ERROR;
						MostrarRegistroTablaDeEstados(r);
						printf("Debido a un la desconexion del nodo asignado en la transformación del bloque  %i del job %i, "
								"se procederá a replanificar la misma en el nodo que posee la otra copia.", r->bloque, r->job);
						fflush(stdout);
						RealizarReplanificacion(r, socketFD);
					}
				}
				break;
				case FINREDUCCIONLOCAL:
				{
					void* datos = paquete.Payload;
					int idJob = *((uint32_t*)datos);
					datos += sizeof(uint32_t);
					char* nodo = string_new();
					strcpy(nodo, datos);
					datos += strlen(datos) + 1;
					bool exito = *((uint32_t*)datos);
					datos += sizeof(bool);
					datos -= paquete.header.tamPayload;
					//actualizar el registro correspondiente
					registroEstado* r = list_find(tablaDeEstados,LAMBDA(bool _(void* item1) { return !strcmp(((registroEstado*)item1)->nodo,nodo) && ((registroEstado*)item1)->job == idJob;}));
					if (exito)
					{
						r->estado = FINALIZADOOK;
						datosWorker* w = list_find(listaWorkers,LAMBDA(bool _(void* item1) { return !strcmp(((datosWorker*)item1)->nodo, r->nodo);}));
						w->contTareasRealizadas++;
						MostrarRegistroTablaDeEstados(r);
						t_list* tablaFiltradaPorNodoYReduccionYJob = list_filter(tablaDeEstados,LAMBDA(bool _(void* item1) { return !strcmp(((registroEstado*)item1)->nodo, nodo) && ((registroEstado*)item1)->etapa == REDUCCIONLOCAL && ((registroEstado*)item1)->job == idJob;}));
						//filtra todos los registros de ese nodo que se hayen en transformacion
						//si todos están en estado finalizadook, puede hacer reducción local
						realizarRG(r, socketFD);

					}
					else
					{
						printf("Falló la reducción local. Se abortará el job.");
						fflush(stdout);
						r->estado = ERROR;
						MostrarRegistroTablaDeEstados(r);
					}
				}
				break;
				case FINREDUCCIONGLOBAL:
				{
					void* datos = paquete.Payload;
					int idJob = *((uint32_t*)datos);
					datos += sizeof(uint32_t);
					char* nodo = string_new();
					strcpy(nodo, datos);
					datos += strlen(datos) + 1;
					bool exito = *((uint32_t*)datos);
					datos += sizeof(bool);
					datos -= paquete.header.tamPayload;
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

void rutina(int n){
	t_config* arch = config_create("/home/utnso/workspace/tp-2017-2c-Yama-Que-Yama/yama/yamaCFG.txt");
	RETARDO_PLANIFICACION = config_get_int_value(arch, "RETARDO_PLANIFICACION");
	ALGORITMO_BALANCEO = string_duplicate(config_get_string_value(arch, "ALGORITMO_BALANCEO"));
	DISP_BASE = config_get_int_value(arch, "DISP_BASE");
	printf("Se ha recargado la configuración.\nEstos son sus nuevos valores:\n");
	fflush(stdout);
	imprimirArchivoConfiguracion();
}


int main(){
	signal(SIGUSR1, rutina);
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();
	CrearListasEInicializarSemaforos();
	socketFS = ConectarAServidor(FS_PUERTO, FS_IP, FILESYSTEM, YAMA, RecibirHandshake);
	Paquete* paquete = malloc(sizeof(Paquete));
	pthread_t conexionFilesystem;
	pthread_create(&conexionFilesystem, NULL, (void*)RecibirPaqueteFilesystem,paquete);
	ServidorConcurrente(IP, PUERTO, YAMA, &listaHilos, &end ,accion);
	pthread_join(conexionFilesystem, NULL);
	free(paquete);
	LiberarListas();
	return 0;
} 
