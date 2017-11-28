#include "sockets.h"
#include <sys/mman.h>
#define TAMBLOQUE (1024*1024)
char *IP;
char *PUNTO_MONTAJE;
char *RUTA_DIRECTORIOS;
char *RUTA_NODOS;
char *RUTA_ARCHIVOS;
int PUERTO;
int socketFS;
int socketYAMA;
t_list* listaHilos;
t_list* datanodes;
bool directorios_ocupados[100];
t_dictionary *archivos_actuales;
t_dictionary *archivos_erroneos;
t_dictionary *archivos_terminados;
int index_directorio=1;
bool end;
pthread_mutex_t mutex_datanodes;
pthread_mutex_t mutex_directorio;
pthread_mutex_t mutex_directorios_ocupados;
pthread_mutex_t mutex_archivos_actuales;
pthread_mutex_t mutex_archivos_erroneos;
pthread_mutex_t mutex_escribir_archivo;

void eliminar_ea_nodos(char*nombre){
	t_config *nodos=config_create(RUTA_NODOS);
	char *nodos_actuales=malloc(100);
	nodos_actuales=config_get_string_value(nodos,"NODOS");
	nodos_actuales=realloc(nodos_actuales,strlen(nodos_actuales)+1);
	char **separado_por_comas=string_split(nodos_actuales,",");
	int cantidad_comas=0;
	int i=0;
	while(separado_por_comas[cantidad_comas]){
		cantidad_comas++;
	}
	char *nuevos_nodos=malloc(100);
	if(cantidad_comas==1){
		//tiene un solo elemento
		char *substring=malloc(100);
		substring=string_substring(separado_por_comas[0],1,strlen(separado_por_comas[0])-2);
		substring=realloc(substring,strlen(substring)+1);
		if(strcmp(nombre,substring)==0){
			config_set_value(nodos,"NODOS","");
		}
	}else{
		//tiene mas de un elemento
		while(i<cantidad_comas){
			char * substring=malloc(100);
			if(i==0){
				substring=string_substring(separado_por_comas[i],1,strlen(separado_por_comas[i])-1);
				substring=realloc(substring,strlen(substring)+1);
				if(strcmp(nombre,substring)==0){
					strcpy(nuevos_nodos,"[");
				}else{
					strcpy(nuevos_nodos,separado_por_comas[i]);
				}
			}else{
				if(i==(cantidad_comas-1)){
					substring=string_substring(separado_por_comas[i],0,strlen(separado_por_comas[i])-1);
					if(strcmp(nombre,substring)==0){
						strcat(nuevos_nodos,"]");
					}else{
						strcat(nuevos_nodos,separado_por_comas[i]);
					}

				}else{
					substring=separado_por_comas[i];
					if(strcmp(nombre,substring)==0){
						strcat(nuevos_nodos,",");
					}else{
						strcat(nuevos_nodos,",");
						strcat(nuevos_nodos,separado_por_comas[i]);
					}
				}
			}
			i++;
		}
	}
	config_set_value(nodos,"NODOS",nuevos_nodos);
	char *libre=malloc(100);
	strcpy(libre,nombre);
	strcat(libre,"Libre");
	if(config_has_property(nodos,libre)){
		int libre_nodo_a_eliminar=config_get_int_value(nodos,libre);
		int libre_total=config_get_int_value(nodos,"LIBRE");
		int libre_total_a_actualizar= libre_total>libre_nodo_a_eliminar ? libre_total-libre_nodo_a_eliminar : 0;
		char *string_libre_total_a_actualizar=malloc(100);
		sprintf(string_libre_total_a_actualizar,"%i",libre_total_a_actualizar);
		string_libre_total_a_actualizar=realloc(string_libre_total_a_actualizar,strlen(string_libre_total_a_actualizar)+1);
		config_set_value(nodos,"LIBRE",string_libre_total_a_actualizar);
		config_set_value(nodos,libre,"0");
		config_save_in_file(nodos,"/home/utnso/metadata/nodos.bin");
	}
	char *total=malloc(100);
	strcpy(total,nombre);
	strcat(total,"Total");
	if(config_has_property(nodos,total)){
		int total_nodo_a_eliminar=config_get_int_value(nodos,total);
		int total_total=config_get_int_value(nodos,"TAMANIO");
		int total_total_a_actualizar= total_total>total_nodo_a_eliminar ? total_total-total_nodo_a_eliminar : 0;
		char *string_total_total_a_actualizar=malloc(100);
		sprintf(string_total_total_a_actualizar,"%i",total_total_a_actualizar);
		string_total_total_a_actualizar=realloc(string_total_total_a_actualizar,strlen(string_total_total_a_actualizar)+1);
		config_set_value(nodos,total,"0");
		config_set_value(nodos,"TAMANIO",string_total_total_a_actualizar);
	}
	config_save_in_file(nodos,"/home/utnso/metadata/nodos.bin");
}

t_archivo_actual *obtener_elemento(t_list*lista,char*nombre_archivo){
	bool nombre_igual(t_archivo_actual*elemento){
		if(strcmp(elemento->nombre_archivo,nombre_archivo)==0){
			return true;
		}
		return false;
	}
	t_archivo_actual *elemento=malloc(sizeof(t_archivo_actual));
	elemento=list_find(lista,(void*) nombre_igual);
	return elemento;
}
void actualizar_nodos_bin(info_datanode *data){
	char *ruta_nodos=malloc(100);
	strcpy(ruta_nodos,PUNTO_MONTAJE);
	strcat(ruta_nodos,"/nodos.bin");
	ruta_nodos=realloc(ruta_nodos,strlen(ruta_nodos)+1);
	t_config *nodos=config_create(ruta_nodos);
	int tamanio_actual;
	tamanio_actual=config_get_int_value(nodos,"TAMANIO");
	tamanio_actual = tamanio_actual == 0 ? data->bloques_totales : tamanio_actual+data->bloques_totales;
	char *string_tamanio_actual=malloc(100);
	sprintf(string_tamanio_actual,"%i",tamanio_actual);
	string_tamanio_actual=realloc(string_tamanio_actual,strlen(string_tamanio_actual)+1);
	config_set_value(nodos,"TAMANIO",string_tamanio_actual);
	config_save_in_file(nodos,ruta_nodos);
	int libre;
	libre=config_get_int_value(nodos,"LIBRE");
	libre= libre==0 ? data->bloques_libres : libre+data->bloques_libres;
	char *string_libre=malloc(100);
	sprintf(string_libre,"%i",libre);
	string_libre=realloc(string_libre,strlen(string_libre)+1);
	config_set_value(nodos,"LIBRE",string_libre);
	config_save_in_file(nodos,ruta_nodos);
	char *nodos_actuales=calloc(1,100);
	nodos_actuales=config_get_string_value(nodos,"NODOS");
	if(nodos_actuales){
		char **separado_por_comas=string_split(nodos_actuales,",");
		int cantidad_comas=0;
		while(separado_por_comas[cantidad_comas]){
			cantidad_comas++;
		}
		int iterate=0;
		char *nuevos_nodos=calloc(1,300);
		while(iterate<cantidad_comas){
			if(cantidad_comas==1){
				char *substring=calloc(1,100);
				substring=string_substring(separado_por_comas[(cantidad_comas-1)],0,strlen(separado_por_comas[(cantidad_comas-1)])-1);
				substring=realloc(substring,strlen(substring)+1);
				strcpy(nuevos_nodos,substring);
				strcat(nuevos_nodos,",");
				strcat(nuevos_nodos,data->nodo);
				strcat(nuevos_nodos,"]");
				nuevos_nodos=realloc(nuevos_nodos,strlen(nuevos_nodos)+1);
			}else{
				if(iterate==0){
					strcpy(nuevos_nodos,separado_por_comas[iterate]);
					strcat(nuevos_nodos,",");
					}else{
						if(iterate==(cantidad_comas-1)){
							char *substring=calloc(1,200);
							substring=string_substring(separado_por_comas[(cantidad_comas-1)],0,strlen(separado_por_comas[(cantidad_comas-1)])-1);
							substring=realloc(substring,strlen(substring)+1);
							strcat(nuevos_nodos,substring);
							strcat(nuevos_nodos,",");
							strcat(nuevos_nodos,data->nodo);
							strcat(nuevos_nodos,"]");
							nuevos_nodos=realloc(nuevos_nodos,strlen(nuevos_nodos)+1);
						}else{
							strcat(nuevos_nodos,separado_por_comas[iterate]);
							strcat(nuevos_nodos,",");
					}
				}

			}
			iterate++;
		}
		nodos_actuales=realloc(nodos_actuales,strlen(nodos_actuales)+1);
		config_set_value(nodos,"NODOS",nuevos_nodos);
		config_save_in_file(nodos,ruta_nodos);
	}else{
		char* nodo_nuevo=calloc(1,120);
		strcpy(nodo_nuevo,"[");
		strcat(nodo_nuevo,data->nodo);
		strcat(nodo_nuevo,"]");
		nodo_nuevo=realloc(nodo_nuevo,strlen(nodo_nuevo)+1);
		config_set_value(nodos,"NODOS",nodo_nuevo);
		config_save_in_file(nodos,ruta_nodos);
	}
	config_save_in_file(nodos,ruta_nodos);
	char *nodo_actual_total=calloc(1,100);
	strcpy(nodo_actual_total,data->nodo);
	strcat(nodo_actual_total,"Total");
	char *string_bloques_totales=calloc(1,100);
	sprintf(string_bloques_totales,"%i",data->bloques_totales);
	string_bloques_totales=realloc(string_bloques_totales,strlen(string_bloques_totales)+1);
	config_set_value(nodos,nodo_actual_total,string_bloques_totales);
	config_save_in_file(nodos,ruta_nodos);
	char *nodo_actual_libre=calloc(1,100);
	strcpy(nodo_actual_libre,data->nodo);
	strcat(nodo_actual_libre,"Libre");
	char *string_bloques_libres=calloc(1,100);
	sprintf(string_bloques_libres,"%i",data->bloques_libres);
	string_bloques_libres=realloc(string_bloques_libres,strlen(string_bloques_libres)+1);
	config_set_value(nodos,nodo_actual_libre,string_bloques_libres);
	config_save_in_file(nodos,ruta_nodos);
}
t_directory **obtener_directorios(){
	int fd_directorio = open(RUTA_DIRECTORIOS,O_RDWR);
	if(fd_directorio==-1){
		printf("Error al intentar abrir el archivo");
	}else{
		struct stat mystat;
		void *directorios;
		if (fstat(fd_directorio, &mystat) < 0) {
			printf("Error al establecer fstat\n");
			close(fd_directorio);
		}else{
			directorios = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd_directorio, 0);
			if (directorios == MAP_FAILED) {
						printf("Error al mapear a memoria: %s\n", strerror(errno));
			}else{
				t_directory **directorios_yamafs=malloc(sizeof(t_directory*));
				(*directorios_yamafs)=malloc(100*sizeof(t_directory));
				memmove((*directorios_yamafs),directorios,100*sizeof(t_directory));
				munmap(directorios,mystat.st_size);
				close(fd_directorio);
				return directorios_yamafs;
			}
		}
	}
}
void guardar_directorios(t_directory **directorios){
	int fd_directorio = open(RUTA_DIRECTORIOS,O_RDWR);
	if(fd_directorio==-1){
		printf("Error al intentar abrir el archivo");
	}else{
		struct stat mystat;
		void *archivo_directorios;
		if (fstat(fd_directorio, &mystat) < 0) {
			printf("Error al establecer fstat\n");
			close(fd_directorio);
		}else{
			archivo_directorios = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd_directorio, 0);
			if (directorios == MAP_FAILED) {
						printf("Error al mapear a memoria: %s\n", strerror(errno));
			}else{
				memmove(archivo_directorios,(*directorios),100*sizeof(t_directory));
				munmap(directorios,mystat.st_size);
				close(fd_directorio);
			}
		}
	}
}
void obtenerValoresArchivoConfiguracion() {
	t_config* arch = config_create("/home/utnso/workspace/tp-2017-2c-Yama-Que-Yama/fs/filesystemCFG.txt");
	IP = string_duplicate(config_get_string_value(arch, "IP"));
	PUERTO = config_get_int_value(arch, "PUERTO");
	PUNTO_MONTAJE = string_duplicate(config_get_string_value(arch,"PUNTO_MONTAJE"));
	config_destroy(arch);
}

void imprimirArchivoConfiguracion(){
	printf(
				"Configuración:\n"
				"IP=%s\n"
				"PUERTO=%d\n"
				"PUNTO_MONTAJE=%s\n",
				IP,
				PUERTO,
				PUNTO_MONTAJE
				);
}
void sacar_datanode(int socket){
	int tiene_socket(info_datanode *datanode){
		if(datanode->socket==socket){
			eliminar_ea_nodos(datanode->nodo);
		}
		return datanode->socket!=socket;
	}
	pthread_mutex_lock(&mutex_datanodes);
	datanodes=list_filter(datanodes,(void*) tiene_socket);
	pthread_mutex_unlock(&mutex_datanodes);
}

void crear_y_actualizar_archivo(t_archivo_actual*elemento,int numero_bloque_enviado,int numero_bloque_datanode,int tamanio_bloque,int numero_copia,char*nombre_nodo,char*nombre_archivo){
	//TODO CREAR Y ACTUALIZAR ARCHIVOS
	//pthread_mutex_lock(&mutex_escribir_archivo);
	struct stat mystat;
	//pthread_mutex_init(&elemento->mutex,NULL);
	//pthread_mutex_lock(&elemento->mutex);
	char *index_directorio_padre=malloc(100);
	index_directorio_padre=integer_to_string(elemento->index_padre);
	//pthread_mutex_unlock(&mutex_archivos_actuales);
	index_directorio_padre=realloc(index_directorio_padre,strlen(index_directorio_padre)+1);
	char *ruta_directorio=malloc(100);
	strcpy(ruta_directorio,PUNTO_MONTAJE);
	strcat(ruta_directorio,"/archivos/");
	strcat(ruta_directorio,index_directorio_padre);
	ruta_directorio=realloc(ruta_directorio,strlen(ruta_directorio)+1);
	if (stat(ruta_directorio, &mystat) == -1) {
	    mkdir(ruta_directorio, 0700);
	}
	free(ruta_directorio);
	ruta_directorio=calloc(1,100);
	strcpy(ruta_directorio,PUNTO_MONTAJE);
	strcat(ruta_directorio,"/archivos/");
	strcat(ruta_directorio,index_directorio_padre);
	strcat(ruta_directorio,"/");
	strcat(ruta_directorio,nombre_archivo);
	ruta_directorio=realloc(ruta_directorio,strlen(ruta_directorio)+1);
	bool primera_vez=false;
	if(access(ruta_directorio, F_OK ) == -1 ){
		//no existe el archivo, se lo crea
		FILE *f=fopen(ruta_directorio,"w");
		fclose(f);
		primera_vez=true;
	}
	t_config *archivo=config_create(ruta_directorio);
	char *key=malloc(100);
	strcpy(key,"BLOQUE");
	strcat(key,integer_to_string(numero_bloque_enviado));
	strcat(key,"COPIA");
	strcat(key,integer_to_string(numero_copia));
	key=realloc(key,strlen(key)+1);
	char *value=malloc(100);
	strcpy(value,"[");
	strcat(value,nombre_nodo);
	strcat(value,",");
	strcat(value,integer_to_string(numero_bloque_datanode));
	strcat(value,"]");
	value=realloc(value,strlen(value)+1);
	config_set_value(archivo,key,value);
	config_save_in_file(archivo,ruta_directorio);
	char *tamanio=malloc(100);
	strcpy(tamanio,"BLOQUE");
	strcat(tamanio,integer_to_string(numero_bloque_enviado));
	strcat(tamanio,"BYTES");
	int tamanio_anterior=0;
	if(!config_has_property(archivo,tamanio)){
		tamanio_anterior=tamanio_bloque;
		config_set_value(archivo,tamanio,integer_to_string(tamanio_bloque));
		config_save_in_file(archivo,ruta_directorio);
	}
	if(config_has_property(archivo,"TAMANIO")){
		int tamanio_total=config_get_int_value(archivo,"TAMANIO");
		printf("Tamaño total %i,tamanio de bloque %i, bloque %i copia %i \n",tamanio_total,tamanio_bloque,numero_bloque_enviado,numero_copia);
		tamanio_total+=tamanio_anterior;
		printf("Tamaño total %i \n",tamanio_total);
		config_set_value(archivo,"TAMANIO",integer_to_string(tamanio_total));
		config_save_in_file(archivo,ruta_directorio);
	}else{
		config_set_value(archivo,"TAMANIO",integer_to_string(tamanio_bloque));
		config_save_in_file(archivo,ruta_directorio);
	}
	char *tipo_archivo=malloc(20);
	if(primera_vez){
		if(elemento->tipo==0){
			strcpy(tipo_archivo,"TEXTO");
		}else{
			strcpy(tipo_archivo,"BINARIO");
		}
		tipo_archivo=realloc(tipo_archivo,strlen(tipo_archivo)+1);
		config_set_value(archivo,"TIPO",tipo_archivo);
	}
	config_save_in_file(archivo,ruta_directorio);
	//pthread_mutex_unlock(&elemento->mutex);
	//pthread_mutex_unlock(&mutex_escribir_archivo);
}
void eliminar_archivo(char*nombre_nodo,char*nombre_archivo){
	t_list*lista=dictionary_get(archivos_actuales,nombre_nodo);
	bool tiene_nombre(t_archivo_actual* archivo){
		if(strcmp(archivo->nombre_archivo,nombre_archivo)==0){
			return true;
		}
		return false;
	}
	t_archivo_actual *archivo=malloc(sizeof(t_archivo_actual));
	archivo=list_remove_by_condition(lista,(void*) tiene_nombre);
	//free(archivo);
	dictionary_put(archivos_actuales,nombre_nodo,lista);
}
void accion(void* socket){
	int socketFD = *(int*)socket;
	Paquete paquete;
	while (RecibirPaqueteServidor(socketFD, FILESYSTEM, &paquete) > 0){
		if(!strcmp(paquete.header.emisor, DATANODE))
		{
			switch (paquete.header.tipoMensaje)
			{
				case NUEVOWORKER:
					{
						void* datos = paquete.Payload;
						int tamanio = paquete.header.tamPayload;
						//EnviarDatosTipo(socketYAMA, FILESYSTEM, datos, tamanio, NUEVOWORKER);
					}
					break;
				case IDENTIFICACIONDATANODE:
				{
					void *datos=paquete.Payload;
					info_datanode *data=calloc(1,sizeof(info_datanode));
					uint32_t size_databin=*((uint32_t*)datos);
					datos += sizeof(uint32_t);
					data->nodo = malloc(110);
					strcpy(data->nodo, datos);
					data->nodo=realloc(data->nodo,strlen(data->nodo)+1);
					datos-=sizeof(uint32_t);
					data->socket=socketFD;
					char *path=malloc(200);
					strcpy(path,PUNTO_MONTAJE);
					strcat(path,"/bitmaps/");
					strcat(path,data->nodo);
					strcat(path,".dat");
					//verifico si existen sus estructuras administrativas
					if(access(path, F_OK ) == -1 ){
						//no tiene un bitmap,es archivo nuevo
						FILE *f=fopen(path,"w");
						fclose(f);
						int size_bitarray=(int)(size_databin/TAMBLOQUE)%8>0 ? ((int)((int)(size_databin/TAMBLOQUE)/8))+1 : (int)(size_databin/TAMBLOQUE);
						truncate(path,size_bitarray);
						int bitmap = open(path,O_RDWR);
						struct stat mystat;
						void *bmap;
						if (fstat(bitmap, &mystat) < 0) {
							printf("Error al establecer fstat\n");
							close(bitmap);
						}else{
							bmap = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, bitmap, 0);
							if (bmap == MAP_FAILED) {
										printf("Error al mapear a memoria: %s\n", strerror(errno));
							}else{
								t_bitarray *bitarray = bitarray_create_with_mode(bmap, size_bitarray, MSB_FIRST);
								//cuando se lo crea, se inicializa todos con 0, es decir, todos los bloques libres
								munmap(bmap,mystat.st_size);
								close(bitmap);
								bitarray_destroy(bitarray);
								data->bloques_libres=(int)(size_databin/TAMBLOQUE);
								data->bloques_totales=(int)(size_databin/TAMBLOQUE);
							}
						}
					}else{
						//ya existe un bitmap, alguna vez este nodo se conecto
						int bitmap = open(path,O_RDWR);
						struct stat mystat;
						void *bmap;
						if (fstat(bitmap, &mystat) < 0) {
							printf("Error al establecer fstat\n");
							close(bitmap);
						}else{
							bmap = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, bitmap, 0);
							if (bmap == MAP_FAILED) {
										printf("Error al mapear a memoria: %s\n", strerror(errno));
							}else{
								int size_bitarray=(int)(size_databin/TAMBLOQUE)%8>0 ? ((int)((int)(size_databin/TAMBLOQUE)/8))+1 : (int)(size_databin/TAMBLOQUE);
								t_bitarray *bitarray =bitarray_create_with_mode(bmap,size_bitarray,MSB_FIRST);
								int i;
								int bloques_ocupados=0;
								for (i=0 ; i < (int)(size_databin/TAMBLOQUE); ++i) {
									if(bitarray_test_bit(bitarray,i)==1){
										bloques_ocupados++;
									}
								}
								data->bloques_totales=(int)(size_databin/TAMBLOQUE);
								data->bloques_libres=data->bloques_totales-bloques_ocupados;
								munmap(bmap,mystat.st_size);
								close(bitmap);
							}
						}
					}
					actualizar_nodos_bin(data);
					pthread_mutex_lock(&mutex_datanodes);
					list_add(datanodes,data);
					pthread_mutex_unlock(&mutex_datanodes);
				}
				break;
				case RESULOPERACION:
				{

					//recibimos los datos


					void *datos=paquete.Payload;
					uint32_t numero_bloque_datanode;
					uint32_t resultado;
					uint32_t tamanio_bloque;
					uint32_t numero_copia;
					uint32_t numero_bloque_enviado;
					char *nombre_nodo=malloc(100);
					char *nombre_archivo=malloc(100);

					numero_bloque_datanode = *((uint32_t*)datos);
					datos+=sizeof(uint32_t);
					numero_copia = *((uint32_t*)datos);
					datos+=sizeof(uint32_t);
					resultado=*((uint32_t*)datos);
					datos+=sizeof(uint32_t);
					tamanio_bloque=*((uint32_t*)datos);
					datos+=sizeof(uint32_t);
					numero_bloque_enviado=*((uint32_t*)datos);
					datos+=sizeof(uint32_t);
					strcpy(nombre_nodo,datos);
					nombre_nodo=realloc(nombre_nodo,strlen(nombre_nodo)+1);
					datos+=strlen(nombre_nodo)+1;
					strcpy(nombre_archivo,datos);
					nombre_archivo=realloc(nombre_archivo,strlen(nombre_archivo)+1);
					datos+=strlen(nombre_archivo)+1;
					pthread_mutex_lock(&mutex_archivos_actuales);
					t_list*archivos=dictionary_get(archivos_actuales,nombre_nodo);
					t_archivo_actual *elemento=obtener_elemento(archivos,nombre_archivo);
					pthread_mutex_unlock(&mutex_archivos_actuales);
					if(dictionary_has_key(archivos_terminados,nombre_archivo)){
						//termino el archivo,hay que eliminarlo y sacarlo de la lista de archivos actuales
						//TODO ELIMINAR ARCHIVOS DE ARCHIVOS ACTUALES PORQUE YA TERMINO
						pthread_mutex_lock(&mutex_archivos_actuales);
						eliminar_archivo(nombre_nodo,nombre_archivo);
						pthread_mutex_unlock(&mutex_archivos_actuales);
					}else{
						if(resultado==1 &&  !dictionary_has_key(archivos_erroneos,nombre_archivo)){
							//escribir y crear directorio y archivo
							pthread_mutex_lock(&mutex_archivos_actuales);
							elemento->total_bloques-=1;
							/*if(numero_bloque_enviado==0 && numero_copia==0){
								char*ruta=malloc(100);
								strcpy(ruta,RUTA_ARCHIVOS);
								strcat(ruta,"/");
								strcat(ruta,integer_to_string(elemento->index_padre));
								strcat(ruta,"/");
								strcat(ruta,nombre_archivo);
								if(access(ruta, F_OK ) != -1 ){
									remove(ruta);
								}
								free(ruta);
							}
							*/
							crear_y_actualizar_archivo(elemento,numero_bloque_enviado,numero_bloque_datanode,tamanio_bloque,numero_copia,nombre_nodo,nombre_archivo);
							pthread_mutex_unlock(&mutex_archivos_actuales);
						}else{
							pthread_mutex_lock(&mutex_archivos_actuales);
							elemento->total_bloques=-1;
							pthread_mutex_unlock(&mutex_archivos_actuales);
							//eliminar lo que se hizo en el archivo
						}
					}
				}
				break;
			}
		}
		else if (!strcmp(paquete.header.emisor, YAMA))
		{
			socketYAMA = socketFD;
			switch (paquete.header.tipoMensaje)
			{
				case ESDATOS:
					//switch (paquete.Payload)
					//{
						//		case LEERARCHIVO:
									//IniciarPrograma(DATOS[1],DATOS[2],socketFD);
							//	break;
								//case ALMACENARARCHIVO:
									//SolicitarBytes(DATOS[1],DATOS[2],DATOS[3],DATOS[4],socketFD);
								//break;
					//}
					break;
			}

		}
		else if (!strcmp(paquete.header.emisor, WORKER))
		{

			switch (paquete.header.tipoMensaje)
			{
				case ESDATOS:{
					void* datos = paquete.Payload;
					//AlmacenarArchivo();
					}
					break;

			}
		}
		else
			perror("No es ni YAMA ni DATANODE NI WORKER");

		if (paquete.Payload != NULL)
			free(paquete.Payload);
	}
	close(socketFD);
	sacar_datanode(socketFD);

}
void crear_y_actualizar_ocupados(char *ruta_directorio){
	t_directory **directorios=obtener_directorios();
	t_directory *aux=(*directorios);
	actualizar_directorios_ocupados(aux);
	memmove(aux,crear(aux,ruta_directorio),100*sizeof(t_directory));
	guardar_directorios(directorios);
	free((*directorios));
	free(directorios);
}
void actualizar_directorios_ocupados(t_directory aux[100]){
	int i;
	for (i = 0; i < 100; ++i) {
		if(strcmp(aux[i].nombre,"/0")==0){
			directorios_ocupados[i]=false;
		}else{
			directorios_ocupados[i]=true;
		}
	}
}
void actualizar_index_directorios(t_directory aux[100]){
	int i;
	for (i= 0; i < 100; ++i) {
		if((strcmp(aux[i].nombre,"/0")!=0) && (aux[i].index > index_directorio)){
			index_directorio=aux[i].index+1;
		}
	}
}
crear(t_directory aux[100],char *ruta){
	//verifica si existe, y si no existe, lo crea;
	pthread_mutex_lock(&mutex_directorio);
	//para que al momento de asignarl
	actualizar_index_directorios(aux);
	pthread_mutex_unlock(&mutex_directorio);
	char **separado_por_barras=string_split(ruta,"/");
	int i=0;
	int padre_anterior=0;
	while(separado_por_barras[i]){
		if(!(existe(separado_por_barras[i],padre_anterior,aux))){
			int index_array=primer_index_libre();
			if(index_array!=-1){
				aux[index_array].padre=padre_anterior;
				strcpy(aux[index_array].nombre,separado_por_barras[i]);
				pthread_mutex_lock(&mutex_directorios_ocupados);
				directorios_ocupados[index_array]=true;
				pthread_mutex_unlock(&mutex_directorios_ocupados);
				pthread_mutex_lock(&mutex_directorio);
				aux[index_array].index=index_directorio;
				index_directorio++;
				pthread_mutex_unlock(&mutex_directorio);
			}else{
				printf("No hay mas lugar libre");
			}
		}
		padre_anterior=obtener_index_directorio(separado_por_barras[i],padre_anterior,aux);
		i++;
	}
	return aux;
}
int primer_index_libre(){
	int i;
	for (i = 0; i < 100; ++i) {
		if(directorios_ocupados[i]==false){
			return i;
		}
	}
	return -1;
}
obtener_index_directorio(char *nombre,int index_padre,t_directory aux[100]){
	int i;
	for (i=0; i < 100; i++) {
		if((aux[i].padre==index_padre) && (strcmp(aux[i].nombre,nombre)==0)){
			return aux[i].index;
		}
	}
}
existe(char*nombre,int index_padre,t_directory aux[100]){
	int i;
	for(i=0;i<100;i++){
		if((aux[i].padre==index_padre)  && (strcmp(aux[i].nombre,nombre)==0)){
			return true;
		}
	}
	return false;

}

bool existe_ruta(char *ruta_fs){
	char *ruta=calloc(1,strlen("/home/utnso/metadata/directorios.dat")+1);
	strcpy(ruta,"/home/utnso/metadata/directorios.dat");
	int fd_directorio = open(ruta,O_RDWR);
	if(fd_directorio==-1){
		printf("Error al intentar abrir el archivo");
		return 0;
	}else{
		struct stat mystat;
		void *directorios;
		if (fstat(fd_directorio, &mystat) < 0) {
			printf("Error al establecer fstat\n");
			close(fd_directorio);
			return 0;
		}
		directorios = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd_directorio, 0);
		if (directorios == MAP_FAILED) {
					printf("Error al mapear a memoria: %s\n", strerror(errno));
					return 0;
		}else{
			t_directory aux[100];
			memmove(aux,directorios,100*sizeof(t_directory));
			char **separado_por_barras=string_split(ruta_fs,"/");
			int cantidad=0;
			while(separado_por_barras[cantidad]){
				cantidad++;
			}
			if(cantidad==1){
				//ruta con un solo directorio
				return existe(separado_por_barras[cantidad-1],-1,aux);
			}else{
				//ruta formada por mas de un directorio
				int index_padre_anterior=0;
				int i=0;
				cantidad--;
				while(cantidad>=0){
					if(i==0){
						if(!(existe(separado_por_barras[i],index_padre_anterior,aux))){
							munmap(directorios,mystat.st_size);
							close(fd_directorio);
							return false;
						}
					}else{
						if(!(existe(separado_por_barras[i],index_padre_anterior,aux))){
							munmap(directorios,mystat.st_size);
							close(fd_directorio);
							return false;
						}

					}
					index_padre_anterior=obtener_index_directorio(separado_por_barras[i],index_padre_anterior,aux);
					i++;
					cantidad--;
				}
				munmap(directorios,mystat.st_size);
				close(fd_directorio);
				return true;
			}
		}
	}
}
bool existe_archivo(char *nombre_archivo,int index_directorio_padre){
	char*ruta=malloc(200);
	strcpy(ruta,PUNTO_MONTAJE);
	strcat(ruta,"/archivos/");
	strcat(ruta,integer_to_string(index_directorio_padre));
	strcat(ruta,"/");
	strcat(ruta,nombre_archivo);
	ruta=realloc(ruta,strlen(ruta)+1);
	if(access(ruta, F_OK ) != -1 ){
		free(ruta);
		return true;
	}
	free(ruta);
	return false;
}
void rmtree(const char path[]){
    size_t path_len;
    char *full_path;
    DIR *dir;
    struct stat stat_path, stat_entry;
    struct dirent *entry;
    stat(path, &stat_path);
    // if path does not exists or is not dir - exit with status -1
    if (S_ISDIR(stat_path.st_mode) == 0) {
        fprintf(stderr, "%s: %s\n", "Is not directory", path);
        exit(-1);
    }
    // if not possible to read the directory for this user
    if ((dir = opendir(path)) == NULL) {
        fprintf(stderr, "%s: %s\n", "Can`t open directory", path);
        exit(-1);
    }
    // the length of the path
    path_len = strlen(path);
    // iteration through entries in the directory
    while ((entry = readdir(dir)) != NULL) {
        // skip entries "." and ".."
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;
        // determinate a full path of an entry
        full_path = calloc(path_len + strlen(entry->d_name) + 1, sizeof(char));
        strcpy(full_path, path);
        strcat(full_path, "/");
        strcat(full_path, entry->d_name);
        // stat for the entry
        stat(full_path, &stat_entry);
        // recursively remove a nested directory
        if (S_ISDIR(stat_entry.st_mode) != 0) {
            rmtree(full_path);
            continue;
        }
        // remove a file object
        unlink(full_path);
        rmdir(path) ;
        /*if (unlink(full_path) == 0)
            printf("Removed a file: %s\n", full_path);
        else
            printf("Can`t remove a file: %s\n", full_path);
         */
    }
    // remove the devastated directory and close the object of it
    /*if (rmdir(path) == 0)
        printf("Removed a directory: %s\n", path);
    else
        printf("Can`t remove a directory: %s\n", path);
	*/
    closedir(dir);
}
void consola() {
	char * linea;
	while(true) {
		linea = readline(">> ");
		if(linea)
		  add_history(linea);
		if(!strcmp(linea, "format")){
			if(access(RUTA_DIRECTORIOS, F_OK ) == -1 ){
				FILE *f=fopen(RUTA_DIRECTORIOS,"w");
				fclose(f);
			}
			crear_directorio();
			setear_directorio(0,0,"root",-1);
			rmtree(RUTA_ARCHIVOS);
			mkdir(RUTA_ARCHIVOS,0700);
		}
		else if(!strncmp(linea, "rm ", 3))
		{
			linea += 3;
			if(!strncmp(linea, "-d", 2)){

			}
			else if(!strncmp(linea, "-b", 2))
				printf("remover bloque\n");
			else
				printf("remover archivo\n");
			linea -= 3;
		}
		else if(!strncmp(linea, "rename ", 7)){
			char **array_input=string_split(linea," ");
			//TODO rename
			if(!array_input[0] || !array_input[1] || !array_input[2]){
				printf("Error, verificar parametros\n");
				fflush(stdout);
			}else{
				char **separado_por_puntos=string_split(array_input[1],".");
				if(separado_por_puntos[1]){
					//es un archivo
				}else{
					//es un directorio
					if(!existe_ruta(array_input[1])){
						printf("El directorio no existe,creelo por favor\n");
						fflush(stdout);
					}else{
						t_directory **directorios=obtener_directorios();
						t_directory *aux=(*directorios);
						char **separado_por_barras=string_split(array_input[1],"/");
						int i=0;
						int padre_anterior=0;
						int index_ultimo_directorio;
						while(separado_por_barras[i]){
							if(!(separado_por_barras[i+1])){
								index_ultimo_directorio=obtener_index_directorio(separado_por_barras[i],padre_anterior,aux);
							}else{
								padre_anterior=obtener_index_directorio(separado_por_barras[i],padre_anterior,aux);
							}
							i++;
						}
						int var;
						for (var = 1; var < 100; ++var){
							if(aux[var].index==index_ultimo_directorio){
								strcpy(aux[var].nombre,array_input[2]);
								break;
							}
						}
						guardar_directorios(directorios);
						free((*directorios));
						free(directorios);
					}
				}
			}
		}
		else if(!strncmp(linea, "mv ", 3))
			printf("mover\n");
		else if(!strncmp(linea, "cat ", 4))
			printf("muestra archivo\n");
		else if(!strncmp(linea, "mkdir ", 6)){
			printf("crea directorio\n");
			char **array_input=string_split(linea," ");
			//TODO
			if(!array_input[0] || !array_input[1]){
				printf("Error, verificar parametros\n");
				fflush(stdout);
			}
			if(existe_ruta(array_input[1])){
				printf("El directorio ya existe!\n");
				fflush(stdout);
			}else{
				printf("El directorio no existe, se creará\n");
				fflush(stdout);
				crear_y_actualizar_ocupados(array_input[1]);
				printf("Directorio creado correctamente\n");
				fflush(stdout);
			}
		}
		else if(!strncmp(linea, "cpfrom ", 7)){
			/*
			 * cpfrom	[path_archivo_origen]	[directorio_yamafs]	 [tipo_archivo]	-	Copiar	un	archivo	local	al	yamafs,
				siguiendo	los	lineamientos	en	la	operaciòn	Almacenar	Archivo,	de	la	Interfaz	del
				FileSystem.
			 * */
			char **array_input=string_split(linea," ");
			int cantidad=0;
			while(1){
				if(array_input[cantidad]){
					cantidad++;
				}else{
					break;
				}
			}
			if(!array_input[1] || !array_input[2] || cantidad>4){
				printf("Error, verifique parámetros\n");
				fflush(stdout);
			}
			char *path_archivo=malloc(strlen(array_input[1])+1);
			strcpy(path_archivo,array_input[1]);
			char *nombre_archivo=malloc(100);
			nombre_archivo=obtener_nombre_archivo(path_archivo);
			char *directorio_yama=malloc(strlen(array_input[2])+1);
			strcpy(directorio_yama,array_input[2]);
			if(!existe_ruta(directorio_yama)){
				printf("El directorio no existe, se creará\n");
				fflush(stdout);
				crear_y_actualizar_ocupados(directorio_yama);
			}
			t_directory **directorios=obtener_directorios();
			t_directory *aux=(*directorios);
			char **separado_por_barras=string_split(directorio_yama,"/");
			int iterador=0;
			int padre_anterior=0;
			int index_directorio_padre;
			while(separado_por_barras[iterador]){
				if(!(separado_por_barras[iterador+1])){
					index_directorio_padre=obtener_index_directorio(separado_por_barras[iterador],padre_anterior,aux);
				}else{
					padre_anterior=obtener_index_directorio(separado_por_barras[iterador],padre_anterior,aux);
				}
				iterador++;
			}
			free((*directorios));
			free(directorios);
			int tipo_archivo = atoi(array_input[3]);
			t_list *bloques_a_enviar  = list_create();
			int archivo = open(path_archivo, O_RDWR);
			struct stat archivo_stat;
			void *data;
			fstat(archivo,&archivo_stat);
			int size_aux=archivo_stat.st_size;
			data = mmap(0,archivo_stat.st_size,PROT_READ,MAP_SHARED, archivo,0);
			bool condicion=true;
			if(tipo_archivo==0){
				//archivo de texto
				int i=TAMBLOQUE-1;
				while(size_aux>TAMBLOQUE){
					if(((char*)data)[i]=='\n'){
						void *datos_bloque=calloc(1,i+1);
						memcpy(datos_bloque,data,i+1);
						bloque *bloque_add=calloc(1,sizeof(bloque));
						bloque_add->datos=datos_bloque;
						bloque_add->tamanio=i+1;
						list_add(bloques_a_enviar,bloque_add);
						data+=i+1;
						size_aux-=i+1;
					}else{
						i--;
					}
				}
				if(condicion){
					void *datos_bloque=calloc(1,size_aux);
					memcpy(datos_bloque,data,size_aux);
					bloque *bloque_add=calloc(1,sizeof(bloque));
					bloque_add->datos=datos_bloque;
					bloque_add->tamanio=size_aux;
					list_add(bloques_a_enviar,bloque_add);
				}
			}else{
				//archivo binario
				if(size_aux>TAMBLOQUE){
					//tamaño archivo mayor al de un bloque
					while(size_aux>TAMBLOQUE){
						void *datos_bloque=calloc(1,TAMBLOQUE);
						memcpy(datos_bloque,data,TAMBLOQUE);
						bloque *bloque_add=calloc(1,sizeof(bloque));
						bloque_add->datos=datos_bloque;
						bloque_add->tamanio=TAMBLOQUE;
						list_add(bloques_a_enviar,bloque_add);
						size_aux-=TAMBLOQUE;
						data+=TAMBLOQUE;
					}
					if(!(size_aux>TAMBLOQUE)){
						void *datos_bloque=calloc(1,size_aux);
						memcpy(datos_bloque,data,size_aux);
						bloque *bloque_add=calloc(1,sizeof(bloque));
						bloque_add->datos=datos_bloque;
						bloque_add->tamanio=size_aux;
						list_add(bloques_a_enviar,bloque_add);
					}
				}else{
					//tamaño archivo menor a un bloque
					void *datos_bloque=calloc(1,size_aux);
					memcpy(datos_bloque,data,size_aux);
					bloque *bloque_add=calloc(1,sizeof(bloque));
					bloque_add->datos=datos_bloque;
					bloque_add->tamanio=size_aux;
					list_add(bloques_a_enviar,bloque_add);
				}
			}
			munmap(data,archivo_stat.st_size);
			close(archivo);
			if(!(enviarBloques((t_list*)bloques_a_enviar,nombre_archivo,index_directorio_padre,tipo_archivo))){
				break;
			}
		}
		else if(!strncmp(linea, "cpto ", 5))
			printf("copiando a\n");
		else if(!strncmp(linea, "cpblock ", 8))
			printf("copia de bloque\n");
		else if(!strncmp(linea, "md5 ", 4))
			printf("MD5\n");
		else if(!strncmp(linea, "ls ", 3)){
			char **array_input=string_split(linea," ");
			//TODO ls
			if(!array_input[0] || !array_input[1]){
				printf("Error, verificar parametros\n");
				fflush(stdout);
			}
			if(!existe_ruta(array_input[1])){
				printf("El directorio no existe, ingrese un directorio valido\n");
			}else{
				mostrar_directorios_hijos(array_input[1]);
			}
		}
		else if(!strncmp(linea, "info ", 5))
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
	}
}
void mostrar_directorios_hijos(char *path){
	char *ruta=calloc(1,strlen("/home/utnso/metadata/directorios.dat")+1);
	strcpy(ruta,"/home/utnso/metadata/directorios.dat");
	int fd_directorio = open(ruta,O_RDWR);
	if(fd_directorio==-1){
		printf("Error al intentar abrir el archivo");
	}else{
		struct stat mystat;
		void *directorios;
		if (fstat(fd_directorio, &mystat) < 0) {
			printf("Error al establecer fstat\n");
			close(fd_directorio);
		}else{
			directorios = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd_directorio, 0);
			if (directorios == MAP_FAILED) {
				printf("Error al mapear a memoria: %s\n", strerror(errno));
			}else{
				t_directory aux[100];
				memmove(aux,directorios,100*sizeof(t_directory));
				char **separado_por_barras=string_split(path,"/");
				int cantidad=0;
				while(separado_por_barras[cantidad]){
					cantidad++;
				}
				int i=0;
				int padre_anterior=0;
				int index_actual;
				while(separado_por_barras[i]){
					if(!(separado_por_barras[i+1])){
						index_actual=obtener_index_directorio(separado_por_barras[i],padre_anterior,aux);
					}else{
						padre_anterior=obtener_index_directorio(separado_por_barras[i],padre_anterior,aux);
					}
					i++;
				}
				mostrar_directorios(index_actual,aux);
			}
		}
	}
}

void mostrar_directorios(int index,t_directory aux[100]){
	int i;
	for (i = 0; i < 100; ++i) {
		if(aux[i].padre == index){
			char *nombre=malloc(100);
			strcpy(nombre,aux[i].nombre);
			nombre=realloc(nombre,strlen(aux[i].nombre)+1);
			printf("%s\n",nombre);
			free(nombre);
		}
	}
}
bool esta_disponible(info_datanode *un_datanode){
	char *ruta=calloc(1,120);
	strcpy(ruta,PUNTO_MONTAJE);
	strcat(ruta,"/nodos.bin");
	ruta=realloc(ruta,strlen(ruta)+1);
	t_config *nodos=config_create(ruta);
	int libre;
	char *nombre_libre=calloc(1,100);
	strcpy(nombre_libre,un_datanode->nodo);
	strcat(nombre_libre,"Libre");
	nombre_libre=realloc(nombre_libre,strlen(nombre_libre)+1);
	libre=config_has_property(nodos,nombre_libre);
	free(nombre_libre);
	return libre>0;
}


bool verificar_existencia_lista_archivos(t_list*lista_archivos,char*nombre){
	int existe_nombre(t_archivo_actual *archivo){
		if(strcmp(archivo->nombre_archivo,nombre)==0){
			return true;
		}
		return false;
	}
	return list_any_satisfy(lista_archivos,(void*) existe_nombre);
}
t_list *obtener(t_list *lista,char*nombre){
	int existe_nombre(t_archivo_actual *archivo){
		if(strcmp(archivo->nombre_archivo,nombre)==0){
			return true;
		}
		return false;
	}
	t_list *aux=list_filter(lista,(void*) existe_nombre);
	return aux;
}
t_datanode_a_enviar*datanodes_disponibles(info_datanode*datanode){
	t_datanode_a_enviar *nuevo=malloc(sizeof(t_datanode_a_enviar));
	nuevo->nombre=malloc(100);
	strcpy(nuevo->nombre,datanode->nodo);
	nuevo->nombre=realloc(nuevo->nombre,strlen(nuevo->nombre)+1);
	nuevo->cantidad=datanode->bloques_libres;
	nuevo->socket=datanode->socket;
	return nuevo;
}
int ceilDivision(int tamanio,int tamanioPagina)
{
	double cantidadPaginas;
	int cantidadReal;
	cantidadPaginas=(tamanio+tamanioPagina-1)/tamanioPagina;
 	 return cantidadPaginas;

}
int se_puede_enviar(t_list*datanodes,int bloques_a_enviar){
	if(list_size(datanodes)>=2){
		int var;
		int contador=0;
		for (var=list_size(datanodes); var >= 2; --var){
			int i=0;
			while(i<list_size(datanodes)){
				t_datanode_a_enviar *un_datanode=list_get(datanodes,i);
				if(un_datanode->cantidad>=ceilDivision(bloques_a_enviar*2,var)){
					contador++;
				}
				i++;
			}
			if(contador>=var){
				return var;
			}else{
				contador=0;
			}
		}
		return 0;
	}else{
		return 0;
	}
}
t_list*obtener_disponibles(t_list*datanodes_a_enviar,int cantidad_datanodes,int cantidad_bloques){
	t_list *lista=list_create();
	bool verificar_disponibilidad(t_datanode_a_enviar*elemento){
		if(elemento->cantidad>=ceilDivision(cantidad_bloques,cantidad_datanodes)){
			return true;
		}else{
			return false;
		}
	}
	lista=list_filter(datanodes_a_enviar,(void*) verificar_disponibilidad);
	return lista;
}
void actualizar_archivos_actuales(char*nombre_nodo,int index_directorio_padre,int tipo_archivo,char*nombre_archivo){

	if(!dictionary_has_key(archivos_actuales,nombre_nodo)){
	  //primera vez que se manda un archivo a este nodo
	  t_list*lista_archivos=list_create();
	  t_archivo_actual *un_archivo=malloc(sizeof(t_archivo_actual));
	  un_archivo->index_padre=index_directorio_padre;
	  un_archivo->total_bloques=1;
	  un_archivo->tipo=tipo_archivo;
	  un_archivo->nombre_archivo=malloc(100);
	  strcpy(un_archivo->nombre_archivo,nombre_archivo);
	  un_archivo->nombre_archivo=realloc(un_archivo->nombre_archivo,strlen(un_archivo->nombre_archivo)+1);
	  pthread_mutex_lock(&mutex_archivos_actuales);
	  list_add(lista_archivos,un_archivo);
	  dictionary_put(archivos_actuales,nombre_nodo,lista_archivos);
	  pthread_mutex_unlock(&mutex_archivos_actuales);
	}else{
	  //ya existe la lista, y tiene algun archivo
		pthread_mutex_lock(&mutex_archivos_actuales);
	  t_list *archivos=dictionary_get(archivos_actuales,nombre_nodo);
	  if(verificar_existencia_lista_archivos(archivos,nombre_archivo)){
		//existe el archivo, hay que aumentar el numero de bloque
		t_archivo_actual*un_archivo=obtener_elemento(archivos,nombre_archivo);
		un_archivo->total_bloques+=1;
		pthread_mutex_unlock(&mutex_archivos_actuales);
	  }else{
		//no existe el archivo, lo creamos
		t_archivo_actual *un_archivo=malloc(sizeof(t_archivo_actual));
		un_archivo->index_padre=index_directorio_padre;
		un_archivo->total_bloques=1;
		un_archivo->nombre_archivo=malloc(100);
		strcpy(un_archivo->nombre_archivo,nombre_archivo);
		un_archivo->nombre_archivo=realloc(un_archivo->nombre_archivo,strlen(un_archivo->nombre_archivo)+1);
		list_add(archivos,un_archivo);
		pthread_mutex_unlock(&mutex_archivos_actuales);
	  }
	}
}
t_resultado_envio* enviar_bloque(t_list *disponibles,t_list*bloques_a_enviar,int i_bloques_a_enviar,int index_copia,char *nombre_archivo, int index_directorio_padre,int tipo_archivo,int numero_copia){
	char *nombre=malloc(100);
	strcpy(nombre,((t_datanode_a_enviar*)list_get(disponibles,index_copia))->nombre);
	nombre=realloc(nombre,strlen(nombre)+1);


	//Se obtiene el numero de bloque del datanode
	char *ruta=malloc(100);
	strcpy(ruta,PUNTO_MONTAJE);
	strcat(ruta,"/bitmaps/");
	strcat(ruta, nombre);
	strcat(ruta,".dat");
	ruta=realloc(ruta,strlen(ruta)+1);
	t_config *nodos=config_create(RUTA_NODOS);
	int total;
	char *nombre_total=malloc(100);
	strcpy(nombre_total,nombre);
	strcat(nombre_total,"Total");
	nombre_total=realloc(nombre_total,strlen(nombre_total)+1);
	total=config_get_int_value(nodos,nombre_total);
	int bitmap = open(ruta,O_RDWR);
	struct stat mystat;
	void *bmap;
	if (fstat(bitmap, &mystat) < 0) {
	  printf("Error al establecer fstat\n");
	  close(bitmap);
	}else{
	  bmap = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, bitmap, 0);
	  if (bmap == MAP_FAILED){
			printf("Error al mapear a memoria: %s\n", strerror(errno));
	  }else{
		int size_bitarray=total%8>0 ? ((int)(total/8))+1 : (int)(total/8);
		t_bitarray *bitarray = bitarray_create_with_mode(bmap, size_bitarray, MSB_FIRST);
		int j;
		for (j=0; j < total; j++) {
		  if(bitarray_test_bit(bitarray,j)==0){
			bitarray_set_bit(bitarray,j);
			break;
		  }
		}
		int numero_bloque=j;
		munmap(bmap,mystat.st_size);
		close(bitmap);


		//creamos tipo de dato a enviar y lo inicializamos
		bloque *primer_bloque=malloc(sizeof(bloque));
		primer_bloque=list_get(bloques_a_enviar,i_bloques_a_enviar);
		primer_bloque->numero=numero_bloque;
		primer_bloque->copia=numero_copia;
		primer_bloque->nombre_archivo=malloc(100);
		strcpy(primer_bloque->nombre_archivo,nombre_archivo);
		primer_bloque->nombre_archivo=realloc(primer_bloque->nombre_archivo,strlen(primer_bloque->nombre_archivo)+1);


		//enviamos al datanode el bloque y el tamaño de bloque
		int tamanio = sizeof(uint32_t) * 4  + primer_bloque->tamanio + sizeof(char)*strlen(primer_bloque->nombre_archivo)+1;
		void *datos = malloc(tamanio);
		*((uint32_t*)datos) = primer_bloque->numero;
		datos += sizeof(uint32_t);
		*((uint32_t*)datos) = primer_bloque->copia;
		datos += sizeof(uint32_t);
		*((uint32_t*)datos) = primer_bloque->tamanio;
		datos += sizeof(uint32_t);
		*((uint32_t*)datos) = i_bloques_a_enviar;
		datos += sizeof(uint32_t);
		memmove(datos,primer_bloque->datos,primer_bloque->tamanio);
		datos += primer_bloque->tamanio;
		strcpy(datos, primer_bloque->nombre_archivo);
		datos +=  strlen(primer_bloque->nombre_archivo) + 1;
		datos -= tamanio;
		//actualizo archivos actuales
		actualizar_archivos_actuales(nombre,index_directorio_padre,tipo_archivo,nombre_archivo);
		//enviamos datos
		int socket=((t_datanode_a_enviar*)list_get(disponibles,index_copia))->socket;
		t_resultado_envio*resultado=malloc(sizeof(t_resultado_envio));
		resultado->bloque=numero_bloque;
		resultado->nombre_nodo=malloc(100);
		strcpy(resultado->nombre_nodo,nombre);
		resultado->nombre_nodo=realloc(resultado->nombre_nodo,strlen(resultado->nombre_nodo)+1);
		resultado->resultado=EnviarDatosTipo(socket, FILESYSTEM, datos, tamanio, SETBLOQUE);
		return resultado;
	  }
	}
}
void limpiar_bitarrays(t_list*lista){
	int var;
	for (var = 0; var < list_size(lista); ++var) {
		char *ruta=malloc(100);
		strcpy(ruta,PUNTO_MONTAJE);
		strcat(ruta,"/bitmaps/");
		strcat(ruta, ((t_bloque_enviado*)list_get(lista,var))->nombre_nodo);
		strcat(ruta,".dat");
		ruta=realloc(ruta,strlen(ruta)+1);
		t_config *nodos=config_create(RUTA_NODOS);
		int total;
		char *nombre_total=malloc(100);
		strcpy(nombre_total,((t_bloque_enviado*)list_get(lista,var))->nombre_nodo);
		strcat(nombre_total,"Total");
		nombre_total=realloc(nombre_total,strlen(nombre_total)+1);
		total=config_get_int_value(nodos,nombre_total);
		int bitmap = open(ruta,O_RDWR);
		struct stat mystat;
		void *bmap;
		if (fstat(bitmap, &mystat) < 0) {
		  printf("Error al establecer fstat\n");
		  close(bitmap);
		}else{
		  bmap = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, bitmap, 0);
		  if (bmap == MAP_FAILED){
				printf("Error al mapear a memoria: %s\n", strerror(errno));
		  }else{
			int size_bitarray=total%8>0 ? ((int)(total/8))+1 : (int)(total/8);
			t_bitarray *bitarray = bitarray_create_with_mode(bmap, size_bitarray, MSB_FIRST);
			bitarray_clean_bit(bitarray,((t_bloque_enviado*)list_get(lista,var))->bloque);
			munmap(bmap,mystat.st_size);
			close(bitmap);
		  }
		}
	}
}
int enviarBloques(t_list *bloques_a_enviar,char *nombre_archivo,int index_directorio_padre,int tipo_archivo){
	//filtramos los que tengan bloques libres
	//hay que filtrar en la lista de datanodes los que tengan bloques libres>0
	//una vez obtenidos,los enviamos
	//TODO ----> ENVIAR BLOQUES

	t_list *datanodes_a_enviar=list_create();
	t_list *disponibles=list_create();
	t_list *bloques_enviados=list_create();
	pthread_mutex_lock(&mutex_datanodes);
	datanodes_a_enviar=list_map(datanodes,(void*) datanodes_disponibles);
	pthread_mutex_unlock(&mutex_datanodes);
	int cantidad_datanodes_a_enviar;
	bool error=false;
	if(!(cantidad_datanodes_a_enviar=se_puede_enviar(datanodes_a_enviar,list_size(bloques_a_enviar)))){
		printf("No hay lugar disponbile\n");
		fflush(stdout);
		error=true;

	}else{
		//datanodes que tengo que enviar
		disponibles=obtener_disponibles(datanodes_a_enviar,cantidad_datanodes_a_enviar,list_size(bloques_a_enviar)*2);
	}
	int size_bloques=list_size(bloques_a_enviar);
	int i_lista_disponibles=0;
	int i_bloques_a_enviar=0;
	if(!error){
		while(size_bloques){
			//obtenemos datos del  datanode para enviarle la primer copia del bloque
			//primer copia es i_lista_disponibles
			int index_primer_copia=i_lista_disponibles;
			int index_segunda_copia= i_lista_disponibles+1>(list_size(disponibles)-1) ? 0 : i_lista_disponibles+1;
			t_resultado_envio*resultado_primer_copia;
			t_resultado_envio*resultado_segunda_copia;
			resultado_primer_copia=enviar_bloque(disponibles,bloques_a_enviar,i_bloques_a_enviar,index_primer_copia,nombre_archivo,index_directorio_padre,tipo_archivo,0);
			resultado_segunda_copia=enviar_bloque(disponibles,bloques_a_enviar,i_bloques_a_enviar,index_segunda_copia,nombre_archivo,index_directorio_padre,tipo_archivo,1);
			if(!(resultado_primer_copia->resultado) || !(resultado_segunda_copia->resultado)){
				//limpio todos los bloques enviados
				limpiar_bitarrays(bloques_enviados);
				//TODO HACER SEMAFOROS
				pthread_mutex_lock(&mutex_archivos_erroneos);
				dictionary_put(archivos_erroneos,nombre_archivo,nombre_archivo);
				pthread_mutex_unlock(&mutex_archivos_erroneos);
				break;
			}else{
				//agrego resultado_primer_copia y resultado_segunda_copia
				t_bloque_enviado*primer_copia=malloc(sizeof(t_bloque_enviado));
				primer_copia->bloque=resultado_primer_copia->bloque;
				primer_copia->nombre_nodo=malloc(100);
				strcpy(primer_copia->nombre_nodo,resultado_primer_copia->nombre_nodo);
				primer_copia->nombre_nodo=realloc(primer_copia->nombre_nodo,strlen(primer_copia->nombre_nodo)+1);
				list_add(bloques_enviados,primer_copia);
				t_bloque_enviado*segunda_copia=malloc(sizeof(t_bloque_enviado));
				segunda_copia->bloque=resultado_segunda_copia->bloque;
				segunda_copia->nombre_nodo=malloc(100);
				strcpy(segunda_copia->nombre_nodo,resultado_segunda_copia->nombre_nodo);
				segunda_copia->nombre_nodo=realloc(segunda_copia->nombre_nodo,strlen(segunda_copia->nombre_nodo)+1);
				list_add(bloques_enviados,segunda_copia);
			}
			i_lista_disponibles=i_lista_disponibles+1>(list_size(disponibles)-1) ? 0 : i_lista_disponibles+1;
			size_bloques--;
			i_bloques_a_enviar++;
			if(size_bloques<=0){
				dictionary_put(archivos_terminados,nombre_archivo,nombre_archivo);
			}
		}
	}
	list_destroy(datanodes_a_enviar);
	list_destroy(disponibles);
	list_destroy(bloques_enviados);
	list_destroy_and_destroy_elements(bloques_a_enviar,free);
	if(error){
		return -1;
	}else{
		return 1;
	}
}

void crear_directorio(){
	t_directory **directorios=malloc(sizeof(t_directory*));
	(*directorios)=malloc(100*sizeof(t_directory));
	t_directory *aux=(*directorios);
	int i;
	for (i = 1; i < 100; ++i) {
		strcpy(aux[i].nombre,"/0");
	}
	truncate(RUTA_DIRECTORIOS,100*sizeof(t_directory));
	guardar_directorios(directorios);
	pthread_mutex_lock(&mutex_directorio);
	index_directorio=0;
	pthread_mutex_unlock(&mutex_directorio);
	free((*directorios));
	free(directorios);
}


void setear_directorio(int index_array,int index,char *nombre,int padre){

	t_directory **directorios=obtener_directorios();
	t_directory *aux=(*directorios);
	aux[index_array].index=index;
	strcpy(aux[index_array].nombre,nombre);
	aux[index_array].padre=padre;
	directorios_ocupados[index_array]=true;
	guardar_directorios(directorios);
	free((*directorios));
	free(directorios);
}


int main(){
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();
	RUTA_DIRECTORIOS=malloc(strlen(PUNTO_MONTAJE)+strlen("/directorios.dat")+1);
	strcpy(RUTA_DIRECTORIOS,PUNTO_MONTAJE);
	strcat(RUTA_DIRECTORIOS,"/directorios.dat");
	RUTA_NODOS=malloc(strlen(PUNTO_MONTAJE)+strlen("/nodos.bin")+1);
	strcpy(RUTA_NODOS,PUNTO_MONTAJE);
	strcat(RUTA_NODOS,"/nodos.bin");
	RUTA_ARCHIVOS=malloc(strlen(PUNTO_MONTAJE)+strlen("/archivos")+1);
	strcpy(RUTA_ARCHIVOS,PUNTO_MONTAJE);
	strcat(RUTA_ARCHIVOS,"/archivos");
	//TODO ACLARACION -----> CODIGO PARA PROBAR LOS DIRECTORIOS PERSISTIDOS
	/*
	t_directory **directorios=obtener_directorios();
	t_directory *aux=(*directorios);
	int var;
	for (var = 0; var < 10; ++var) {
		printf("%i,%i,%s\n",aux[var].index,aux[var].padre,aux[var].nombre);
	}
	return 0;
	 */
	datanodes=list_create();
	archivos_actuales=dictionary_create();
	archivos_terminados=dictionary_create();
	archivos_erroneos=dictionary_create();
	pthread_t hiloConsola;
	pthread_mutex_init(&mutex_datanodes,NULL);
	pthread_mutex_init(&mutex_directorio,NULL);
	pthread_mutex_init(&mutex_directorios_ocupados,NULL);
	pthread_mutex_init(&mutex_archivos_actuales,NULL);
	pthread_mutex_init(&mutex_archivos_erroneos,NULL);
	pthread_mutex_init(&mutex_escribir_archivo,NULL);
	pthread_create(&hiloConsola, NULL, (void*)consola,NULL);
	ServidorConcurrente(IP, PUERTO, FILESYSTEM, &listaHilos, &end, accion);
	pthread_join(hiloConsola, NULL);
	return 0;
}
