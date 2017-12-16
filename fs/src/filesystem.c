#include "sockets.h"
#include <sys/mman.h>
#define TAMBLOQUE (1024*1024)
char *IP;
char *PUNTO_MONTAJE;
char *RUTA_DIRECTORIOS;
char *RUTA_NODOS;
char *RUTA_ARCHIVOS;
char *ruta_a_guardar;
char *RUTA_BITMAPS;
int PUERTO;
int socketFS;
int socketYAMA;
t_cpblock *cpblock;
t_list* listaHilos;
t_list* datanodes;
t_list* datanodes_anteriores;
char *nodo_desconectado;
bool directorios_ocupados[100];
t_dictionary *archivos_actuales;
t_dictionary *archivos_erroneos;
t_dictionary *archivos_terminados;
//t_dictionary *datanodes_dictionary;
int index_directorio = 1;
int anterior_md5;
int anterior_cpto=0;
bool end;
bool md5=false;
bool formateado=false;
bool unico_bloque=false;
bool estable=false;
bool rechazado=false;
bool sin_datanodes=true;
bool yama_online=false;
//char **archivos_almacenados;
pthread_mutex_t mutex_datanodes;
pthread_mutex_t mutex_directorio;
pthread_mutex_t mutex_directorios_ocupados;
pthread_mutex_t mutex_archivos_actuales;
pthread_mutex_t mutex_archivos_erroneos;
pthread_mutex_t mutex_escribir_archivo;
pthread_mutex_t mutex_md5;
pthread_mutex_t mutex_cpfrom;
pthread_mutex_t mutex_respuesta_solicitud;
pthread_mutex_t mutex_orden_md5;
pthread_mutex_t mutex_respuesta_solicitud_cpto;
pthread_mutex_t mutex_cpto;
pthread_mutex_t orden_cpto;
pthread_mutex_t orden_cpblock;

void config_remove_key(t_config *config, char *key) {
	dictionary_remove(config->properties, key);
}
t_directory **obtener_directorios() {
	int fd_directorio = open(RUTA_DIRECTORIOS, O_RDWR);
	if (fd_directorio == -1) {
		printf("Error al intentar abrir el archivo");
	} else {
		struct stat mystat;
		void *directorios;
		if (fstat(fd_directorio, &mystat) < 0) {
			printf("Error al establecer fstat\n");
			close(fd_directorio);
		} else {
			directorios = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ,
					MAP_SHARED, fd_directorio, 0);
			if (directorios == MAP_FAILED) {
				printf("Error al mapear a memoria: %s\n", strerror(errno));
			} else {
				t_directory **directorios_yamafs = malloc(sizeof(t_directory*));
				(*directorios_yamafs) = malloc(100 * sizeof(t_directory));
				memmove((*directorios_yamafs), directorios,100 * sizeof(t_directory));
				munmap(directorios, mystat.st_size);
				close(fd_directorio);
				return directorios_yamafs;
			}
		}
	}
}
int index_ultimo_directorio(char*ruta, char* tipo) {
	t_directory **directorios = obtener_directorios();
	t_directory *aux = (*directorios);
	char**separado_por_barras = string_split(ruta, "/");
	int i = 0;
	int padre_anterior = 0;
	int index_actual;
	if (strcmp("a", tipo) == 0) {
		int cantidad = 0;
		while (separado_por_barras[cantidad]) {
			cantidad++;
		}
		cantidad--;
		int i;
		for (i = 0; i < cantidad; ++i) {
			padre_anterior = obtener_index_directorio(separado_por_barras[i],
					padre_anterior, aux);
		}
		free((*directorios));
		free(directorios);
		return padre_anterior;
	} else {
		while (separado_por_barras[i]) {
			if (!(separado_por_barras[i + 1])) {
				index_actual = obtener_index_directorio(separado_por_barras[i],
						padre_anterior, aux);
			} else {
				padre_anterior = obtener_index_directorio(
						separado_por_barras[i], padre_anterior, aux);
			}
			i++;
		}
		free((*directorios));
		free(directorios);
		return index_actual;
	}
}
void eliminar_ea_nodos(char*nombre) {
	t_config *nodos = config_create(RUTA_NODOS);
	char *nodos_actuales = malloc(100);
	nodos_actuales = config_get_string_value(nodos, "NODOS");
	nodos_actuales = realloc(nodos_actuales, strlen(nodos_actuales) + 1);
	char**separado_por_comas = string_split(nodos_actuales, ",");
	int cantidad_comas = 0;
	int i = 0;
	while (separado_por_comas[cantidad_comas]) {
		cantidad_comas++;
	}
	char *nuevos_nodos = malloc(100);
	if (cantidad_comas == 1) {
		//tiene un solo elemento
		config_remove_key(nodos,"NODOS");
	} else {
		//tiene mas de un elemento
		char * substring;
		while (i < cantidad_comas) {
			if (i == 0) {
				substring=string_substring(separado_por_comas[i], 1,strlen(separado_por_comas[i]) - 1);
				substring = realloc(substring, strlen(substring) + 1);
				if (strcmp(nombre, substring) == 0) {
					strcpy(nuevos_nodos, "[");
				} else {
					strcpy(nuevos_nodos, separado_por_comas[i]);
				}
			} else {
				if (i == (cantidad_comas - 1)) {
					substring=string_substring(separado_por_comas[i], 0,
							strlen(separado_por_comas[i]) - 1);
					if (strcmp(nombre, substring) == 0) {
						strcat(nuevos_nodos, "]");
					} else {
						strcat(nuevos_nodos, separado_por_comas[i]);
					}

				} else {
					substring=separado_por_comas[i];
					if (strcmp(nombre, substring) == 0) {
						strcat(nuevos_nodos, ",");
					} else {
						strcat(nuevos_nodos, ",");
						strcat(nuevos_nodos, separado_por_comas[i]);
					}
				}
			}
			i++;
		}
		free(substring);
	}
	config_set_value(nodos, "NODOS", nuevos_nodos);
	char *libre = malloc(100);
	strcpy(libre, nombre);
	strcat(libre, "Libre");
	char *string_libre_total_a_actualizar = malloc(100);
	if (config_has_property(nodos, libre)) {
		int libre_nodo_a_eliminar = config_get_int_value(nodos, libre);
		int libre_total = config_get_int_value(nodos, "LIBRE");
		int libre_total_a_actualizar =
				libre_total > libre_nodo_a_eliminar ?
						libre_total - libre_nodo_a_eliminar : 0;
		sprintf(string_libre_total_a_actualizar, "%i",
				libre_total_a_actualizar);
		string_libre_total_a_actualizar = realloc(
				string_libre_total_a_actualizar,
				strlen(string_libre_total_a_actualizar) + 1);
		config_set_value(nodos, "LIBRE", string_libre_total_a_actualizar);
		config_save_in_file(nodos, "/home/utnso/metadata/nodos.bin");
	}
	char *total = malloc(100);
	strcpy(total, nombre);
	strcat(total, "Total");
	char *string_total_total_a_actualizar = malloc(100);
	if (config_has_property(nodos, total)) {
		int total_nodo_a_eliminar = config_get_int_value(nodos, total);
		int total_total = config_get_int_value(nodos, "TAMANIO");
		int total_total_a_actualizar =total_total > total_nodo_a_eliminar ?total_total - total_nodo_a_eliminar : 0;
		sprintf(string_total_total_a_actualizar, "%i",total_total_a_actualizar);
		string_total_total_a_actualizar = realloc(string_total_total_a_actualizar,strlen(string_total_total_a_actualizar) + 1);
		config_set_value(nodos, "TAMANIO", string_total_total_a_actualizar);
	}
	config_remove_key(nodos,total);
	config_remove_key(nodos,libre);
	config_save_in_file(nodos, "/home/utnso/metadata/nodos.bin");
	config_destroy(nodos);
	//free(nodos_actuales);
	free(nuevos_nodos);
	free(libre);
	free(total);
	free(string_total_total_a_actualizar);
	free(string_libre_total_a_actualizar);
}

t_archivo_actual *obtener_elemento(t_list*lista, char*nombre_archivo) {
	bool nombre_igual(t_archivo_actual*elemento) {
		if (strcmp(elemento->nombre_archivo, nombre_archivo) == 0) {
			return true;
		}
		return false;
	}
	//t_archivo_actual *elemento = malloc(sizeof(t_archivo_actual));
	//elemento = list_find(lista, (void*) nombre_igual);
	return list_find(lista, (void*) nombre_igual);
}
void actualizar_nodos_bin(info_datanode *data) {
	char *ruta_nodos = malloc(100);
	strcpy(ruta_nodos, PUNTO_MONTAJE);
	strcat(ruta_nodos, "/nodos.bin");
	ruta_nodos = realloc(ruta_nodos, strlen(ruta_nodos) + 1);
	t_config *nodos = config_create(ruta_nodos);
	int tamanio_actual;
	tamanio_actual = config_get_int_value(nodos, "TAMANIO");
	tamanio_actual =tamanio_actual == 0 ?data->bloques_totales :tamanio_actual + data->bloques_totales;
	char*string_actual;
	string_actual=integer_to_string(string_actual,tamanio_actual);
	config_set_value(nodos, "TAMANIO", string_actual);
	config_save_in_file(nodos, ruta_nodos);
	int libre;
	libre = config_get_int_value(nodos, "LIBRE");
	libre = libre == 0 ? data->bloques_libres : libre + data->bloques_libres;
	char *string_libre;
	string_libre=integer_to_string(string_libre,libre);
	config_set_value(nodos, "LIBRE",string_libre);
	config_save_in_file(nodos, ruta_nodos);
	char *nodos_actuales = calloc(1, 100);
	char *nuevos_nodos = calloc(1, 300);
	char* nodo_nuevo = calloc(1, 120);
	if (config_has_property(nodos,"NODOS")){
		strcpy(nodos_actuales,config_get_string_value(nodos, "NODOS"));
		char**separado_por_comas = string_split(nodos_actuales, ",");
		int cantidad_comas = 0;
		while (separado_por_comas[cantidad_comas]) {
			cantidad_comas++;
		}
		int iterate = 0;
		while (iterate < cantidad_comas) {
			if (cantidad_comas == 1) {
				char *substring;
				substring=string_substring(separado_por_comas[(cantidad_comas - 1)], 0,strlen(separado_por_comas[(cantidad_comas - 1)]) - 1);
				substring = realloc(substring, strlen(substring) + 1);
				strcpy(nuevos_nodos, substring);
				strcat(nuevos_nodos, ",");
				strcat(nuevos_nodos, data->nodo);
				strcat(nuevos_nodos, "]");
				nuevos_nodos = realloc(nuevos_nodos, strlen(nuevos_nodos) + 1);
				free(substring);
			} else {
				if (iterate == 0) {
					strcpy(nuevos_nodos, separado_por_comas[iterate]);
					strcat(nuevos_nodos, ",");
				} else {
					if (iterate == (cantidad_comas - 1)) {
						char *substring;
						substring=string_substring(separado_por_comas[(cantidad_comas - 1)], 0,strlen(separado_por_comas[(cantidad_comas - 1)])- 1);
						substring = realloc(substring, strlen(substring) + 1);
						strcat(nuevos_nodos, substring);
						strcat(nuevos_nodos, ",");
						strcat(nuevos_nodos, data->nodo);
						strcat(nuevos_nodos, "]");
						nuevos_nodos = realloc(nuevos_nodos,strlen(nuevos_nodos) + 1);
						free(substring);
					} else {
						strcat(nuevos_nodos, separado_por_comas[iterate]);
						strcat(nuevos_nodos, ",");
					}
				}

			}
			iterate++;
		}
		nodos_actuales = realloc(nodos_actuales, strlen(nodos_actuales) + 1);
		config_set_value(nodos, "NODOS", nuevos_nodos);
		config_save_in_file(nodos, ruta_nodos);
	} else {
		strcpy(nodo_nuevo, "[");
		strcat(nodo_nuevo, data->nodo);
		strcat(nodo_nuevo, "]");
		nodo_nuevo = realloc(nodo_nuevo, strlen(nodo_nuevo) + 1);
		config_set_value(nodos, "NODOS", nodo_nuevo);
		config_save_in_file(nodos, ruta_nodos);
	}
	config_save_in_file(nodos, ruta_nodos);
	char *nodo_actual_total = calloc(1, 100);
	strcpy(nodo_actual_total, data->nodo);
	strcat(nodo_actual_total, "Total");
	char *string_bloques_totales = calloc(1, 100);
	sprintf(string_bloques_totales, "%i", data->bloques_totales);
	string_bloques_totales = realloc(string_bloques_totales,strlen(string_bloques_totales) + 1);
	config_set_value(nodos, nodo_actual_total, string_bloques_totales);
	config_save_in_file(nodos, ruta_nodos);
	char *nodo_actual_libre = calloc(1, 100);
	strcpy(nodo_actual_libre, data->nodo);
	strcat(nodo_actual_libre, "Libre");
	char *string_bloques_libres = calloc(1, 100);
	sprintf(string_bloques_libres, "%i", data->bloques_libres);
	string_bloques_libres = realloc(string_bloques_libres,strlen(string_bloques_libres) + 1);
	config_set_value(nodos, nodo_actual_libre, string_bloques_libres);
	config_save_in_file(nodos, ruta_nodos);
	config_destroy(nodos);
	free(ruta_nodos);
	free(nodos_actuales);
	free(nuevos_nodos);
	free(nodo_nuevo);
	free(nodo_actual_total);
	free(string_bloques_totales);
	free(nodo_actual_libre);
	free(string_bloques_libres);
	free(string_actual);
	free(string_libre);
}

void guardar_directorios(t_directory **directorios) {
	int fd_directorio = open(RUTA_DIRECTORIOS, O_RDWR);
	if (fd_directorio == -1) {
		printf("Error al intentar abrir el archivo");
	} else {
		struct stat mystat;
		void *archivo_directorios;
		if (fstat(fd_directorio, &mystat) < 0) {
			printf("Error al establecer fstat\n");
			close(fd_directorio);
		} else {
			archivo_directorios = mmap(NULL, mystat.st_size,
					PROT_WRITE | PROT_READ, MAP_SHARED, fd_directorio, 0);
			if (directorios == MAP_FAILED) {
				printf("Error al mapear a memoria: %s\n", strerror(errno));
			} else {
				memmove(archivo_directorios, (*directorios),
						100 * sizeof(t_directory));
				munmap(directorios, mystat.st_size);
				close(fd_directorio);
			}
		}
	}
}
void obtenerValoresArchivoConfiguracion() {
	t_config* arch =
			config_create(
					"/home/utnso/workspace/tp-2017-2c-Yama-Que-Yama/fs/filesystemCFG.txt");
	IP = string_duplicate(config_get_string_value(arch, "IP"));
	PUERTO = config_get_int_value(arch, "PUERTO");
	PUNTO_MONTAJE = string_duplicate(
			config_get_string_value(arch, "PUNTO_MONTAJE"));
	config_destroy(arch);
}

void imprimirArchivoConfiguracion() {
	printf("Configuraci��n:\n"
			"IP=%s\n"
			"PUERTO=%d\n"
			"PUNTO_MONTAJE=%s\n", IP, PUERTO, PUNTO_MONTAJE);
}
int verificar_estado(){
	char* obtener_nombre(info_datanode*elemento){
		return elemento->nodo;
	}
	int rv;
	if(list_size(datanodes_anteriores)>0){
		t_list*lista=list_map(datanodes,(void*) obtener_nombre);
		DIR*primer_nivel,*segundo_nivel;
		struct dirent *ep,*ap;
		primer_nivel=opendir(RUTA_ARCHIVOS);
		while ((ep = readdir (primer_nivel))!=NULL){
			if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, "..")){
				continue;
			}
			char*ruta=malloc(100);
			strcpy(ruta,RUTA_ARCHIVOS);
			strcat(ruta,"/");
			strcat(ruta,ep->d_name);
			segundo_nivel=opendir(ruta);
			while((ap = readdir (segundo_nivel))!=NULL){
				if (!strcmp(ap->d_name, ".") || !strcmp(ap->d_name, "..")){
					continue;
				}
				strcat(ruta,"/");
				strcat(ruta,ap->d_name);
				ruta=realloc(ruta,strlen(ruta)+1);
				t_config*archivo=config_create(ruta);
				int i=0;
				while(1){
					char**primera_copia=malloc(sizeof(char*));
					primera_copia[0]=malloc(20);
					primera_copia[1]=malloc(4);
					char**segunda_copia=malloc(sizeof(char*));
					segunda_copia[0]=malloc(20);
					segunda_copia[1]=malloc(4);
					char**tercera_copia=malloc(sizeof(char*));
					tercera_copia[0]=malloc(20);
					tercera_copia[1]=malloc(4);
					char*key_primera_copia=malloc(30);
					strcpy(key_primera_copia,"BLOQUE");
					char*string;
					string=integer_to_string(string,i);
					strcat(key_primera_copia,string);
					strcat(key_primera_copia,"COPIA0");
					key_primera_copia=realloc(key_primera_copia,strlen(key_primera_copia)+1);
					char*key_segunda_copia=malloc(30);
					strcpy(key_segunda_copia,"BLOQUE");
					strcat(key_segunda_copia,string);
					strcat(key_segunda_copia,"COPIA1");
					key_segunda_copia=realloc(key_segunda_copia,strlen(key_segunda_copia)+1);
					char*key_tercera_copia=malloc(30);
					strcpy(key_tercera_copia,"BLOQUE");
					strcat(key_tercera_copia,string);
					strcat(key_tercera_copia,"COPIA2");
					key_tercera_copia=realloc(key_tercera_copia,strlen(key_tercera_copia)+1);
					char*bytes=malloc(100);
					strcpy(bytes,"BLOQUE");
					strcat(bytes,string);
					strcat(bytes,"BYTES");
					bytes=realloc(bytes,strlen(bytes)+1);
					bool flag;
					if(!config_has_property(archivo,bytes)){
						free(bytes);
						free(primera_copia);
						free(segunda_copia);
						free(key_primera_copia);
						free(key_segunda_copia);
						free(key_tercera_copia);
						free(string);
						break;
					}
					if(config_has_property(archivo,key_primera_copia) && config_has_property(archivo,key_segunda_copia)){
						primera_copia=config_get_array_value(archivo,key_primera_copia);
						segunda_copia=config_get_array_value(archivo,key_segunda_copia);
						if(config_has_property(archivo,key_tercera_copia)){
							tercera_copia=config_get_array_value(archivo,key_tercera_copia);
							flag=true;
						}
						int var;
						int contador=0;
						for ( var = 0; var < list_size(lista); ++var) {
							char*nombre=list_get(lista,var);
							if(!strcmp(nombre,primera_copia[0])|| !strcmp(nombre,segunda_copia[0])){
									contador++;
							}
							if(flag){
								if(!strcmp(nombre,tercera_copia[0])){
									contador++;
								}
							}
						}
						if(contador==0){
							rv=-1;
							break;
						}
					}else{
						int var;
						int contador=0;
						if(config_has_property(archivo,key_primera_copia)){
							primera_copia=config_get_array_value(archivo,key_primera_copia);
							for ( var = 0; var < list_size(lista); ++var) {
								char*nombre=list_get(lista,var);
								if(!strcmp(nombre,primera_copia[0])){
									contador++;
								}
							}
						}
						if(config_has_property(archivo,key_segunda_copia)){
							segunda_copia=config_get_array_value(archivo,key_segunda_copia);
							for ( var = 0; var < list_size(lista); ++var) {
								char*nombre=list_get(lista,var);
								if(!strcmp(nombre,segunda_copia[0])){
									contador++;
								}
							}
						}
						if(config_has_property(archivo,key_tercera_copia)){
							tercera_copia=config_get_array_value(archivo,key_tercera_copia);
							for ( var = 0; var < list_size(lista); ++var) {
								char*nombre=list_get(lista,var);
								if(!strcmp(nombre,tercera_copia[0])){
									contador++;
								}
							}
						}
						if(contador==0){
							rv=-1;
							break;
						}

					}
					i++;
					free(primera_copia[0]);
					free(primera_copia[1]);
					free(primera_copia);
					free(segunda_copia[0]);
					free(segunda_copia[1]);
					free(segunda_copia);
					free(tercera_copia[0]);
					free(tercera_copia[1]);
					free(tercera_copia);
					free(key_primera_copia);
					free(key_segunda_copia);
					free(key_tercera_copia);
					free(bytes);
					free(string);
					if(rv==-1){
						rv=-1;
						break;
					}
				}
				if(rv==-1){
					break;
				}
			}
			if(rv==-1){
				break;
			}
			free(ruta);
		}
		closedir (primer_nivel);
		if(rv==-1){
			return -1;
		}else{
			return 1;
		}
	}
}
void sacar_datanode(int socket) {
	int tiene_socket(info_datanode *datanode) {
		if (datanode->socket == socket) {
			nodo_desconectado=malloc(strlen(datanode->nodo)+1);
			strcpy(nodo_desconectado,datanode->nodo);
			eliminar_ea_nodos(datanode->nodo);
		}
		return datanode->socket != socket;
	}
	pthread_mutex_lock(&mutex_datanodes);
	datanodes = list_filter(datanodes, (void*) tiene_socket);
	pthread_mutex_unlock(&mutex_datanodes);
}

void crear_y_actualizar_archivo(t_archivo_actual*elemento,int numero_bloque_enviado, int numero_bloque_datanode,
		int tamanio_bloque, int numero_copia, char*nombre_nodo,char*nombre_archivo) {
	struct stat mystat;
	char *index_directorio_padre = malloc(100);
	char *string_padre;
	string_padre=integer_to_string(string_padre,elemento->index_padre);
	strcpy(index_directorio_padre,string_padre);
	index_directorio_padre = realloc(index_directorio_padre,strlen(index_directorio_padre) + 1);
	char *ruta_directorio = malloc(100);
	strcpy(ruta_directorio, RUTA_ARCHIVOS);
	strcat(ruta_directorio, "/");
	strcat(ruta_directorio, index_directorio_padre);
	ruta_directorio = realloc(ruta_directorio, strlen(ruta_directorio) + 1);
	if (stat(ruta_directorio, &mystat) == -1) {
		mkdir(ruta_directorio, 0700);
	}
	free(ruta_directorio);
	ruta_directorio = calloc(1, 100);
	strcpy(ruta_directorio, RUTA_ARCHIVOS);
	strcat(ruta_directorio, "/");
	strcat(ruta_directorio, index_directorio_padre);
	strcat(ruta_directorio, "/");
	strcat(ruta_directorio, nombre_archivo);
	ruta_directorio = realloc(ruta_directorio, strlen(ruta_directorio) + 1);
	bool primera_vez = false;
	if (access(ruta_directorio, F_OK) == -1) {
		//no existe el archivo, se lo crea
		FILE *f = fopen(ruta_directorio, "w");
		fclose(f);
		primera_vez = true;
	}
	t_config *archivo = config_create(ruta_directorio);
	char *key = malloc(100);
	char*key_integer;
	key_integer=integer_to_string(key_integer,numero_bloque_enviado);
	char*key_copia;
	key_copia=integer_to_string(key_copia,numero_copia);
	strcpy(key, "BLOQUE");
	strcat(key, key_integer);
	strcat(key, "COPIA");
	strcat(key, key_copia);
	key = realloc(key, strlen(key) + 1);
	char *value = malloc(100);
	char *value_string;
	value_string=integer_to_string(value_string,numero_bloque_datanode);
	strcpy(value, "[");
	strcat(value, nombre_nodo);
	strcat(value, ",");
	strcat(value, value_string);
	strcat(value, "]");
	value = realloc(value, strlen(value) + 1);
	config_set_value(archivo, key, value);
	config_save(archivo);
	char *tamanio = malloc(100);
	char *tamanio_string;
	tamanio_string=integer_to_string(tamanio_string,numero_bloque_enviado);
	strcpy(tamanio, "BLOQUE");
	strcat(tamanio, tamanio_string);
	strcat(tamanio, "BYTES");
	int tamanio_anterior = 0;
	char*string_tamanio_bloque;
	if (!config_has_property(archivo, tamanio)) {
		tamanio_anterior = tamanio_bloque;
		string_tamanio_bloque=integer_to_string(string_tamanio_bloque,tamanio_bloque);
		config_set_value(archivo, tamanio, string_tamanio_bloque);
		config_save(archivo);
		config_save_in_file(archivo, ruta_directorio);
		free(string_tamanio_bloque);
	}
	char*string_tamanio_total;
	if (config_has_property(archivo, "TAMANIO")) {
		int tamanio_total = config_get_int_value(archivo, "TAMANIO");
		tamanio_total += tamanio_anterior;
		string_tamanio_total=integer_to_string(string_tamanio_total,tamanio_total);
		config_set_value(archivo, "TAMANIO",string_tamanio_total);
		config_save(archivo);
	} else {
		string_tamanio_total=integer_to_string(string_tamanio_total,tamanio_bloque);
		config_set_value(archivo, "TAMANIO", string_tamanio_total);
		config_save(archivo);
		config_save_in_file(archivo, ruta_directorio);
	}
	char *tipo_archivo = malloc(20);
	if (primera_vez) {
		if (elemento->tipo == 0) {
			strcpy(tipo_archivo, "TEXTO");
		} else {
			strcpy(tipo_archivo, "BINARIO");
		}
		tipo_archivo = realloc(tipo_archivo, strlen(tipo_archivo) + 1);
		config_set_value(archivo, "TIPO", tipo_archivo);
		config_save(archivo);
	}
	config_save_in_file(archivo, ruta_directorio);
	config_destroy(archivo);
	free(index_directorio_padre);
	free(ruta_directorio);
	free(key);
	free(value);
	free(tamanio);
	free(tipo_archivo);
	free(string_padre);
	free(key_integer);
	free(key_copia);
	free(value_string);
	free(tamanio_string);
	free(string_tamanio_total);
}
void eliminar_archivo(char*nombre_nodo, char*nombre_archivo) {
	t_list*lista = dictionary_get(archivos_actuales, nombre_nodo);
	bool tiene_nombre(t_archivo_actual* archivo) {
		if (strcmp(archivo->nombre_archivo, nombre_archivo) == 0) {
			return true;
		}
		return false;
	}
	void hacer_free(t_archivo_actual*elemento){
		free(elemento->nombre_archivo);
		free(elemento);
	}
	list_remove_and_destroy_by_condition(lista, (void*) tiene_nombre,(void*)hacer_free);
}
void escribir_archivo_temporal(int tamanio_archivo, int numero_bloque,
		int index_directorio, void*bloque, int tamanio_bloque,
		char*nombre_archivo) {
	char *path = malloc(100);
	if(md5){
		strcpy(path, PUNTO_MONTAJE);
		strcat(path, "/");
		strcat(path, "temporal_md5");
	}else{
		strcpy(path,ruta_a_guardar);
		strcat(path,"/");
		strcat(path,nombre_archivo);
	}
	path=realloc(path,strlen(path)+1);
	if (access(path, F_OK) == -1) {
		anterior_md5 = 0;
		FILE*file = fopen(path, "w");
		fclose(file);
		truncate(path, tamanio_archivo);
	}
	int temporal = open(path, O_RDWR);
	struct stat mystat;
	void *bmap;
	if (fstat(temporal, &mystat) < 0) {
		printf("Error al establecer fstat\n");
		close(temporal);
	} else {
		bmap = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED,temporal, 0);
		void* aux = bmap;
		bmap+=anterior_md5;
		if (bmap == MAP_FAILED) {
			printf("Error al mapear a memoria: %s\n", strerror(errno));
			close(temporal);
		} else {
			memmove(bmap, bloque, tamanio_bloque);
			munmap(aux, mystat.st_size);
			close(temporal);
			anterior_md5 += tamanio_bloque;
			free(path);
		}
	}
}
t_list*obtener_lista_bloques(char*ruta_archivo){
	t_config*archivo=config_create(ruta_archivo);
	t_list *bloques=list_create();
	bool existe=true;
	int i=0;
	while(existe){
		char*bytes=malloc(100);
		strcpy(bytes,"BLOQUE");
		char*string_i;
		string_i=integer_to_string(string_i,i);
		strcat(bytes,string_i);
		strcat(bytes,"BYTES");
		bytes=realloc(bytes,strlen(bytes)+1);
		char *primera_copia=malloc(100);
		strcpy(primera_copia,"BLOQUE");
		strcat(primera_copia,string_i);
		strcat(primera_copia,"COPIA0");
		char *segunda_copia=malloc(100);
		strcpy(segunda_copia,"BLOQUE");
		strcat(segunda_copia,string_i);
		strcat(segunda_copia,"COPIA1");
		if(config_has_property(archivo,bytes)){
			t_bloque_yama *un_bloque=malloc(sizeof(t_bloque_yama));
			un_bloque->numero_bloque=i;
			un_bloque->tamanio=config_get_int_value(archivo,bytes);
			char **primera_copia_array=config_get_array_value(archivo,primera_copia);
			un_bloque->primer_bloque_nodo=atoi(primera_copia_array[1]);
			char **segunda_copia_array=config_get_array_value(archivo,segunda_copia);
			un_bloque->segundo_bloque_nodo=atoi(segunda_copia_array[1]);
			un_bloque->primer_nombre_nodo=string_new();
			strcpy(un_bloque->primer_nombre_nodo,primera_copia_array[0]);
			un_bloque->segundo_nombre_nodo=string_new();
			strcpy(un_bloque->segundo_nombre_nodo,segunda_copia_array[0]);
			list_add(bloques,un_bloque);
		}else{
			existe=false;
		}
		i++;
		free(string_i);
	}
	return bloques;
}

void escribir_archivo_cpto(int tamanio_archivo, int bloque_archivo,int index_directorio,void* bloque, int tamanio_bloque,char*nombre_archivo){
	char *path = malloc(100);
	strcpy(path,ruta_a_guardar);
	strcat(path,"/");
	strcat(path,nombre_archivo);
	path=realloc(path,strlen(path)+1);
	if (access(path, F_OK) == -1) {
		anterior_md5 = 0;
		FILE*file = fopen(path, "w");
		fclose(file);
		truncate(path, tamanio_archivo);
	}
	int archivo = open(path, O_RDWR);
	struct stat mystat;
	void *bmap;
	void*aux;
	if (fstat(archivo, &mystat) < 0) {
		printf("Error al establecer fstat\n");
		close(archivo);
	} else {
		bmap = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED,archivo, 0);
		aux=bmap;
		bmap+=anterior_cpto;
		if (bmap == MAP_FAILED) {
			printf("Error al mapear a memoria: %s\n", strerror(errno));
			close(archivo);
		} else {
			memmove(bmap, bloque, tamanio_bloque);
			munmap(aux, mystat.st_size);
			close(archivo);
			anterior_cpto += tamanio_bloque;
			free(path);
		}
	}
}
void crear_copia(char *ruta_archivo,char *nombre_nodo,int bloque_archivo,int bloque_datanode){
	t_config*archivo=config_create(ruta_archivo);
	char*primer_copia=malloc(30);
	char*segunda_copia=malloc(30);
	char*tercera_copia=malloc(30);
	char*string_bloque;
	string_bloque=integer_to_string(string_bloque,bloque_archivo);
	strcpy(primer_copia,"BLOQUE");
	strcat(primer_copia,string_bloque);
	strcat(primer_copia,"COPIA0");
	primer_copia=realloc(primer_copia,strlen(primer_copia)+1);
	strcpy(segunda_copia,"BLOQUE");
	strcat(segunda_copia,string_bloque);
	strcat(segunda_copia,"COPIA1");
	segunda_copia=realloc(segunda_copia,strlen(segunda_copia)+1);
	strcpy(tercera_copia,"BLOQUE");
	strcat(tercera_copia,string_bloque);
	strcat(tercera_copia,"COPIA2");
	tercera_copia=realloc(tercera_copia,strlen(tercera_copia)+1);
	char*value=malloc(50);
	strcpy(value,"[");
	strcat(value,nombre_nodo);
	strcat(value,",");
	char*string_bloque_datanode;
	string_bloque_datanode=integer_to_string(string_bloque_datanode,bloque_datanode);
	strcat(value,string_bloque_datanode);
	strcat(value,"]");
	value=realloc(value,strlen(value)+1);
	if(config_has_property(archivo,primer_copia) && config_has_property(archivo,segunda_copia)){
		config_set_value(archivo,tercera_copia,value);
	}else{
		if(config_has_property(archivo,primer_copia)){
			config_set_value(archivo,segunda_copia,value);
		}else{
			config_set_value(archivo,primer_copia,value);
		}
	}
	config_save(archivo);
	config_save_in_file(archivo,ruta_archivo);
	config_destroy(archivo);
	free(primer_copia);
	free(segunda_copia);
	free(tercera_copia);
	free(value);
	free(string_bloque_datanode);
	free(string_bloque);
}
void accion(void* socket) {
	int socketFD = *(int*) socket;
	Paquete paquete;
	void* datos;
	while (RecibirPaqueteServidorFS(socketFD, FILESYSTEM, &paquete) > 0) {
		if (!strcmp(paquete.header.emisor, DATANODE)) {
			switch (paquete.header.tipoMensaje) {
			case RESPUESTASETBLOQUECPTO:{
				datos=paquete.Payload;
				int bloque_archivo=*((uint32_t*) datos);
				datos += sizeof(uint32_t);
				int bloque_datanode=*((uint32_t*) datos);
				datos += sizeof(uint32_t);
				int index_padre=*((uint32_t*) datos);
				datos += sizeof(uint32_t);
				int resultado=*((uint32_t*) datos);
				datos += sizeof(uint32_t);
				char*nombre_archivo=malloc(50);
				strcpy(nombre_archivo,datos);
				datos+=strlen(nombre_archivo)+1;
				nombre_archivo=realloc(nombre_archivo,strlen(nombre_archivo)+1);
				char*nombre_nodo=malloc(10);
				strcpy(nombre_nodo,datos);
				nombre_nodo=realloc(nombre_nodo,strlen(nombre_nodo)+1);
				datos+=strlen(nombre_nodo)+1;

				if(resultado==1){
					char*ruta_archivo=malloc(100);
					strcpy(ruta_archivo,RUTA_ARCHIVOS);
					strcat(ruta_archivo,"/");
					char*index;
					index=integer_to_string(index,index_padre);
					strcat(ruta_archivo,index);
					strcat(ruta_archivo,"/");
					strcat(ruta_archivo,nombre_archivo);
					ruta_archivo=realloc(ruta_archivo,strlen(ruta_archivo)+1);
					crear_copia(ruta_archivo,nombre_nodo,bloque_archivo,bloque_datanode);
					free(ruta_archivo);
					free(index);
				}else{
					printf("Error en el guardado de datos\n");
				}
				free(nombre_archivo);
				free(nombre_nodo);
			}
			break;
			case RESPUESTAGETBLOQUE:{
				datos=paquete.Payload;
				cpblock=malloc(sizeof(t_cpblock));
				cpblock->tamanio=*((uint32_t*) datos);
				datos += sizeof(uint32_t);
				cpblock->nombre_nodo=malloc(50);
				strcpy(cpblock->nombre_nodo,datos);
				cpblock->nombre_nodo=realloc(cpblock->nombre_nodo,strlen(cpblock->nombre_nodo)+1);
				datos+=strlen(cpblock->nombre_nodo)+1;
				cpblock->datos=calloc(1,cpblock->tamanio);
				memmove(cpblock->datos,datos,cpblock->tamanio);
				datos+=cpblock->tamanio;
				pthread_mutex_unlock(&orden_cpblock);
			}
			break;
			case RESPUESTASOLICITUD: {
				pthread_mutex_lock(&mutex_respuesta_solicitud);
				datos = paquete.Payload;
				int bloque_archivo = *((uint32_t*) datos);
				datos += sizeof(uint32_t);
				int bloques_totales = *((uint32_t*) datos);
				datos += sizeof(uint32_t);
				int index_directorio = *((uint32_t*) datos);
				datos += sizeof(uint32_t);
				int tamanio_bloque = *((uint32_t*) datos);
				datos += sizeof(uint32_t);
				int tamanio_archivo = *((uint32_t*) datos);
				datos += sizeof(uint32_t);
				char*nombre_archivo = malloc(100);
				strcpy(nombre_archivo, datos);
				nombre_archivo = realloc(nombre_archivo,strlen(nombre_archivo) + 1);
				datos += strlen(nombre_archivo) + 1;
				void *bloque = malloc(tamanio_bloque);
				memmove(bloque, datos, tamanio_bloque);
				datos += tamanio_bloque;
				escribir_archivo_temporal(tamanio_archivo, bloque_archivo,index_directorio, bloque, tamanio_bloque,nombre_archivo);
				pthread_mutex_unlock(&mutex_md5);
				if (bloques_totales == 1 && md5){
					anterior_md5=0;
					pthread_mutex_unlock(&mutex_orden_md5);
				}
				free(nombre_archivo);
				free(bloque);
				pthread_mutex_unlock(&mutex_respuesta_solicitud);
			}
				break;
			case RESPUESTASOLICITUDCPTO:{
				pthread_mutex_lock(&mutex_respuesta_solicitud_cpto);
				datos=malloc(paquete.header.tamPayload);
				datos = paquete.Payload;
				int bloque_archivo = *((uint32_t*) datos);
				datos += sizeof(uint32_t);
				int bloques_totales = *((uint32_t*) datos);
				datos += sizeof(uint32_t);
				int index_directorio = *((uint32_t*) datos);
				datos += sizeof(uint32_t);
				int tamanio_bloque = *((uint32_t*) datos);
				datos += sizeof(uint32_t);
				int tamanio_archivo = *((uint32_t*) datos);
				datos += sizeof(uint32_t);
				char*nombre_archivo = malloc(100);
				strcpy(nombre_archivo, datos);
				nombre_archivo = realloc(nombre_archivo,strlen(nombre_archivo) + 1);
				datos += strlen(nombre_archivo) + 1;
				char*ruta_a_guardar=malloc(100);
				strcpy(ruta_a_guardar,datos);
				ruta_a_guardar=realloc(ruta_a_guardar,strlen(ruta_a_guardar)+1);
				datos += strlen(ruta_a_guardar) + 1;
				void *bloque = malloc(tamanio_bloque);
				memmove(bloque, datos, tamanio_bloque);
				datos += tamanio_bloque;
				if(strcmp(ruta_a_guardar,"cat")==0){
					pthread_mutex_unlock(&mutex_cpto);
					fflush(stdout);
					printf("%s",(char*)bloque);
					fflush(stdout);
					if(bloques_totales==1){
						fflush(stdout);
						printf("%s\n",(char*)bloque);
						fflush(stdout);
					}
				}else{
					escribir_archivo_cpto(tamanio_archivo, bloque_archivo,index_directorio, bloque, tamanio_bloque,nombre_archivo);
					pthread_mutex_unlock(&mutex_cpto);
					if(bloques_totales==1){
						pthread_mutex_unlock(&orden_cpto);
					}
				}
				free(nombre_archivo);
				free(ruta_a_guardar);
				free(bloque);
				pthread_mutex_unlock(&mutex_respuesta_solicitud_cpto);
			}
			break;
			case IDENTIFICACIONDATANODE: {
				datos = paquete.Payload;
				info_datanode *data = calloc(1, sizeof(info_datanode));
				uint32_t size_databin = *((uint32_t*) datos);
				datos += sizeof(uint32_t);
				data->puertoworker = *((uint32_t*) datos);
				datos += sizeof(uint32_t);
				data->ip_nodo = malloc(110);
				strcpy(data->ip_nodo, datos);
				data->ip_nodo = realloc(data->ip_nodo, strlen(data->ip_nodo) + 1);
				datos+=strlen(data->ip_nodo)+1;
				data->nodo=malloc(10);
				strcpy(data->nodo,datos);
				data->nodo=realloc(data->nodo,strlen(data->nodo)+1);
				datos+=strlen(data->nodo)+1;
				data->socket = socketFD;
				if(yama_online){
					int tamanio_yama=sizeof(uint32_t)+strlen(data->ip_nodo)+1+strlen(data->nodo)+1;
					void*datos_yama=malloc(tamanio_yama);
					*((uint32_t*) datos_yama)=data->puertoworker;
					datos_yama+=sizeof(uint32_t);
					strcpy(datos_yama,data->ip_nodo);
					datos_yama+=strlen(data->ip_nodo)+1;
					strcpy(datos_yama,data->nodo);
					datos_yama+=strlen(data->nodo)+1;
					datos_yama-=tamanio_yama;
					EnviarDatosTipo(socketYAMA,FILESYSTEM,datos_yama,tamanio_yama,NUEVOWORKER);
					free(datos_yama);
				}

				char *path = malloc(200);
				strcpy(path, PUNTO_MONTAJE);
				strcat(path, "/bitmaps");
				DIR*d;
				d = opendir(path);
				if(!d){
					mkdir(path,0700);
				}
				strcat(path,"/");
				strcat(path, data->nodo);
				strcat(path, ".dat");
				//verifico si existen sus estructuras administrativas
				if (access(path, F_OK) == -1) {
					//no tiene un bitmap,es archivo nuevo
					FILE *f = fopen(path, "w");
					fclose(f);
					int size_bitarray =(int) (size_databin / TAMBLOQUE) % 8 > 0 ?((int) ((int) (size_databin / TAMBLOQUE) / 8))+ 1 :(int) (size_databin / TAMBLOQUE);
					truncate(path, size_bitarray);

					int bitmap = open(path, O_RDWR);
					struct stat mystat;
					void *bmap;
					if (fstat(bitmap, &mystat) < 0) {
						printf("Error al establecer fstat\n");
						close(bitmap);
					} else {
						bmap = mmap(NULL, mystat.st_size,PROT_WRITE | PROT_READ, MAP_SHARED, bitmap, 0);
						if (bmap == MAP_FAILED) {
							printf("Error al mapear a memoria: %s\n",
									strerror(errno));
						} else {
							t_bitarray *bitarray = bitarray_create_with_mode(
									bmap, size_bitarray, MSB_FIRST);
							//cuando se lo crea, se inicializa todos con 0, es decir, todos los bloques libres
							munmap(bmap, mystat.st_size);
							close(bitmap);
							bitarray_destroy(bitarray);
							data->bloques_libres = (int) (size_databin/ TAMBLOQUE);
							data->bloques_totales = (int) (size_databin/ TAMBLOQUE);
						}
					}
				} else {
					//ya existe un bitmap, alguna vez este nodo se conecto
					int bitmap = open(path, O_RDWR);
					struct stat mystat;
					void *bmap;
					if (fstat(bitmap, &mystat) < 0) {
						printf("Error al establecer fstat\n");
						close(bitmap);
					} else {
						bmap = mmap(NULL, mystat.st_size,
								PROT_WRITE | PROT_READ, MAP_SHARED, bitmap, 0);
						if (bmap == MAP_FAILED) {
							printf("Error al mapear a memoria: %s\n",
									strerror(errno));
						} else {
							int size_bitarray =(int) (size_databin / TAMBLOQUE) % 8 > 0 ?((int) ((int) (size_databin/ TAMBLOQUE) / 8)) + 1 :(int) (size_databin / TAMBLOQUE);
							t_bitarray *bitarray = bitarray_create_with_mode(bmap, size_bitarray, MSB_FIRST);
							int i;
							int bloques_ocupados = 0;
							for (i = 0; i < (int) (size_databin / TAMBLOQUE);
									++i) {
								if (bitarray_test_bit(bitarray, i) == 1) {
									bloques_ocupados++;
								}
							}
							data->bloques_totales = (int) (size_databin/ TAMBLOQUE);
							data->bloques_libres = data->bloques_totales- bloques_ocupados;
							bitarray_destroy(bitarray);
							munmap(bmap, mystat.st_size);
							close(bitmap);
						}
					}
				}
				sin_datanodes=false;
				actualizar_nodos_bin(data);
				pthread_mutex_lock(&mutex_datanodes);
				list_add(datanodes, data);
				pthread_mutex_unlock(&mutex_datanodes);
				if(verificar_estado()==1){
					printf("Filesystem en estado seguro");
					fflush(stdout);
					estable=true;
					rechazado=false;
				}
			}
				break;
			case RESULOPERACION: {

				//recibimos los datos

				datos = paquete.Payload;
				uint32_t numero_bloque_datanode;
				uint32_t resultado;
				uint32_t tamanio_bloque;
				uint32_t numero_copia;
				uint32_t numero_bloque_enviado;
				char *nombre_nodo = malloc(100);
				char *nombre_archivo=malloc(100);

				numero_bloque_datanode = *((uint32_t*) datos);
				datos += sizeof(uint32_t);
				numero_copia = *((uint32_t*) datos);
				datos += sizeof(uint32_t);
				resultado = *((uint32_t*) datos);
				datos += sizeof(uint32_t);
				tamanio_bloque = *((uint32_t*) datos);
				datos += sizeof(uint32_t);
				numero_bloque_enviado = *((uint32_t*) datos);
				datos += sizeof(uint32_t);
				strcpy(nombre_nodo, datos);
				nombre_nodo = realloc(nombre_nodo, strlen(nombre_nodo) + 1);
				datos += strlen(nombre_nodo) + 1;
				strcpy(nombre_archivo, datos);
				nombre_archivo = realloc(nombre_archivo,strlen(nombre_archivo) + 1);
				datos += strlen(nombre_archivo) + 1;
				pthread_mutex_lock(&mutex_archivos_actuales);
				t_list*archivos = dictionary_get(archivos_actuales,nombre_nodo);
				//t_archivo_actual *elemento = obtener_elemento(archivos,nombre_archivo);
				pthread_mutex_unlock(&mutex_archivos_actuales);
				if (dictionary_has_key(archivos_terminados, nombre_archivo)) {
					//termino el archivo,hay que eliminarlo y sacarlo de la lista de archivos actuales
					pthread_mutex_lock(&mutex_archivos_actuales);
					if(unico_bloque){
						crear_y_actualizar_archivo(((t_archivo_actual*)obtener_elemento(archivos,nombre_archivo)),
								numero_bloque_enviado, numero_bloque_datanode,tamanio_bloque, numero_copia, nombre_nodo,nombre_archivo);
						unico_bloque=false;
					}
					crear_y_actualizar_archivo(((t_archivo_actual*)obtener_elemento(archivos,nombre_archivo))
							,numero_bloque_enviado, numero_bloque_datanode,tamanio_bloque, numero_copia, nombre_nodo,nombre_archivo);
					eliminar_archivo(nombre_nodo, nombre_archivo);
					pthread_mutex_unlock(&mutex_archivos_actuales);
				} else {
					if (resultado == 1 && !dictionary_has_key(archivos_erroneos,nombre_archivo)) {
						//escribir y crear directorio y archivo
						pthread_mutex_lock(&mutex_archivos_actuales);
						((t_archivo_actual*)obtener_elemento(archivos,nombre_archivo))->total_bloques -= 1;
						crear_y_actualizar_archivo(((t_archivo_actual*)obtener_elemento(archivos,nombre_archivo)),numero_bloque_enviado, numero_bloque_datanode,tamanio_bloque, numero_copia, nombre_nodo,nombre_archivo);
						pthread_mutex_unlock(&mutex_archivos_actuales);
					} else {
						pthread_mutex_lock(&mutex_archivos_actuales);
						((t_archivo_actual*)obtener_elemento(archivos,nombre_archivo))->total_bloques = -1;
						pthread_mutex_unlock(&mutex_archivos_actuales);
						//eliminar lo que se hizo en el archivo
					}
				}
				//pthread_mutex_unlock(&mutex_archivos_actuales);
				pthread_mutex_unlock(&mutex_cpfrom);
			}
				break;
			case ESHANDSHAKE:{
				datos=paquete.Payload;
				bool tiene_nombre(char*elemento){
					if(strcmp(elemento,datos)==0){
						return true;
					}
					return false;
				}
				if(!list_is_empty(datanodes_anteriores)){
					if(formateado && (list_find(datanodes_anteriores,(void*) tiene_nombre)==NULL)){
						printf("Se rechazo la conexion de %s, el filesystem ya est�� formateado \n",(char*)datos);
						fflush(stdout);
						rechazado=true;
						close(socketFD);
						break;
					}
					if(!estable && (list_find(datanodes_anteriores,(void*) tiene_nombre)==NULL)){
						printf("Se rechazo la conexion de %s, el sistema no esta estable y nunca se conect�� este nodo \n",(char*)datos);
						fflush(stdout);
						rechazado=true;
						close(socketFD);
						break;
					}
				}else{
					if(formateado){
						printf("Se rechazo la conexion de %s, nunca se conecto este nodo \n",(char*)datos);
						fflush(stdout);
						rechazado=true;
						close(socketFD);
						break;
					}
				}
				rechazado=false;
				}
				break;
			}
		} else if (!strcmp(paquete.header.emisor, YAMA)) {
			socketYAMA = socketFD;
			if(!estable){
				close(socketFD);
				rechazado=true;
				break;
			}else{
				yama_online=true;
				switch (paquete.header.tipoMensaje){
					case ESHANDSHAKE:{
						pthread_mutex_lock(&mutex_datanodes);
						int tamanioAEnviar = sizeof(uint32_t); //por a cantidad de elementos de la lista
						void*datosAEnviar=malloc(tamanioAEnviar);
						*((uint32_t*)datosAEnviar) = list_size(datanodes);
						datosAEnviar += sizeof(uint32_t);
						int i;
						for (i = 0; i < list_size(datanodes); i++){
							int tam = sizeof(uint32_t)+
										   + strlen(((info_datanode*)list_get(datanodes, i))->ip_nodo)+1
										   +strlen(((info_datanode*)list_get(datanodes, i))->nodo) + 1;
							datosAEnviar -= tamanioAEnviar;
							datosAEnviar = realloc(datosAEnviar, tamanioAEnviar+tam);
							datosAEnviar += tamanioAEnviar;
							tamanioAEnviar+=tam;
							//serializacion elemento
							*((uint32_t*)datosAEnviar) = ((info_datanode*)list_get(datanodes, i))->puertoworker;
							datosAEnviar += sizeof(uint32_t);
							strcpy(datosAEnviar,((info_datanode*)list_get(datanodes, i))->ip_nodo);
							datosAEnviar+=strlen(((info_datanode*)list_get(datanodes, i))->ip_nodo)+1;
							strcpy(datosAEnviar,((info_datanode*)list_get(datanodes, i))->nodo);
							datosAEnviar+=strlen(((info_datanode*)list_get(datanodes, i))->nodo)+1;
						}
						datosAEnviar -= tamanioAEnviar;
						EnviarDatosTipo(socketYAMA,FILESYSTEM,datosAEnviar,tamanioAEnviar,LISTAWORKERS);
						pthread_mutex_unlock(&mutex_datanodes);
						free(datosAEnviar);
					}
					break;
					case SOLICITUDBLOQUESYAMA:{
						datos=(paquete.Payload+7);
						int index_padre=index_ultimo_directorio(datos,"a");
						char**separado_por_barras=string_split(datos,"/");
						int i=0;
						while(separado_por_barras[i]){
							i++;
						}
						i--;
						char*ruta_archivo_en_metadata=malloc(100);
						strcpy(ruta_archivo_en_metadata,RUTA_ARCHIVOS);
						strcat(ruta_archivo_en_metadata,"/");
						char*string_index;
						string_index=integer_to_string(string_index,index_padre);
						strcat(ruta_archivo_en_metadata,string_index);
						strcat(ruta_archivo_en_metadata,"/");
						strcat(ruta_archivo_en_metadata,separado_por_barras[i]);
						t_list*lista_bloques =obtener_lista_bloques(ruta_archivo_en_metadata);
						//la funcion obtener_lista_bloques me carga la lista

						int tamanioAEnviar = sizeof(uint32_t); //por a cantidad de elementos de la lista
						datos = malloc(tamanioAEnviar);
						*((uint32_t*)datos) = list_size(lista_bloques);
						datos += sizeof(uint32_t);

						for (i = 0; i < list_size(lista_bloques); i++){
							t_bloque_yama* bloque = list_get(lista_bloques, i);
							int tam = sizeof(uint32_t) * 2 + //por tamanio y numero_bloque
										   + sizeof(uint32_t) * 2 + //por el bloque_nodo de cada copia
										   + strlen(bloque->primer_nombre_nodo) + strlen(bloque->segundo_nombre_nodo) + 2; //por los nombres de los nodos y sus /n
							datos -= tamanioAEnviar;
							datos = realloc(datos, tamanioAEnviar+tam);
							datos += tamanioAEnviar;
							tamanioAEnviar+=tam;
							//serializacion elemento
							((uint32_t*)datos)[0] = bloque->numero_bloque;
							((uint32_t*)datos)[1] = bloque->tamanio;
							((uint32_t*)datos)[2] = bloque->primer_bloque_nodo;
							((uint32_t*)datos)[3] = bloque->segundo_bloque_nodo;
							datos += sizeof(uint32_t) * 4;
							strcpy(datos,bloque->primer_nombre_nodo);
							datos+=strlen(bloque->primer_nombre_nodo)+1;
							strcpy(datos,bloque->segundo_nombre_nodo);
							datos+=strlen(bloque->segundo_nombre_nodo)+1;
						}
						datos -= tamanioAEnviar;
						datos = realloc(datos, tamanioAEnviar+sizeof(uint32_t));
						datos += tamanioAEnviar;
						*((uint32_t*)datos) = *((uint32_t*)(paquete.Payload+paquete.header.tamPayload-sizeof(uint32_t)));
						datos -= tamanioAEnviar;
						EnviarDatosTipo(socketYAMA,FILESYSTEM,datos,tamanioAEnviar+sizeof(uint32_t),SOLICITUDBLOQUESYAMA);
						free(string_index);
						free(datos);
					}
					break;
				}
			}
		} else if (!strcmp(paquete.header.emisor, WORKER)) {
			if(!estable){
				break;
			}else{
				switch (paquete.header.tipoMensaje) {
					case ESDATOS: {
						void* datos = paquete.Payload;
						//AlmacenarArchivo();
					}
						break;

					}
			}
		} else
			perror("No es ni YAMA ni DATANODE NI WORKER");

		if (paquete.Payload != NULL)
			free(paquete.Payload);
	}
	close(socketFD);
	sacar_datanode(socketFD);
	if(!rechazado){
		void *datos_datanode;
		datos_datanode=malloc(strlen(nodo_desconectado)+1);
		strcpy(datos_datanode,nodo_desconectado);
		EnviarDatosTipo(socketYAMA,FILESYSTEM,datos_datanode,strlen(nodo_desconectado)+1,NODODESCONECTADO);
	}


}
void crear_y_actualizar_ocupados(char *ruta_directorio) {
	t_directory **directorios = obtener_directorios();
	t_directory *aux = (*directorios);
	actualizar_directorios_ocupados(aux);
	memmove(aux, crear(aux, ruta_directorio), 100 * sizeof(t_directory));
	guardar_directorios(directorios);
	free((*directorios));
	free(directorios);
}
void actualizar_directorios_ocupados(t_directory aux[100]) {
	int i;
	for (i = 0; i < 100; ++i) {
		if (strcmp(aux[i].nombre, "/0") == 0) {
			directorios_ocupados[i] = false;
		} else {
			directorios_ocupados[i] = true;
		}
	}
}
void actualizar_index_directorios(t_directory aux[100]) {
	int i;
	for (i = 0; i < 100; ++i) {
		if ((strcmp(aux[i].nombre, "/0") != 0)
				&& (aux[i].index >= index_directorio)) {
			index_directorio = aux[i].index + 1;
		}
	}
}
crear(t_directory aux[100], char *ruta) {
	//verifica si existe, y si no existe, lo crea;
	pthread_mutex_lock(&mutex_directorio);
	//para que al momento de asignarl
	actualizar_index_directorios(aux);
	pthread_mutex_unlock(&mutex_directorio);
	char**separado_por_barras = string_split(ruta, "/");
	int i = 0;
	int padre_anterior = 0;
	while (separado_por_barras[i]) {
		if (!(existe(separado_por_barras[i], padre_anterior, aux))) {
			int index_array = primer_index_libre();
			if (index_array != -1) {
				aux[index_array].padre = padre_anterior;
				strcpy(aux[index_array].nombre, separado_por_barras[i]);
				pthread_mutex_lock(&mutex_directorios_ocupados);
				directorios_ocupados[index_array] = true;
				pthread_mutex_unlock(&mutex_directorios_ocupados);
				pthread_mutex_lock(&mutex_directorio);
				aux[index_array].index = index_directorio;
				index_directorio++;
				pthread_mutex_unlock(&mutex_directorio);
			} else {
				printf("No hay mas lugar libre");
				break;
			}
		}
		padre_anterior = obtener_index_directorio(separado_por_barras[i],
				padre_anterior, aux);
		i++;
	}
	return aux;
}
int primer_index_libre() {
	int i;
	for (i = 0; i < 100; ++i) {
		if (directorios_ocupados[i] == false) {
			return i;
		}
	}
	return -1;
}
obtener_index_directorio(char *nombre, int index_padre, t_directory aux[100]) {
	int i;
	for (i = 0; i < 100; i++) {
		if ((aux[i].padre == index_padre)
				&& (strcmp(aux[i].nombre, nombre) == 0)) {
			return aux[i].index;
		}
	}
}
existe(char*nombre, int index_padre, t_directory aux[100]) {
	int i;
	for (i = 0; i < 100; i++) {
		if ((aux[i].padre == index_padre)
				&& (strcmp(aux[i].nombre, nombre) == 0)) {
			return true;
		}
	}
	return false;

}

bool existe_ruta(char *ruta_fs) {
	char *ruta = calloc(1, strlen(RUTA_DIRECTORIOS) + 1);
	strcpy(ruta, RUTA_DIRECTORIOS);
	int fd_directorio = open(ruta, O_RDWR);
	if (fd_directorio == -1) {
		printf("Error al intentar abrir el archivo");
		return 0;
	} else {
		struct stat mystat;
		void *directorios;
		if (fstat(fd_directorio, &mystat) < 0) {
			printf("Error al establecer fstat\n");
			close(fd_directorio);
			return 0;
		}
		directorios = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ,
				MAP_SHARED, fd_directorio, 0);
		if (directorios == MAP_FAILED) {
			printf("Error al mapear a memoria: %s\n", strerror(errno));
			return 0;
		} else {
			t_directory aux[100];
			memmove(aux, directorios, 100 * sizeof(t_directory));
			char**separado_por_barras= string_split(ruta_fs, "/");
			int cantidad = 0;
			while (separado_por_barras[cantidad]) {
				cantidad++;
			}
			if (cantidad == 1) {
				//ruta con un solo directorio
				return existe(separado_por_barras[cantidad - 1], 0, aux);
			} else {
				//ruta formada por mas de un directorio
				int index_padre_anterior = 0;
				int i = 0;
				cantidad--;
				while (cantidad >= 0) {
					if (i == 0) {
						if (!(existe(separado_por_barras[i],
								index_padre_anterior, aux))) {
							munmap(directorios, mystat.st_size);
							close(fd_directorio);
							return false;
						}
					} else {
						if (!(existe(separado_por_barras[i],
								index_padre_anterior, aux))) {
							munmap(directorios, mystat.st_size);
							close(fd_directorio);
							return false;
						}

					}
					index_padre_anterior = obtener_index_directorio(
							separado_por_barras[i], index_padre_anterior, aux);
					i++;
					cantidad--;
				}
				munmap(directorios, mystat.st_size);
				close(fd_directorio);
				return true;
			}
		}
		free(ruta);
	}
}
bool existe_archivo(char *nombre_archivo, int index_directorio_padre) {
	char*ruta = malloc(200);
	strcpy(ruta, PUNTO_MONTAJE);
	strcat(ruta, "/archivos/");
	char*index_padre;
	index_padre=integer_to_string(index_padre,index_directorio_padre);
	strcat(ruta, index_padre);
	strcat(ruta, "/");
	strcat(ruta, nombre_archivo);
	ruta = realloc(ruta, strlen(ruta) + 1);
	if (access(ruta, F_OK) != -1) {
		free(ruta);
		return true;
	}
	free(ruta);
	free(index_padre);
	return false;
}
void rmtree(const char path[]) {
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
		rmdir(path);
	}
	closedir(dir);
}
void limpiar_directorio_original(t_config*archivo_original){
	t_dictionary*nodos=dictionary_create();
	void crear_listas(char*key,char*value){
		if(strlen(key)>12){
			char**separado_por_comas=string_split(value,",");
			char*nodo=string_substring(separado_por_comas[0],1,strlen(separado_por_comas[0]));
			char*bloque=string_substring(separado_por_comas[1],0,strlen(separado_por_comas[1])-1);

			if(dictionary_has_key(nodos,nodo)){
				list_add(((t_list*)dictionary_get(nodos,nodo)),atoi(bloque));
			}else{
				t_list*lista=list_create();
				list_add(lista,atoi(bloque));
				dictionary_put(nodos,nodo,lista);
			}
			free(nodo);
			free(bloque);
			free(separado_por_comas[0]);
			free(separado_por_comas[1]);
			free(separado_por_comas);
		}
	}
	dictionary_iterator(archivo_original->properties,(void*) crear_listas);
	void limpiar(char*key,t_list*elementos){
		char*ruta=malloc(50);
		strcpy(ruta,RUTA_BITMAPS);
		strcat(ruta,"/");
		strcat(ruta,key);
		strcat(ruta,".dat");
		int fd=open(ruta,O_RDWR);
		struct stat mystat;
		void*bmap;
		if(fstat(fd, &mystat) < 0){
			printf("Error al establecer fstat\n");
			close(fd);
		}else{
			bmap = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED,fd, 0);
			if (bmap == MAP_FAILED) {
				printf("Error al mapear a memoria: %s\n", strerror(errno));
			}else{
				t_bitarray *bitarray=bitarray_create_with_mode(bmap,mystat.st_size,MSB_FIRST);
				int var;
				for (var = 0; var < list_size(elementos); ++var) {
					bitarray_clean_bit(bitarray,(int)list_get(elementos,var));
				}
				bitarray_destroy(bitarray);
				munmap(bmap,mystat.st_size);
				close(fd);
			}
		}
		free(ruta);
		list_destroy(elementos);
	}
	dictionary_iterator(nodos,(void*) limpiar);
	dictionary_destroy(nodos);
	bool primera_vez=true;
	void actualizar(info_datanode*elemento){
		char*ruta=malloc(50);
		strcpy(ruta,RUTA_BITMAPS);
		strcat(ruta,"/");
		strcat(ruta,elemento->nodo);
		strcat(ruta,".dat");
		int bitmap = open(ruta, O_RDWR);
		struct stat mystat;
		void *bmap;
		if (fstat(bitmap, &mystat) < 0) {
			printf("Error al establecer fstat\n");
			close(bitmap);
		} else {
			bmap = mmap(NULL, mystat.st_size,PROT_WRITE | PROT_READ, MAP_SHARED, bitmap, 0);
			if (bmap == MAP_FAILED) {
				printf("Error al mapear a memoria: %s\n",
						strerror(errno));
			} else {
				t_bitarray *bitarray = bitarray_create_with_mode(bmap, mystat.st_size, MSB_FIRST);
				int i;
				elemento->bloques_libres=0;
				for (i = 0; i < elemento->bloques_totales;++i) {
					if (bitarray_test_bit(bitarray, i) == 0) {
						elemento->bloques_libres++;
					}
				}
				bitarray_destroy(bitarray);
				munmap(bmap, mystat.st_size);
				close(bitmap);
			}
		}
		free(ruta);
		t_config*archivo=config_create(RUTA_NODOS);
		char*libre=malloc(50);
		strcpy(libre,elemento->nodo);
		strcat(libre,"Libre");
		libre=realloc(libre,strlen(libre)+1);
		char*string_libre=integer_to_string(string_libre,(int)elemento->bloques_libres);
		config_set_value(archivo,libre,string_libre);
		char*string_libre_2= malloc(100);
		if(primera_vez){
			config_set_value(archivo,"LIBRE",string_libre);
			primera_vez=false;
		}else{
			int libre;
			libre = config_get_int_value(archivo, "LIBRE");
			sprintf(string_libre_2, "%i", libre+elemento->bloques_libres);
			string_libre_2 = realloc(string_libre_2, strlen(string_libre_2) + 1);
			config_set_value(archivo, "LIBRE", string_libre_2);
			config_save(archivo);
		}
		config_save_in_file(archivo,RUTA_NODOS);
		config_destroy(archivo);
		free(string_libre);
		free(libre);
		free(string_libre_2);
	}
	pthread_mutex_lock(&mutex_datanodes);
	list_iterate(datanodes, (void*) actualizar);
	pthread_mutex_unlock(&mutex_datanodes);

}
int obtener_socket(char*nombre) {
	bool nombre_igual(info_datanode*datanode) {
		if (strcmp(datanode->nodo, nombre) == 0) {
			return true;
		}
		return false;
	}
	return ((info_datanode*) list_find(datanodes, (void*) nombre_igual))->socket;
}
existe_en_datanode(char *nombre){
	bool igual_nombre(info_datanode*elemento){
		if(strcmp(elemento->nodo,nombre)==0){
			return true;
		}
		return false;
	}
	return list_find(datanodes,(void*) igual_nombre);
}
t_list*inicializar_lista(t_list*lista){
	void inicializar(info_datanode*elemento){
		t_solicitudes*aux=malloc(sizeof(t_solicitudes));
		aux->cantidad=0;
		aux->nombre_nodo=elemento->nodo;
		list_add(lista,aux);
	}
	list_iterate(datanodes,(void*) inicializar);
	return lista;
}
bool existe_en_lista(t_list*lista,char *nombre){
	bool esta(t_solicitudes*elemento){
		return !strcmp(elemento->nombre_nodo,nombre);
	}
	return list_find(lista,(void*) esta);
}
void subir_uno(t_list*lista,char*nombre){
	void subir(t_solicitudes*elemento){
		if(strcmp(elemento->nombre_nodo,nombre)==0){
			elemento->cantidad++;
		}
	}
	list_iterate(lista,(void*) subir);
}
int cantidad_de(t_list* lista,char*nombre){
	bool esta(t_solicitudes*elemento){
			return !strcmp(elemento->nombre_nodo,nombre);
		}
	return ((t_solicitudes*)list_find(lista,(void*) esta))->cantidad;
}
void solicitar_bloques(char*nombre_archivo, int index_padre) {
	char*ruta = malloc(100);
	strcpy(ruta, RUTA_ARCHIVOS);
	strcat(ruta, "/");
	char*string_padre;
	string_padre=integer_to_string(string_padre,index_padre);
	strcat(ruta, string_padre);
	strcat(ruta, "/");
	strcat(ruta, nombre_archivo);
	t_config*archivo = config_create(ruta);
	t_list*disponibilidad_datanodes=list_create();
	disponibilidad_datanodes=inicializar_lista(disponibilidad_datanodes);
	int cantidad = 0;
	while (1) {
		char*bloque_bytes = calloc(1, 50);
		strcpy(bloque_bytes, "BLOQUE");
		char*string_cantidad;
		string_cantidad=integer_to_string(string_cantidad,cantidad);
		strcat(bloque_bytes, string_cantidad);
		strcat(bloque_bytes, "BYTES");
		bloque_bytes = realloc(bloque_bytes, strlen(bloque_bytes) + 1);
		if (config_has_property(archivo, bloque_bytes)) {
			cantidad++;
		} else {
			break;
		}
		free(bloque_bytes);
		free(string_cantidad);
	}
	int aux = cantidad;
	int i = 0;
	bool flag=false;
	while (aux > 0) {
		pthread_mutex_lock(&mutex_md5);
		char*string_i;
		string_i=integer_to_string(string_i,i);
		char*bloque_bytes = calloc(1, 50);
		strcpy(bloque_bytes, "BLOQUE");
		strcat(bloque_bytes, string_i);
		strcat(bloque_bytes, "BYTES");
		bloque_bytes = realloc(bloque_bytes, strlen(bloque_bytes) + 1);
		char*primer_bloque = calloc(1, 50);
		strcpy(primer_bloque, "BLOQUE");
		strcat(primer_bloque, string_i);
		strcat(primer_bloque, "COPIA0");
		primer_bloque = realloc(primer_bloque, strlen(primer_bloque) + 1);
		char*segundo_bloque = calloc(1, 50);
		strcpy(segundo_bloque, "BLOQUE");
		strcat(segundo_bloque, string_i);
		strcat(segundo_bloque, "COPIA1");
		segundo_bloque = realloc(segundo_bloque, strlen(segundo_bloque) + 1);
		char*tercer_bloque=calloc(1,50);
		strcpy(tercer_bloque,"BLOQUE");
		strcat(tercer_bloque,string_i);
		strcat(tercer_bloque,"COPIA2");
		tercer_bloque=realloc(tercer_bloque,strlen(tercer_bloque)+1);
		if(config_has_property(archivo,tercer_bloque)){
			flag=true;
		}
		t_solicitud_bloque*bloque = malloc(sizeof(t_solicitud_bloque));
		if (config_has_property(archivo, primer_bloque) && config_has_property(archivo, segundo_bloque)){
			char**datos_primer_bloque = config_get_array_value(archivo,primer_bloque);
			if (i == 0) {
				if(existe_en_lista(disponibilidad_datanodes,datos_primer_bloque[0])){
					subir_uno(disponibilidad_datanodes,datos_primer_bloque[0]);
					bloque->bloque_nodo = atoi(datos_primer_bloque[1]);
					bloque->nombre_nodo = malloc(20);
					strcpy(bloque->nombre_nodo, datos_primer_bloque[0]);
					bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
				}else{
					char**datos_segundo_bloque = config_get_array_value(archivo,segundo_bloque);
					if(existe_en_lista(disponibilidad_datanodes,datos_segundo_bloque[0])){
						subir_uno(disponibilidad_datanodes,datos_segundo_bloque[0]);
						bloque->bloque_nodo = atoi(datos_segundo_bloque[1]);
						bloque->nombre_nodo = malloc(20);
						strcpy(bloque->nombre_nodo, datos_segundo_bloque[0]);
						bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
					}else{
						if(flag){
							char**datos_tercer_bloque = config_get_array_value(archivo,tercer_bloque);
							if(existe_en_lista(disponibilidad_datanodes,datos_tercer_bloque[0])){
								subir_uno(disponibilidad_datanodes,datos_tercer_bloque[0]);
								bloque->bloque_nodo = atoi(datos_tercer_bloque[1]);
								bloque->nombre_nodo = malloc(20);
								strcpy(bloque->nombre_nodo, datos_tercer_bloque[0]);
								bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
							}
						}
					}
				}
			} else {
				char**datos_segundo_bloque = config_get_array_value(archivo,segundo_bloque);
				if ((existe_en_lista(disponibilidad_datanodes,datos_primer_bloque[0])) && (existe_en_lista(disponibilidad_datanodes,datos_segundo_bloque[0]))){
					if (cantidad_de(disponibilidad_datanodes,datos_primer_bloque[0]) <= cantidad_de(disponibilidad_datanodes,datos_segundo_bloque[0])){
						subir_uno(disponibilidad_datanodes,datos_primer_bloque[0]);
						bloque->bloque_nodo = atoi(datos_primer_bloque[1]);
						bloque->nombre_nodo = malloc(20);
						strcpy(bloque->nombre_nodo, datos_primer_bloque[0]);
						bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
					} else {
						subir_uno(disponibilidad_datanodes,datos_segundo_bloque[0]);
						bloque->bloque_nodo = atoi(datos_segundo_bloque[1]);
						bloque->nombre_nodo = malloc(20);
						strcpy(bloque->nombre_nodo, datos_segundo_bloque[0]);
						bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
					}
				} else{
					//alguno de los dos no esta conectado
					if(existe_en_lista(disponibilidad_datanodes,datos_primer_bloque[0])){
						subir_uno(disponibilidad_datanodes,datos_primer_bloque[0]);
						bloque->bloque_nodo = atoi(datos_primer_bloque[1]);
						bloque->nombre_nodo = malloc(20);
						strcpy(bloque->nombre_nodo, datos_primer_bloque[0]);
						bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
					}else{
						if(existe_en_lista(disponibilidad_datanodes,datos_segundo_bloque[0])){
							subir_uno(disponibilidad_datanodes,datos_segundo_bloque[0]);
							bloque->bloque_nodo = atoi(datos_segundo_bloque[1]);
							bloque->nombre_nodo = malloc(20);
							strcpy(bloque->nombre_nodo, datos_segundo_bloque[0]);
							bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
						}else{
							if(flag){
								char**datos_tercer_bloque=config_get_array_value(archivo,tercer_bloque);
								subir_uno(disponibilidad_datanodes,datos_tercer_bloque[0]);
								bloque->bloque_nodo = atoi(datos_tercer_bloque[1]);
								bloque->nombre_nodo = malloc(20);
								strcpy(bloque->nombre_nodo, datos_tercer_bloque[0]);
								bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
							}
						}
					}
				}
			}
		}else{
			if(config_has_property(archivo,primer_bloque)){
				char**datos_primer_bloque = config_get_array_value(archivo,primer_bloque);
				if(existe_en_lista(disponibilidad_datanodes,datos_primer_bloque[0])){
					subir_uno(disponibilidad_datanodes,datos_primer_bloque[0]);
					bloque->bloque_nodo = atoi(datos_primer_bloque[1]);
					bloque->nombre_nodo = malloc(20);
					strcpy(bloque->nombre_nodo, datos_primer_bloque[0]);
					bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
				}else{
					if(config_has_property(archivo,segundo_bloque)){
						char**datos_segundo_bloque = config_get_array_value(archivo,segundo_bloque);
						if(existe_en_lista(disponibilidad_datanodes,datos_segundo_bloque[0])){
							subir_uno(disponibilidad_datanodes,datos_segundo_bloque[0]);
							bloque->bloque_nodo = atoi(datos_segundo_bloque[1]);
							bloque->nombre_nodo = malloc(20);
							strcpy(bloque->nombre_nodo, datos_segundo_bloque[0]);
							bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
						}else{
							if(flag){
								char**datos_tercer_bloque=config_get_array_value(archivo,tercer_bloque);
								subir_uno(disponibilidad_datanodes,datos_tercer_bloque[0]);
								bloque->bloque_nodo = atoi(datos_tercer_bloque[1]);
								bloque->nombre_nodo = malloc(20);
								strcpy(bloque->nombre_nodo, datos_tercer_bloque[0]);
								bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
							}
						}
					}else{
						if(flag){
							char**datos_tercer_bloque=config_get_array_value(archivo,tercer_bloque);
							subir_uno(disponibilidad_datanodes,datos_tercer_bloque[0]);
							bloque->bloque_nodo = atoi(datos_tercer_bloque[1]);
							bloque->nombre_nodo = malloc(20);
							strcpy(bloque->nombre_nodo, datos_tercer_bloque[0]);
							bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
						}
					}
				}
			}else{
				if(config_has_property(archivo,segundo_bloque)){
					char**datos_segundo_bloque=config_get_array_value(archivo,segundo_bloque);
					if(existe_en_lista(disponibilidad_datanodes,datos_segundo_bloque[0])){
						subir_uno(disponibilidad_datanodes,datos_segundo_bloque[0]);
						bloque->bloque_nodo = atoi(datos_segundo_bloque[1]);
						bloque->nombre_nodo = malloc(20);
						strcpy(bloque->nombre_nodo, datos_segundo_bloque[0]);
						bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
					}else{
						if(flag){
							char**datos_tercer_bloque=config_get_array_value(archivo,tercer_bloque);
							subir_uno(disponibilidad_datanodes,datos_tercer_bloque[0]);
							bloque->bloque_nodo = atoi(datos_tercer_bloque[1]);
							bloque->nombre_nodo = malloc(20);
							strcpy(bloque->nombre_nodo, datos_tercer_bloque[0]);
							bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
						}
					}
				}else{
					if(flag){
						char**datos_tercer_bloque=config_get_array_value(archivo,tercer_bloque);
						subir_uno(disponibilidad_datanodes,datos_tercer_bloque[0]);
						bloque->bloque_nodo = atoi(datos_tercer_bloque[1]);
						bloque->nombre_nodo = malloc(20);
						strcpy(bloque->nombre_nodo, datos_tercer_bloque[0]);
						bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
					}
				}
			}
		}
		bloque->bloque_archivo = i;
		bloque->bloques_totales = cantidad;
		bloque->index_directorio = index_padre;
		bloque->tamanio_bloque = config_get_int_value(archivo, bloque_bytes);
		bloque->nombre_archivo = malloc(70);
		strcpy(bloque->nombre_archivo, nombre_archivo);
		bloque->nombre_archivo = realloc(bloque->nombre_archivo,strlen(nombre_archivo) + 1);
		int tamanio_archivo = config_get_int_value(archivo, "TAMANIO");
		int tamanio = sizeof(uint32_t) * 6
				+ sizeof(char) * strlen(bloque->nombre_archivo) + 1;
		void *datos = malloc(tamanio);
		*((uint32_t*) datos) = bloque->bloque_archivo;
		datos += sizeof(uint32_t);
		*((uint32_t*) datos) = bloque->bloque_nodo;
		datos += sizeof(uint32_t);
		*((uint32_t*) datos) = bloque->bloques_totales;
		datos += sizeof(uint32_t);
		*((uint32_t*) datos) = bloque->index_directorio;
		datos += sizeof(uint32_t);
		*((uint32_t*) datos) = bloque->tamanio_bloque;
		datos += sizeof(uint32_t);
		*((uint32_t*) datos) = tamanio_archivo;
		datos += sizeof(uint32_t);
		strcpy(datos, bloque->nombre_archivo);
		datos += strlen(bloque->nombre_archivo) + 1;
		datos -= tamanio;
		pthread_mutex_lock(&mutex_datanodes);
		int socket = obtener_socket(bloque->nombre_nodo);
		pthread_mutex_unlock(&mutex_datanodes);
		EnviarDatosTipo(socket, FILESYSTEM, datos, tamanio, SOLICITUDBLOQUE);
		free(bloque);
		free(bloque_bytes);
		free(primer_bloque);
		free(segundo_bloque);
		free(tercer_bloque);
		free(string_i);
		//free(string_padre);
		i++;
		aux--;
		cantidad--;
	}
	free(ruta);
	config_destroy(archivo);
	list_destroy_and_destroy_elements(disponibilidad_datanodes,free);

}
int obtener_total_y_formatear_nodos_bin(char*nombre_archivo,bool primera_vez){

	int tamanio_nodo;
	char *nombre_nodo=malloc(100);
	strcpy(nombre_nodo,nombre_archivo);
	nombre_nodo=strtok(nombre_nodo,".");
	nombre_nodo=realloc(nombre_nodo,strlen(nombre_nodo)+1);
	char*total_nodo=malloc(100);
	strcpy(total_nodo,nombre_nodo);
	strcat(total_nodo,"Total");
	total_nodo=realloc(total_nodo,strlen(total_nodo)+1);
	t_config*nodos=config_create(RUTA_NODOS);
	tamanio_nodo=config_get_int_value(nodos,total_nodo);
	char*libre_nodo=malloc(100);
	strcpy(libre_nodo,nombre_nodo);
	strcat(libre_nodo,"Libre");
	libre_nodo=realloc(libre_nodo,strlen(libre_nodo)+1);
	char*string_tamanio_nodo;
	string_tamanio_nodo=integer_to_string(string_tamanio_nodo,tamanio_nodo);
	config_set_value(nodos,libre_nodo,string_tamanio_nodo);
	config_save(nodos);
	char*string_tamanio_anterior;
	if(primera_vez){
		config_set_value(nodos,"LIBRE",string_tamanio_nodo);
		config_set_value(nodos,"NODOS",config_get_string_value(nodos,"NODOS"));
		config_save(nodos);
		config_save_in_file(nodos,RUTA_NODOS);
		config_destroy(nodos);
	}else{
		int tamanio_anterior=config_get_int_value(nodos,"LIBRE");
		tamanio_anterior+=tamanio_nodo;
		string_tamanio_anterior=integer_to_string(string_tamanio_anterior,tamanio_anterior);
		config_set_value(nodos,"LIBRE",string_tamanio_anterior);
		config_set_value(nodos,"NODOS",config_get_string_value(nodos,"NODOS"));
		config_save(nodos);
		config_save_in_file(nodos,RUTA_NODOS);
		config_destroy(nodos);
		free(string_tamanio_anterior);
	}
	free(total_nodo);
	free(libre_nodo);
	free(nombre_nodo);
	free(string_tamanio_nodo);
	return tamanio_nodo;
}
void formatear_bitarray(char*nombre_archivo,bool primera_vez){
	int size_databin=obtener_total_y_formatear_nodos_bin(nombre_archivo,primera_vez);
	int size_bitarray =size_databin % 8 > 0 ? ((int) (size_databin / 8)) + 1 : (int) (size_databin / 8);
	char*ruta=malloc(100);
	strcpy(ruta,RUTA_BITMAPS);
	strcat(ruta,"/");
	strcat(ruta,nombre_archivo);
	ruta=realloc(ruta,strlen(ruta)+1);
	int bitmap = open(ruta, O_RDWR);
	struct stat mystat;
	void *bmap;
	if (fstat(bitmap, &mystat) < 0) {
		printf("Error al establecer fstat\n");
		close(bitmap);
	} else {
		bmap = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED,bitmap, 0);
		if (bmap == MAP_FAILED) {
			printf("Error al mapear a memoria: %s\n", strerror(errno));
		} else {
			t_bitarray *bitarray=bitarray_create(bmap,size_bitarray);
			int var;
			for (var = 0; var < size_databin; ++var) {
				bitarray_clean_bit(bitarray,var);
			}
			bitarray_destroy(bitarray);
			munmap(bmap,mystat.st_size);
			close(bitmap);
		}
	}
	free(ruta);
}
void igualar(info_datanode*elemento){
	//printf(elemento->bloques_libres);
	elemento->bloques_libres=elemento->bloques_totales;
}
void solicitar_bloques_cpto(char*nombre_archivo,int index_padre,char*ruta_a_guardar){
	char*ruta = malloc(100);
	strcpy(ruta, RUTA_ARCHIVOS);
	strcat(ruta, "/");
	char*string_index_padre;
	string_index_padre=integer_to_string(string_index_padre,index_padre);
	strcat(ruta,string_index_padre);
	strcat(ruta, "/");
	strcat(ruta, nombre_archivo);
	t_config*archivo = config_create(ruta);
	t_list*disponibilidad_datanodes=list_create();
	disponibilidad_datanodes=inicializar_lista(disponibilidad_datanodes);
	int cantidad = 0;
	char*string_cantidad;
	while (1) {
		char*bloque_bytes = calloc(1, 50);
		char*string_bytes=malloc(10);
		string_cantidad=integer_to_string(string_cantidad,cantidad);
		strcpy(string_bytes,string_cantidad);
		strcpy(bloque_bytes, "BLOQUE");
		strcat(bloque_bytes, string_bytes);
		strcat(bloque_bytes, "BYTES");
		bloque_bytes = realloc(bloque_bytes, strlen(bloque_bytes) + 1);
		if (config_has_property(archivo, bloque_bytes)) {
			cantidad++;
		} else {
			break;
		}
		free(bloque_bytes);
		free(string_bytes);
	}
	int aux = cantidad;
	int i = 0;
	bool flag=false;
	while (aux > 0) {
		char *string_i;
		string_i=integer_to_string(string_i,i);
		char*bloque_bytes = calloc(1, 50);
		strcpy(bloque_bytes, "BLOQUE");
		strcat(bloque_bytes, string_i);
		strcat(bloque_bytes, "BYTES");
		bloque_bytes = realloc(bloque_bytes, strlen(bloque_bytes) + 1);
		char*primer_bloque = calloc(1, 50);
		strcpy(primer_bloque, "BLOQUE");
		strcat(primer_bloque, string_i);
		strcat(primer_bloque, "COPIA0");
		primer_bloque = realloc(primer_bloque, strlen(primer_bloque) + 1);
		char*segundo_bloque = calloc(1, 50);
		strcpy(segundo_bloque, "BLOQUE");
		strcat(segundo_bloque, string_i);
		strcat(segundo_bloque, "COPIA1");
		segundo_bloque = realloc(segundo_bloque, strlen(segundo_bloque) + 1);
		char*tercer_bloque=calloc(1,50);
		strcpy(tercer_bloque,"BLOQUE");
		strcat(tercer_bloque,string_i);
		strcat(tercer_bloque,"COPIA2");
		tercer_bloque=realloc(tercer_bloque,strlen(tercer_bloque)+1);
		t_solicitud_bloque*bloque = malloc(sizeof(t_solicitud_bloque));
		if (config_has_property(archivo, primer_bloque) && config_has_property(archivo, segundo_bloque)){
			char**datos_primer_bloque = config_get_array_value(archivo,primer_bloque);
			if (i == 0) {
				if(existe_en_lista(disponibilidad_datanodes,datos_primer_bloque[0])){
					subir_uno(disponibilidad_datanodes,datos_primer_bloque[0]);
					bloque->bloque_nodo = atoi(datos_primer_bloque[1]);
					bloque->nombre_nodo = malloc(20);
					strcpy(bloque->nombre_nodo, datos_primer_bloque[0]);
					bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
				}else{
					char**datos_segundo_bloque = config_get_array_value(archivo,segundo_bloque);
					if(existe_en_lista(disponibilidad_datanodes,datos_segundo_bloque[0])){
						subir_uno(disponibilidad_datanodes,datos_segundo_bloque[0]);
						bloque->bloque_nodo = atoi(datos_segundo_bloque[1]);
						bloque->nombre_nodo = malloc(20);
						strcpy(bloque->nombre_nodo, datos_segundo_bloque[0]);
						bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
					}else{
						if(flag){
							char**datos_tercer_bloque = config_get_array_value(archivo,tercer_bloque);
							if(existe_en_lista(disponibilidad_datanodes,datos_tercer_bloque[0])){
								subir_uno(disponibilidad_datanodes,datos_tercer_bloque[0]);
								bloque->bloque_nodo = atoi(datos_tercer_bloque[1]);
								bloque->nombre_nodo = malloc(20);
								strcpy(bloque->nombre_nodo, datos_tercer_bloque[0]);
								bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
							}
						}
					}
				}
			} else {
				char**datos_segundo_bloque = config_get_array_value(archivo,segundo_bloque);
				if ((existe_en_lista(disponibilidad_datanodes,datos_primer_bloque[0])) && (existe_en_lista(disponibilidad_datanodes,datos_segundo_bloque[0]))){
					if (cantidad_de(disponibilidad_datanodes,datos_primer_bloque[0]) <= cantidad_de(disponibilidad_datanodes,datos_segundo_bloque[0])){
						subir_uno(disponibilidad_datanodes,datos_primer_bloque[0]);
						bloque->bloque_nodo = atoi(datos_primer_bloque[1]);
						bloque->nombre_nodo = malloc(20);
						strcpy(bloque->nombre_nodo, datos_primer_bloque[0]);
						bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
					} else {
						subir_uno(disponibilidad_datanodes,datos_segundo_bloque[0]);
						bloque->bloque_nodo = atoi(datos_segundo_bloque[1]);
						bloque->nombre_nodo = malloc(20);
						strcpy(bloque->nombre_nodo, datos_segundo_bloque[0]);
						bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
					}
				} else{
					//alguno de los dos no esta conectado
					if(existe_en_lista(disponibilidad_datanodes,datos_primer_bloque[0])){
						subir_uno(disponibilidad_datanodes,datos_primer_bloque[0]);
						bloque->bloque_nodo = atoi(datos_primer_bloque[1]);
						bloque->nombre_nodo = malloc(20);
						strcpy(bloque->nombre_nodo, datos_primer_bloque[0]);
						bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
					}else{
						if(existe_en_lista(disponibilidad_datanodes,datos_segundo_bloque[0])){
							subir_uno(disponibilidad_datanodes,datos_segundo_bloque[0]);
							bloque->bloque_nodo = atoi(datos_segundo_bloque[1]);
							bloque->nombre_nodo = malloc(20);
							strcpy(bloque->nombre_nodo, datos_segundo_bloque[0]);
							bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
						}else{
							if(flag){
								char**datos_tercer_bloque=config_get_array_value(archivo,tercer_bloque);
								subir_uno(disponibilidad_datanodes,datos_tercer_bloque[0]);
								bloque->bloque_nodo = atoi(datos_tercer_bloque[1]);
								bloque->nombre_nodo = malloc(20);
								strcpy(bloque->nombre_nodo, datos_tercer_bloque[0]);
								bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
							}
						}
					}
				}
			}
		}else{
			if(config_has_property(archivo,primer_bloque)){
				char**datos_primer_bloque = config_get_array_value(archivo,primer_bloque);
				if(existe_en_lista(disponibilidad_datanodes,datos_primer_bloque[0])){
					subir_uno(disponibilidad_datanodes,datos_primer_bloque[0]);
					bloque->bloque_nodo = atoi(datos_primer_bloque[1]);
					bloque->nombre_nodo = malloc(20);
					strcpy(bloque->nombre_nodo, datos_primer_bloque[0]);
					bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
				}else{
					if(config_has_property(archivo,segundo_bloque)){
						char**datos_segundo_bloque = config_get_array_value(archivo,segundo_bloque);
						if(existe_en_lista(disponibilidad_datanodes,datos_segundo_bloque[0])){
							subir_uno(disponibilidad_datanodes,datos_segundo_bloque[0]);
							bloque->bloque_nodo = atoi(datos_segundo_bloque[1]);
							bloque->nombre_nodo = malloc(20);
							strcpy(bloque->nombre_nodo, datos_segundo_bloque[0]);
							bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
						}else{
							if(flag){
								char**datos_tercer_bloque=config_get_array_value(archivo,tercer_bloque);
								subir_uno(disponibilidad_datanodes,datos_tercer_bloque[0]);
								bloque->bloque_nodo = atoi(datos_tercer_bloque[1]);
								bloque->nombre_nodo = malloc(20);
								strcpy(bloque->nombre_nodo, datos_tercer_bloque[0]);
								bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
							}
						}
					}else{
						if(flag){
							char**datos_tercer_bloque=config_get_array_value(archivo,tercer_bloque);
							subir_uno(disponibilidad_datanodes,datos_tercer_bloque[0]);
							bloque->bloque_nodo = atoi(datos_tercer_bloque[1]);
							bloque->nombre_nodo = malloc(20);
							strcpy(bloque->nombre_nodo, datos_tercer_bloque[0]);
							bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
						}
					}
				}
			}else{
				if(config_has_property(archivo,segundo_bloque)){
					char**datos_segundo_bloque=config_get_array_value(archivo,segundo_bloque);
					if(existe_en_lista(disponibilidad_datanodes,datos_segundo_bloque[0])){
						subir_uno(disponibilidad_datanodes,datos_segundo_bloque[0]);
						bloque->bloque_nodo = atoi(datos_segundo_bloque[1]);
						bloque->nombre_nodo = malloc(20);
						strcpy(bloque->nombre_nodo, datos_segundo_bloque[0]);
						bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
					}else{
						if(flag){
							char**datos_tercer_bloque=config_get_array_value(archivo,tercer_bloque);
							subir_uno(disponibilidad_datanodes,datos_tercer_bloque[0]);
							bloque->bloque_nodo = atoi(datos_tercer_bloque[1]);
							bloque->nombre_nodo = malloc(20);
							strcpy(bloque->nombre_nodo, datos_tercer_bloque[0]);
							bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
						}
					}
				}else{
					if(flag){
						char**datos_tercer_bloque=config_get_array_value(archivo,tercer_bloque);
						subir_uno(disponibilidad_datanodes,datos_tercer_bloque[0]);
						bloque->bloque_nodo = atoi(datos_tercer_bloque[1]);
						bloque->nombre_nodo = malloc(20);
						strcpy(bloque->nombre_nodo, datos_tercer_bloque[0]);
						bloque->nombre_nodo = realloc(bloque->nombre_nodo,strlen(bloque->nombre_nodo) + 1);
					}
				}
			}
		}
		bloque->bloque_archivo = i;
		bloque->bloques_totales = cantidad;
		bloque->index_directorio = index_padre;
		bloque->tamanio_bloque = config_get_int_value(archivo, bloque_bytes);
		bloque->nombre_archivo = malloc(70);
		strcpy(bloque->nombre_archivo, nombre_archivo);
		bloque->nombre_archivo = realloc(bloque->nombre_archivo,strlen(nombre_archivo) + 1);
		int tamanio_archivo = config_get_int_value(archivo, "TAMANIO");
		int tamanio = sizeof(uint32_t) * 6+ sizeof(char) * strlen(bloque->nombre_archivo) + 1 + sizeof(char)*strlen(ruta_a_guardar)+1;
		void *datos = malloc(tamanio);
		*((uint32_t*) datos) = bloque->bloque_archivo;
		datos += sizeof(uint32_t);
		*((uint32_t*) datos) = bloque->bloque_nodo;
		datos += sizeof(uint32_t);
		*((uint32_t*) datos) = bloque->bloques_totales;
		datos += sizeof(uint32_t);
		*((uint32_t*) datos) = bloque->index_directorio;
		datos += sizeof(uint32_t);
		*((uint32_t*) datos) = bloque->tamanio_bloque;
		datos += sizeof(uint32_t);
		*((uint32_t*) datos) = tamanio_archivo;
		datos += sizeof(uint32_t);
		strcpy(datos, bloque->nombre_archivo);
		datos += strlen(bloque->nombre_archivo) + 1;
		strcpy(datos,ruta_a_guardar);
		datos+=strlen(ruta_a_guardar)+1;
		datos -= tamanio;
		pthread_mutex_lock(&mutex_datanodes);
		printf("%s\n",bloque->nombre_nodo);
		int socket = obtener_socket(bloque->nombre_nodo);
		pthread_mutex_unlock(&mutex_datanodes);
		pthread_mutex_lock(&mutex_cpto);
		EnviarDatosTipo(socket, FILESYSTEM, datos, tamanio, SOLICITUDBLOQUECPTO);
		free(bloque);
		free(bloque_bytes);
		free(primer_bloque);
		free(segundo_bloque);
		free(tercer_bloque);
		//free(string_index_padre);
		free(string_i);
		//free(string_cantidad);
		i++;
		aux--;
		cantidad--;
	}
	free(ruta);
	config_destroy(archivo);
	list_destroy_and_destroy_elements(disponibilidad_datanodes,free);
}
bool existe_una_copia_sola(t_config *archivo,char*numero_bloque){
	char*primera_copia=malloc(100);
	char*segunda_copia=malloc(100);
	char*tercera_copia=malloc(100);
	strcpy(primera_copia,"BLOQUE");
	strcpy(segunda_copia,"BLOQUE");
	strcpy(tercera_copia,"BLOQUE");
	strcat(primera_copia,numero_bloque);
	strcat(segunda_copia,numero_bloque);
	strcat(tercera_copia,numero_bloque);
	strcat(primera_copia,"COPIA0");
	strcat(segunda_copia,"COPIA1");
	strcat(tercera_copia,"COPIA2");
	int cantidad=0;
	if(config_has_property(archivo,primera_copia)){
		cantidad++;
	}
	if(config_has_property(archivo,segunda_copia)){
		cantidad++;
	}
	if(config_has_property(archivo,tercera_copia)){
		cantidad++;
	}
	return cantidad==1 ? true :false;
}
void consola() {
	char * linea;
	while (true) {
		linea = readline(">> ");
		if (linea)
			add_history(linea);
		if (!strcmp(linea, "format")) {
			if (access(RUTA_DIRECTORIOS, F_OK) == -1) {
				FILE *f = fopen(RUTA_DIRECTORIOS, "w");
				fclose(f);
			}
			crear_directorio();
			setear_directorio(0, 0, "root", -1);
			DIR*directory;
			directory=opendir(RUTA_ARCHIVOS);
			if(!directory){
				mkdir(RUTA_ARCHIVOS,0700);
			}
			rmtree(RUTA_ARCHIVOS);
			mkdir(RUTA_ARCHIVOS, 0700);
			if(!sin_datanodes){
				DIR*d;
				struct dirent *dir;
				d = opendir(RUTA_BITMAPS);
				bool primera_vez=true;
				int i=0;
				if (d){
					while ((dir = readdir(d)) != NULL)
					{
						if(!(strcmp(dir->d_name,".")==0 || strcmp(dir->d_name,"..")==0)){
							if(i!=0){
								primera_vez=false;
							}
							formatear_bitarray(dir->d_name,primera_vez);
						}
						i++;
					}
					closedir(d);
				}
				if(list_size(datanodes)!=0){
					list_iterate(datanodes,(void*) igualar);
				}
			}
			formateado=true;
			estable = true;

		}
		else if (!strncmp(linea, "rm ", 3)) {
			linea += 3;
			if (!strncmp(linea, "-d", 2)) {
				linea -= 3;
				char **array_input=string_split(linea," ");
				if(!array_input[0] || !array_input[1] || !array_input[2]){
					printf("Error,verifique los par��metros \n");
				}else{
					int index=index_ultimo_directorio(array_input[2],"d");
					DIR*d;
					struct dirent *ep;
					int i=0;
					char*ruta_directorio=malloc(100);
					strcpy(ruta_directorio,RUTA_ARCHIVOS);
					strcat(ruta_directorio,"/");
					char*string_index;
					string_index=integer_to_string(string_index,index);
					strcat(ruta_directorio,string_index);
					ruta_directorio=realloc(ruta_directorio,strlen(ruta_directorio)+1);
					d=opendir(ruta_directorio);
					bool flag_closedir=false;
					if(d){
						while ((ep = readdir (d))!=NULL){
							if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, "..")){
								continue;
							}else{
								i++;
							}
						}
						closedir(d);
						flag_closedir=true;
					}
					if(i!=0){
						printf("Error, el directorio no est�� vac��o \n");
					}else{
						t_directory**directorios=obtener_directorios();
						t_directory *aux=(*directorios);
						int var;
						for (var = 0; var < 100; ++var) {
							if(aux[var].index==index){
								aux[var].index=-4518;
								strcpy(aux[var].nombre,"/0");
								pthread_mutex_lock(&mutex_directorios_ocupados);
								directorios_ocupados[var]=false;
								pthread_mutex_unlock(&mutex_directorios_ocupados);
								break;
							}
						}
						guardar_directorios(directorios);
						free((*directorios));
						free(directorios);
						if(!flag_closedir){
							free(ruta_directorio);
						}
					}
					free(string_index);
				}

			} else if (!strncmp(linea, "-b", 2)){
				linea -= 3;
				char **array_input=string_split(linea," ");
				if(!array_input[0] || !array_input[1] || !array_input[2] || !array_input[3] || !array_input[4]){
					printf("Error,verifique los par��metros \n");
				}else{
					char *ruta_archivo=malloc(100);
					strcpy(ruta_archivo,RUTA_ARCHIVOS);
					strcat(ruta_archivo,"/");
					int index_padre=index_ultimo_directorio(array_input[2],"a");
					char *string_index_padre;
					string_index_padre=integer_to_string(string_index_padre,index_padre);
					strcpy(index_padre,string_index_padre);
					strcat(ruta_archivo,string_index_padre);
					strcat(ruta_archivo,"/");
					strcat(ruta_archivo,((char*)obtener_nombre_archivo(array_input[2])));
					if(!existe_archivo(((char*)obtener_nombre_archivo(array_input[2])),index_padre)){
						printf("Error, el archivo no existe \n");
					}else{
						if(!strcmp(array_input[4],"0") || !strcmp(array_input[4],"1") || !strcmp(array_input[4],"2")){
							t_config*archivo=config_create(ruta_archivo);
							char*copia_a_eliminar=malloc(100);
							char**array=malloc(sizeof(char*)*2);
							char*nodo_a_limpiar=malloc(10);
							int bloque_a_limpiar;
							array[0]=malloc(10);
							array[1]=malloc(5);
							strcpy(copia_a_eliminar,"BLOQUE");
							strcat(copia_a_eliminar,array_input[3]);
							strcat(copia_a_eliminar,"COPIA");
							strcat(copia_a_eliminar,array_input[4]);
							if(existe_una_copia_sola(archivo,array_input[3])){
								printf("Error, existe una sola copia del archivo \n");
							}
							if(config_has_property(archivo,copia_a_eliminar)){
								array=config_get_array_value(archivo,copia_a_eliminar);
								strcpy(nodo_a_limpiar,((char*)array[0]));
								bloque_a_limpiar=atoi(array[1]);
								config_remove_key(archivo,copia_a_eliminar);
								config_save_in_file(archivo,ruta_archivo);
								char*ruta_bitmap=malloc(100);
								strcpy(ruta_bitmap,RUTA_BITMAPS);
								strcat(ruta_bitmap,"/");
								strcat(ruta_bitmap,nodo_a_limpiar);
								strcat(ruta_bitmap,".dat");
								void *bitarray_mmap;
								int bitmap=open(ruta_bitmap,O_RDWR);
								if(bitmap==-1){
									printf("Error al abrir el archivo, no existe \n");
									close(bitmap);
								}else{
									struct stat mystat;
									if (fstat(bitmap, &mystat) < 0) {
										printf("Error al establecer fstat\n");
										close(bitmap);
									}else{
										bitarray_mmap=mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ,MAP_SHARED, bitmap, 0);
										if (mmap == MAP_FAILED) {
											printf("Error al mapear a memoria: %s\n", strerror(errno));
										}else{
											t_bitarray *bitarray = bitarray_create_with_mode(bitarray_mmap,mystat.st_size, MSB_FIRST);
											printf("%i\n",bitarray_test_bit(bitarray,bloque_a_limpiar));
											bitarray_clean_bit(bitarray,bloque_a_limpiar);
											printf("%i\n",bitarray_test_bit(bitarray,bloque_a_limpiar));
											bitarray_destroy(bitarray);
										}
										munmap(bitarray_mmap,mystat.st_size);
										close(bitmap);
									}
								}
								free(ruta_bitmap);
							}else{
								printf("Error, no existe una copia del bloque %s en %s \n",array_input[3],array_input[4]);
							}
						}else{
							printf("Error, no existe una copia con ese valor \n");
						}
					}
					free(string_index_padre);
				}
			}
			else{
				linea -= 3;
				char **array_input=string_split(linea," ");
				if(!array_input[0] || !array_input[1] ){
					printf("Error,verifique los par��metros \n");
				}else{
					char *nombre_archivo=malloc(100);
					strcpy(nombre_archivo,obtener_nombre_archivo(array_input[1]));
					nombre_archivo=realloc(nombre_archivo,strlen(nombre_archivo)+1);
					int index_directorio_padre=index_ultimo_directorio(array_input[1],"a");
					char *ruta_archivo_yamafs=malloc(100);
					strcpy(ruta_archivo_yamafs,RUTA_ARCHIVOS);
					strcat(ruta_archivo_yamafs,"/");
					char*string_index_padre;
					string_index_padre=integer_to_string(string_index_padre,index_directorio_padre);
					strcat(ruta_archivo_yamafs,string_index_padre);
					strcat(ruta_archivo_yamafs,"/");
					strcat(ruta_archivo_yamafs,nombre_archivo);
					ruta_archivo_yamafs=realloc(ruta_archivo_yamafs,strlen(ruta_archivo_yamafs)+1);
					t_config*archivo=config_create(ruta_archivo_yamafs);
					limpiar_directorio_original(archivo);
					config_destroy(archivo);
					remove(ruta_archivo_yamafs);
					char *directorio=malloc(100);
					strcpy(directorio,RUTA_ARCHIVOS);
					strcat(directorio,"/");
					strcat(directorio,string_index_padre);
					directorio=realloc(directorio,strlen(directorio)+1);
					DIR*d;
					struct dirent *dir;
					d = opendir(directorio);
					int i=0;
					if (d){
						while ((dir = readdir(d)) != NULL)
						{
							if(!(strcmp(dir->d_name,".")==0 || strcmp(dir->d_name,"..")==0)){
								i++;
							}else{

							}
						}
						closedir(d);
					}
					if(i==0){
						rmdir(directorio);
					}
					free(nombre_archivo);
					free(ruta_archivo_yamafs);
					free(directorio);
					free(string_index_padre);
				}
			}
		}else if (!strncmp(linea, "rename ", 7)) {
			char **array_input = string_split(linea, " ");
			if (!array_input[0] || !array_input[1] || !array_input[2]
					|| !array_input[3]) {
				printf("Error, verificar parametros\n");
				fflush(stdout);
			} else {
				char **separado_por_barras = string_split(array_input[1], "/");
				if (atoi(array_input[3]) == 0) {
					//es un archivo
					int cantidad = 0;
					while (separado_por_barras[cantidad]) {
						cantidad++;
					}
					cantidad--;
					t_directory **directorios = obtener_directorios();
					t_directory *aux = (*directorios);
					int i;
					int padre_anterior = 0;
					for (i = 0; i < cantidad; ++i) {
						padre_anterior = obtener_index_directorio(
								separado_por_barras[i], padre_anterior, aux);
					}
					free((*directorios));
					free(directorios);
					char *ruta_vieja = malloc(100);
					strcpy(ruta_vieja, RUTA_ARCHIVOS);
					strcat(ruta_vieja, "/");
					char*string_padre;
					string_padre=integer_to_string(string_padre,padre_anterior);
					strcat(ruta_vieja, string_padre);
					strcat(ruta_vieja, "/");
					strcat(ruta_vieja, separado_por_barras[cantidad]);
					ruta_vieja = realloc(ruta_vieja, strlen(ruta_vieja) + 1);
					char *ruta_nueva = malloc(100);
					strcpy(ruta_nueva, RUTA_ARCHIVOS);
					strcat(ruta_nueva, "/");
					strcat(ruta_nueva, string_padre);
					strcat(ruta_nueva, "/");
					strcat(ruta_nueva, array_input[2]);
					ruta_nueva = realloc(ruta_nueva, strlen(ruta_nueva) + 1);
					rename(ruta_vieja, ruta_nueva);
					free(ruta_vieja);
					free(ruta_nueva);
					free(string_padre);
				} else {
					//es un directorio
					if (!existe_ruta(array_input[1])) {
						printf("El directorio no existe,creelo por favor\n");
						fflush(stdout);
					} else {
						t_directory **directorios = obtener_directorios();
						t_directory *aux = (*directorios);
						int i = 0;
						int padre_anterior = 0;
						int index_ultimo_directorio;
						while (separado_por_barras[i]) {
							if (!(separado_por_barras[i + 1])) {
								index_ultimo_directorio =
										obtener_index_directorio(
												separado_por_barras[i],
												padre_anterior, aux);
							} else {
								padre_anterior = obtener_index_directorio(
										separado_por_barras[i], padre_anterior,
										aux);
							}
							i++;
						}
						int var;
						for (var = 1; var < 100; ++var) {
							if (aux[var].index == index_ultimo_directorio) {
								strcpy(aux[var].nombre, array_input[2]);
								break;
							}
						}
						guardar_directorios(directorios);
						free((*directorios));
						free(directorios);
					}
				}
			}
		} else if (!strncmp(linea, "mv ", 3)){
			char**array_input=string_split(linea," ");
			if(!array_input[0] || !array_input[1] || !array_input[2] || !array_input[3]){
				printf("Error, verificar par��metros \n");
			}else{
				int tipo=atoi(array_input[3]);
				char*ruta_original=malloc(100);
				char*ruta_destino=malloc(100);
				char *nombre_archivo=malloc(100);
				char*ruta_archivo_original=malloc(100);
				char*directorio_destino=malloc(100);
				strcpy(ruta_original,array_input[1]);
				strcpy(ruta_destino,array_input[2]);
				ruta_original=realloc(ruta_original,strlen(ruta_original)+1);
				ruta_destino=realloc(ruta_destino,strlen(ruta_destino)+1);
				if(tipo==0){
					//es un archivo
					if(!existe_ruta(ruta_destino)){
						printf("Error, no existe la ruta destino");
					}else{
						int index_directorio_padre_original=index_ultimo_directorio(ruta_original,"a");
						int index_directorio_padre_destino=index_ultimo_directorio(ruta_destino,"d");
						strcpy(nombre_archivo,obtener_nombre_archivo(ruta_original));
						nombre_archivo=realloc(nombre_archivo,strlen(nombre_archivo)+1);
						if(!existe_archivo(nombre_archivo,index_directorio_padre_original)){
							printf("Error,no existe el archivo original \n");
						}else{
							strcpy(ruta_archivo_original,RUTA_ARCHIVOS);
							strcat(ruta_archivo_original,"/");
							char*index_original;
							index_original=integer_to_string(index_original,index_directorio_padre_original);
							strcat(ruta_archivo_original,index_original);
							strcat(ruta_archivo_original,"/");
							strcat(ruta_archivo_original,nombre_archivo);
							ruta_archivo_original=realloc(ruta_archivo_original,strlen(ruta_archivo_original)+1);
							t_config*archivo_original=config_create(ruta_archivo_original);
							strcpy(directorio_destino,RUTA_ARCHIVOS);
							strcat(directorio_destino,"/");
							char*index_destino;
							index_destino=integer_to_string(index_destino,index_directorio_padre_destino);
							strcat(directorio_destino,index_destino);
							directorio_destino=realloc(directorio_destino,strlen(directorio_destino)+1);
							DIR*d;
							d=opendir(directorio_destino);
							if(!d){
								mkdir(directorio_destino,0700);
							}
							closedir(directorio_destino);
							directorio_destino=malloc(100);
							strcpy(directorio_destino,RUTA_ARCHIVOS);
							strcat(directorio_destino,"/");
							strcat(directorio_destino,index_destino);
							strcat(directorio_destino,"/");
							strcat(directorio_destino,nombre_archivo);
							directorio_destino=realloc(directorio_destino,strlen(directorio_destino)+1);
							if(access(directorio_destino,F_OK)==-1){
								FILE*f=fopen(directorio_destino,"w");
								fclose(f);
							}
							config_save_in_file(archivo_original,directorio_destino);
							config_destroy(archivo_original);
							remove(ruta_archivo_original);
							char*directorio_original=malloc(100);
							strcpy(directorio_original,RUTA_ARCHIVOS);
							strcat(directorio_original,"/");
							strcat(directorio_original,index_original);
							directorio_original=realloc(directorio_original,strlen(directorio_original)+1);
							struct dirent *dir;
							DIR*directory;
							directory= opendir(directorio_original);
							int i=0;
							if (directory){
								while ((dir = readdir(directory)) != NULL)
								{
									if(!(strcmp(dir->d_name,".")==0 || strcmp(dir->d_name,"..")==0)){
										i++;
									}else{

									}
								}
							}
							closedir(directory);
							if(i==0){
								rmdir(directorio_original);
							}
							closedir(d);
							free(index_original);
							free(index_destino);
						}
					}
				}else{
					//es un directorio
					int ultimo_index_original=index_ultimo_directorio(ruta_original,"d");
					int ultimo_index_destino=index_ultimo_directorio(ruta_destino,"d");
					t_directory **directorios = obtener_directorios();
					t_directory *aux = (*directorios);
					int var;
					for (var = 0; var < 100; ++var) {
						if(aux[var].index==ultimo_index_original){
							aux[var].padre=ultimo_index_destino;
							break;
						}
					}
					guardar_directorios(directorios);
					free((*directorios));
					free(directorios);

				}
				free(nombre_archivo);
				free(ruta_archivo_original);
				free(directorio_destino);
				free(ruta_original);
				free(ruta_destino);

			}
		}
		else if (!strncmp(linea, "cat ", 4)){
			char **array_input = string_split(linea, " ");
			if (!array_input[0] || !array_input[1]) {
				printf("Error, verificar parametros\n");
				fflush(stdout);
			}else{
				char*nombre=malloc(100);
				strcpy(nombre,obtener_nombre_archivo(array_input[1]));
				nombre=realloc(nombre,strlen(nombre)+1);
				int index=index_ultimo_directorio(array_input[1],"a");
				if(!existe_archivo(nombre,index)){
					printf("Error, el archivo no existe en ese directorio \n");
					fflush(stdout);
				}else{
					solicitar_bloques_cpto(nombre,index,"cat");
				}
			}
		}
		else if (!strncmp(linea, "mkdir ", 6)) {
			char **array_input = string_split(linea, " ");
			if (!array_input[0] || !array_input[1]) {
				printf("Error, verificar parametros\n");
				fflush(stdout);
			}
			if (existe_ruta(array_input[1])) {
				printf("El directorio ya existe!\n");
				fflush(stdout);
			} else {
				printf("El directorio no existe, se crear��\n");
				fflush(stdout);
				crear_y_actualizar_ocupados(array_input[1]);
				printf("Directorio creado correctamente\n");
				fflush(stdout);
			}
		} else if (!strncmp(linea, "cpfrom ", 7)) {
			/*
			 * cpfrom	[path_archivo_origen]	[directorio_yamafs]	 [tipo_archivo]	-	Copiar	un	archivo	local	al	yamafs,
			 siguiendo	los	lineamientos	en	la	operaci��n	Almacenar	Archivo,	de	la	Interfaz	del
			 FileSystem.
			 * */
			char **array_input = string_split(linea, " ");
			int cantidad = 0;
			while (1) {
				if (array_input[cantidad]) {
					cantidad++;
				} else {
					break;
				}
			}
			if (!array_input[1] || !array_input[2] || cantidad > 4) {
				printf("Error, verifique par��metros\n");
				fflush(stdout);
			}
			char *path_archivo = malloc(strlen(array_input[1]) + 1);
			strcpy(path_archivo, array_input[1]);
			char *nombre_archivo = malloc(100);
			nombre_archivo = obtener_nombre_archivo(path_archivo);
			char *directorio_yama = malloc(strlen(array_input[2]) + 1);
			strcpy(directorio_yama, array_input[2]);
			if (!existe_ruta(directorio_yama)) {
				printf("El directorio no existe, se crear��\n");
				fflush(stdout);
				crear_y_actualizar_ocupados(directorio_yama);
			}
			int index_directorio_padre=index_ultimo_directorio(directorio_yama,"d");
			int tipo_archivo = atoi(array_input[3]);
			t_list *bloques_a_enviar = list_create();
			int archivo = open(path_archivo, O_RDWR);
			struct stat archivo_stat;
			void *data;
			fstat(archivo, &archivo_stat);
			int size_aux = archivo_stat.st_size;
			data = mmap(0, archivo_stat.st_size, PROT_READ, MAP_SHARED, archivo,0);
			bool condicion = true;
			if (tipo_archivo == 0) {
				//archivo de texto
				int i = TAMBLOQUE - 1;
				while (size_aux > TAMBLOQUE) {
					if (((char*) data)[i] == '\n' || ((char*) data)[i] == '\r') {
						//void *datos_bloque = calloc(1, i + 1);
						//memcpy(datos_bloque, data, i + 1);
						bloque *bloque_add = calloc(1, sizeof(bloque));
						bloque_add->datos=malloc(i+1);
						memcpy(bloque_add->datos,data,i+1);
						//bloque_add->datos = datos_bloque;
						bloque_add->tamanio = i + 1;
						list_add(bloques_a_enviar, bloque_add);
						data += i + 1;
						size_aux -= i + 1;
					} else {
						i--;
					}
				}
				if (condicion) {
					//void *datos_bloque = calloc(1, size_aux);
					//memcpy(datos_bloque, data, size_aux);
					bloque *bloque_add = calloc(1, sizeof(bloque));
					bloque_add->datos=calloc(1, size_aux);
					memcpy(bloque_add->datos,data,size_aux);
					//bloque_add->datos = datos_bloque;
					bloque_add->tamanio = size_aux;
					list_add(bloques_a_enviar, bloque_add);
				}
			} else {
				//archivo binario
				if (size_aux > TAMBLOQUE) {
					//tama��o archivo mayor al de un bloque
					while (size_aux > TAMBLOQUE) {
						void *datos_bloque = calloc(1, TAMBLOQUE);
						memcpy(datos_bloque, data, TAMBLOQUE);
						bloque *bloque_add = calloc(1, sizeof(bloque));
						bloque_add->datos = datos_bloque;
						bloque_add->tamanio = TAMBLOQUE;
						list_add(bloques_a_enviar, bloque_add);
						size_aux -= TAMBLOQUE;
						data += TAMBLOQUE;
					}
					if (!(size_aux > TAMBLOQUE)) {
						void *datos_bloque = calloc(1, size_aux);
						memcpy(datos_bloque, data, size_aux);
						bloque *bloque_add = calloc(1, sizeof(bloque));
						bloque_add->datos = datos_bloque;
						bloque_add->tamanio = size_aux;
						list_add(bloques_a_enviar, bloque_add);
					}
				} else {
					//tama��o archivo menor a un bloque
					void *datos_bloque = calloc(1, size_aux);
					memcpy(datos_bloque, data, size_aux);
					bloque *bloque_add = calloc(1, sizeof(bloque));
					bloque_add->datos = datos_bloque;
					bloque_add->tamanio = size_aux;
					list_add(bloques_a_enviar, bloque_add);
				}
			}
			munmap(data, archivo_stat.st_size);
			close(archivo);
			if(list_size(bloques_a_enviar)==1){
				unico_bloque=true;
			}
			if (!(enviarBloques((t_list*) bloques_a_enviar, nombre_archivo,
					index_directorio_padre, tipo_archivo))){
				break;
			}
			free(path_archivo);
			free(nombre_archivo);
			free(directorio_yama);
		} else if (!strncmp(linea, "cpto ", 5)){
			char **array_input = string_split(linea, " ");
			if (!array_input[0] || !array_input[1] || !array_input[2]) {
				printf("Error, verificar parametros\n");
				fflush(stdout);
			}else{
				int index_padre = index_ultimo_directorio(array_input[1], "a");
				char**separado_por_barras = string_split(array_input[1], "/");
				int i = 0;
				while (separado_por_barras[i]) {
					i++;
				}
				i--;
				if (!existe_archivo(separado_por_barras[i], index_padre)) {
					printf("No existe el archivo, por favor creelo\n");
				}else{
					ruta_a_guardar=malloc(100);
					strcpy(ruta_a_guardar,array_input[2]);
					ruta_a_guardar=realloc(ruta_a_guardar,strlen(ruta_a_guardar)+1);
					solicitar_bloques_cpto(separado_por_barras[i], index_padre,ruta_a_guardar);
					pthread_mutex_unlock(&orden_cpto);
					printf("Archivo creado satisfactoriamente \n");
				}
			}
		}
		else if (!strncmp(linea, "cpblock ", 8)){
			char **array_input = string_split(linea, " ");
			if (!array_input[0] || !array_input[1] || !array_input[2] || !array_input[3]) {
				printf("Error, verificar parametros\n");
				fflush(stdout);
			} else {
				char*nombre_archivo=malloc(100);
				strcpy(nombre_archivo,obtener_nombre_archivo(array_input[1]));
				nombre_archivo=realloc(nombre_archivo,strlen(nombre_archivo)+1);
				int index=index_ultimo_directorio(array_input[1],"a");
				if(!existe_archivo(nombre_archivo,index)){
					printf("Error, no existe el archivo \n");
				}else{
					bool mismo_nombre(info_datanode*elemento){
						return !strcmp(elemento->nodo,array_input[3]);
					}
					if(!list_find(datanodes,(void*) mismo_nombre)){
						printf("El %s no se encuentra conectado \n",array_input[3]);
					}else{
						char*ruta_config=malloc(150);
						strcpy(ruta_config,RUTA_ARCHIVOS);
						strcat(ruta_config,"/");
						char*string_index;
						string_index=integer_to_string(string_index,index);
						strcat(ruta_config,string_index);
						strcat(ruta_config,"/");
						strcat(ruta_config,nombre_archivo);
						ruta_config=realloc(ruta_config,strlen(ruta_config)+1);
						t_config*archivo=config_create(ruta_config);
						char*bloque=malloc(50);
						strcpy(bloque,"BLOQUE");
						strcat(bloque,array_input[2]);
						strcat(bloque,"COPIA0");
						char*segundo_bloque=malloc(50);
						strcpy(segundo_bloque,"BLOQUE");
						strcat(segundo_bloque,array_input[2]);
						strcat(segundo_bloque,"COPIA1");
						char*tercer_bloque=malloc(50);
						strcpy(tercer_bloque,"BLOQUE");
						strcat(tercer_bloque,array_input[2]);
						strcat(tercer_bloque,"COPIA2");
						bool flag_existe=false;
						while(1){
							if(config_has_property(archivo,bloque)){
								char**primera_copia=malloc(2*sizeof(char*));
								primera_copia[0]=malloc(10);
								primera_copia[1]=malloc(5);
								primera_copia=config_get_array_value(archivo,bloque);
								if(strcmp(primera_copia[0],array_input[3])==0){
									flag_existe=true;
								}
								free(primera_copia[0]);
								free(primera_copia[1]);
								free(primera_copia);
							}
							if(config_has_property(archivo,segundo_bloque)){
								char**segunda_copia=malloc(2*sizeof(char*));
								segunda_copia[0]=malloc(10);
								segunda_copia[1]=malloc(5);
								segunda_copia=config_get_array_value(archivo,segundo_bloque);
								if(strcmp(segunda_copia[0],array_input[3])==0){
									flag_existe=true;
								}
								free(segunda_copia[0]);
								free(segunda_copia[1]);
								free(segunda_copia);
							}
							if(config_has_property(archivo,tercer_bloque)){
								char**tercera_copia=malloc(2*sizeof(char*));
								tercera_copia[0]=malloc(10);
								tercera_copia[1]=malloc(5);
								tercera_copia=config_get_array_value(archivo,tercer_bloque);
								if(strcmp(tercera_copia[0],array_input[3])==0){
									flag_existe=true;
								}
								free(tercera_copia[0]);
								free(tercera_copia[1]);
								free(tercera_copia);
							}
							break;
						}
						if(flag_existe){
							printf("Error, ya existe una copia en ese nodo, por favor elija otro \n");
						}else{
							char**array=malloc(2*sizeof(char*));
							array[0]=malloc(10);
							array[1]=malloc(5);
							char*tamanio_bloque=malloc(50);
							strcpy(tamanio_bloque,"BLOQUE");
							strcat(tamanio_bloque,array_input[2]);
							strcat(tamanio_bloque,"BYTES");
							tamanio_bloque=realloc(tamanio_bloque,strlen(tamanio_bloque)+1);
							if(config_has_property(archivo,bloque)){
								array=config_get_array_value(archivo,bloque);
								bool mismo_nombre(info_datanode*elemento){
									return !strcmp(elemento->nodo,array[0]);
								}
								int tamanio=sizeof(uint32_t)*2+strlen(array_input[3])+1;
								void*datos=malloc(tamanio);
								*((uint32_t*)datos)=config_get_int_value(archivo,tamanio_bloque);
								datos+=sizeof(uint32_t);
								*((uint32_t*)datos)=atoi(array[1]);
								datos+=sizeof(uint32_t);
								strcpy(datos,array_input[3]);
								datos+=strlen(array_input[3])+1;
								datos-=tamanio;
								EnviarDatosTipo(((info_datanode*)list_find(datanodes,(void*) mismo_nombre))->socket,
										FILESYSTEM,datos,tamanio,GETBLOQUE);
								free(datos);
								free(array[0]);
								free(array[1]);
								free(array);
							}else{
								if(config_has_property(archivo,segundo_bloque)){
									array=config_get_array_value(archivo,segundo_bloque);
									bool mismo_nombre(info_datanode*elemento){
										return !strcmp(elemento->nodo,array[0]);
									}
									int tamanio=sizeof(uint32_t)*2;
									void*datos=malloc(tamanio);
									*((uint32_t*)datos)=config_get_int_value(archivo,tamanio_bloque);
									datos+=sizeof(uint32_t);
									*((uint32_t*)datos)=atoi(array[1]);
									datos+=sizeof(uint32_t);
									datos-=tamanio;
									EnviarDatosTipo(((info_datanode*)list_find(datanodes,(void*) mismo_nombre))->socket,
											FILESYSTEM,datos,tamanio,GETBLOQUE);
									free(datos);
									free(array[0]);
									free(array[1]);
									free(array);
								}else{
									array=config_get_array_value(archivo,tercer_bloque);
									bool mismo_nombre(info_datanode*elemento){
										return !strcmp(elemento->nodo,array[0]);
									}
									int tamanio=sizeof(uint32_t)*2;
									void*datos=malloc(tamanio);
									*((uint32_t*)datos)=config_get_int_value(archivo,tamanio_bloque);
									datos+=sizeof(uint32_t);
									*((uint32_t*)datos)=atoi(array[1]);
									datos+=sizeof(uint32_t);
									datos-=tamanio;
									EnviarDatosTipo(((info_datanode*)list_find(datanodes,(void*) mismo_nombre))->socket,
											FILESYSTEM,datos,tamanio,GETBLOQUE);
									free(datos);
									free(array[0]);
									free(array[1]);
									free(array);
								}
							}
							free(bloque);
							free(tamanio_bloque);
							free(ruta_config);
							config_destroy(archivo);
							pthread_mutex_lock(&orden_cpblock);
							char*ruta_bitmap=malloc(50);
							strcpy(ruta_bitmap,RUTA_BITMAPS);
							strcat(ruta_bitmap,"/");
							strcat(ruta_bitmap,array_input[3]);
							strcat(ruta_bitmap,".dat");
							int bitmap=open(ruta_bitmap,O_RDWR);
							int rv=1;
							void *bitarray_mmap;
							t_config*nodos_bin=config_create(RUTA_NODOS);
							char*total_string=malloc(100);
							strcpy(total_string,array_input[3]);
							strcat(total_string,"Total");
							int total=config_get_int_value(nodos_bin,total_string);
							int bloque_nodo;
							if(bitmap==-1){
								printf("Error al abrir el archivo, no existe \n");
								close(bitmap);
							}else{
								struct stat mystat;
								if (fstat(bitmap, &mystat) < 0) {
											printf("Error al establecer fstat\n");
											close(bitmap);
								}else{
									bitarray_mmap=mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ,MAP_SHARED, bitmap, 0);
									if (mmap == MAP_FAILED) {
										printf("Error al mapear a memoria: %s\n", strerror(errno));
									}else{
										int size_bitarray =total % 8 > 0 ? ((int) (total / 8)) + 1 : (int) (total / 8);
										t_bitarray *bitarray = bitarray_create_with_mode(bitarray_mmap,size_bitarray, MSB_FIRST);
										int j;
										for (j = 0; j < total; j++) {
											if (bitarray_test_bit(bitarray, j) == 0) {
												bitarray_set_bit(bitarray, j);
												break;
											}
										}
										bloque_nodo=j;
										if(j==(total-1)){
											rv=-1;
										}
										munmap(bitarray_mmap,mystat.st_size);
										close(bitmap);
									}
								}
								if(rv==-1){
									printf("Error, ese nodo no tiene m��s lugar\n");
								}else{
									bool mismo_nombre(info_datanode*elemento){
										return !strcmp(elemento->nodo,cpblock->nombre_nodo);
									}
									int tamanio_a_enviar= 4 *sizeof(uint32_t)+sizeof(char)*strlen(nombre_archivo)+1+cpblock->tamanio;
									void*datos_a_enviar=calloc(1,tamanio_a_enviar);
									*((uint32_t*)datos_a_enviar) = atoi(array_input[2]);//numero de bloque archivo
									datos_a_enviar += sizeof(uint32_t);
									*((uint32_t*)datos_a_enviar) = bloque_nodo; //bloque a setear
									datos_a_enviar+=sizeof(uint32_t);
									*((uint32_t*)datos_a_enviar) =index ; //bloque a setear
									datos_a_enviar+=sizeof(uint32_t);
									*((uint32_t*)datos_a_enviar) = cpblock->tamanio; //bloque a setear
									datos_a_enviar+=sizeof(uint32_t);
									strcpy(datos_a_enviar,nombre_archivo);
									datos_a_enviar+=strlen(nombre_archivo)+1;
									memmove(datos_a_enviar,cpblock->datos,cpblock->tamanio);
									datos_a_enviar+=cpblock->tamanio;
									datos_a_enviar-=tamanio_a_enviar;
									EnviarDatosTipo(((info_datanode*)list_find(datanodes,(void*) mismo_nombre))->socket,
											FILESYSTEM,datos_a_enviar,tamanio_a_enviar,SETBLOQUECPTO);
									free(datos_a_enviar);
								}
							}
							free(ruta_bitmap);
							free(total_string);
							config_destroy(nodos_bin);
							free(cpblock->datos);
							free(cpblock->nombre_nodo);
							free(cpblock);
							free(string_index);
						}
					}
				}
			}
		}
		else if (!strncmp(linea, "md5 ", 4)) {
			char **array_input = string_split(linea, " ");
			if (!array_input[0] || !array_input[1]) {
				printf("Error, verificar parametros\n");
				fflush(stdout);
			} else {
				int index_padre = index_ultimo_directorio(array_input[1], "a");
				char**separado_por_barras = string_split(array_input[1], "/");
				int i = 0;
				while (separado_por_barras[i]) {
					i++;
				}
				i--;
				if (!existe_archivo(separado_por_barras[i], index_padre)) {
					printf("No existe el archivo, por favor creelo");
				} else {
					//enviar bloques
					md5=true;
					solicitar_bloques(separado_por_barras[i], index_padre);
					//trabate con el semaforo
					//se_trabo=true;
					pthread_mutex_lock(&mutex_orden_md5);
					char*string_system=malloc(100);
					strcpy(string_system,"md5sum ");
					strcat(string_system,PUNTO_MONTAJE);
					strcat(string_system,"/");
					strcat(string_system,"temporal_md5");
					string_system=realloc(string_system,strlen(string_system)+1);
					system(string_system);
					//devolve el md5
					char*ruta_temporal=malloc(100);
					strcpy(ruta_temporal,PUNTO_MONTAJE);
					strcat(ruta_temporal,"/temporal_md5");
					ruta_temporal=realloc(ruta_temporal,strlen(ruta_temporal)+1);
					remove(ruta_temporal);
					free(ruta_temporal);
					free(string_system);
					//pthread_mutex_unlock(&mutex_md5);
				}
			}
		} else if (!strncmp(linea, "ls ", 3)) {
			char **array_input = string_split(linea, " ");
			if (!array_input[0] || !array_input[1]) {
				printf("Error, verificar parametros\n");
				fflush(stdout);
			} else {
				if (!existe_ruta(array_input[1])) {
					printf(
							"El directorio no existe,por favor ingrese un directorio v��lido \n");
				} else {
					t_directory **directorios = obtener_directorios();
					t_directory *aux = (*directorios);
					char **separado_por_barras = string_split(array_input[1],
							"/");
					int iterador = 0;
					int padre_anterior = 0;
					int ultimo_index;
					while (separado_por_barras[iterador]) {
						if (!(separado_por_barras[iterador + 1])) {
							ultimo_index = obtener_index_directorio(
									separado_por_barras[iterador],
									padre_anterior, aux);
						} else {
							padre_anterior = obtener_index_directorio(
									separado_por_barras[iterador],
									padre_anterior, aux);
						}
						iterador++;
					}
					free((*directorios));
					free(directorios);
					char *ruta = malloc(100);
					strcpy(ruta, RUTA_ARCHIVOS);
					strcat(ruta, "/");
					char *string_ultimo_index;
					string_ultimo_index=integer_to_string(string_ultimo_index,ultimo_index);
					strcat(ruta, string_ultimo_index);
					ruta = realloc(ruta, strlen(ruta) + 1);
					DIR* dir = opendir(ruta);
					struct dirent *ent;
					if (dir) {
						while ((ent = readdir(dir)) != NULL) {
							if (!(strcmp(ent->d_name, ".") == 0
									|| strcmp(ent->d_name, "..") == 0)) {
								printf("%s\n", ent->d_name);
							}
						}
						closedir(dir);
					}
					free(string_ultimo_index);
				}
			}

		} else if (!strncmp(linea, "info ", 5)){
			char** array_input=string_split(linea," ");
			if (!array_input[0] || !array_input[1]) {
				printf("Error, verificar parametros\n");
				fflush(stdout);
			} else {
				int index_directorio_padre=index_ultimo_directorio(array_input[1],"a");
				char *nombre_archivo=malloc(100);
				strcpy(nombre_archivo,obtener_nombre_archivo(array_input[1]));
				nombre_archivo=realloc(nombre_archivo,strlen(nombre_archivo)+1);
				if(existe_archivo(nombre_archivo,index_directorio_padre)){
					char*ruta_archivo=malloc(100);
					strcpy(ruta_archivo,RUTA_ARCHIVOS);
					strcat(ruta_archivo,"/");
					char*index_padre;
					index_padre=integer_to_string(index_padre,index_directorio_padre);
					strcat(ruta_archivo,index_padre);
					strcat(ruta_archivo,"/");
					strcat(ruta_archivo,nombre_archivo);
					t_config*archivo=config_create(ruta_archivo);
					printf("Nombre Archivo: %s \n",nombre_archivo);
					printf("Tamanio : %s bytes\n",config_get_string_value(archivo,"TAMANIO"));
					bool condicion=true;
					int i=0;
					char*primera_copia;
					char*segunda_copia;
					char*tamanio_bloque;
					char *tercera_copia;
					char*string_i;
					while(condicion){
						primera_copia=malloc(50);
						segunda_copia=malloc(50);
						tamanio_bloque=malloc(50);
						tercera_copia=malloc(50);
						string_i=integer_to_string(string_i,i);
						strcpy(primera_copia,"BLOQUE");
						strcpy(segunda_copia,"BLOQUE");
						strcpy(tamanio_bloque,"BLOQUE");
						strcpy(tercera_copia,"BLOQUE");
						strcat(primera_copia,string_i);
						strcat(segunda_copia,string_i);
						strcat(tamanio_bloque,string_i);
						strcat(tercera_copia,string_i);
						strcat(primera_copia,"COPIA0");
						strcat(segunda_copia,"COPIA1");
						strcat(tamanio_bloque,"BYTES");
						strcat(tercera_copia,"COPIA2");
						primera_copia=realloc(primera_copia,strlen(primera_copia)+1);
						segunda_copia=realloc(segunda_copia,strlen(segunda_copia)+1);
						tamanio_bloque=realloc(tamanio_bloque,strlen(tamanio_bloque)+1);
						tercera_copia=realloc(tercera_copia,strlen(tercera_copia)+1);
						if(config_has_property(archivo,tamanio_bloque)){
							printf("Bloque %s tamanio %s bytes \n",string_i,config_get_string_value(archivo,tamanio_bloque));
							if(config_has_property(archivo,primera_copia) && config_has_property(archivo,segunda_copia)){
								char**array_primera_copia=config_get_array_value(archivo,primera_copia);
								printf("Bloque %s, copia 0 esta en %s en el bloque %s\n",string_i,array_primera_copia[0],array_primera_copia[1]);
								char**array_segunda_copia=config_get_array_value(archivo,segunda_copia);
								printf("Bloque %s, copia 1 esta en %s en el bloque %s\n",string_i,array_segunda_copia[0],array_segunda_copia[1]);
								if(config_has_property(archivo,tercera_copia)){
									char**array_tercera_copia=config_get_array_value(archivo,tercera_copia);
									printf("Bloque %s, copia 2 esta en %s en el bloque %s\n",string_i,array_tercera_copia[0],array_tercera_copia[1]);
								}
							}else{
								if(config_has_property(archivo,primera_copia)){
									char**array_primera_copia=config_get_array_value(archivo,primera_copia);
									printf("Bloque %s, copia 0 esta en %s en el bloque %s\n",string_i,array_primera_copia[0],array_primera_copia[1]);
								}else{
									if(config_has_property(archivo,tercera_copia)){
										char**array_tercera_copia=config_get_array_value(archivo,tercera_copia);
										printf("Bloque %s, copia 2 esta en %s en el bloque %s\n",string_i,array_tercera_copia[0],array_tercera_copia[1]);
									}
									if(config_has_property(archivo,segunda_copia)){
										char**array_segunda_copia=config_get_array_value(archivo,segunda_copia);
										printf("Bloque %s, copia 1 esta en %s en el bloque %s\n",string_i,array_segunda_copia[0],array_segunda_copia[1]);
									}
								}
							}
						}else{
							condicion=false;
						}
						i++;
						free(primera_copia);
						free(segunda_copia);
						free(tercera_copia);
						free(tamanio_bloque);
						free(string_i);
					}
					free(ruta_archivo);
					free(index_padre);
					config_destroy(archivo);
				}else{
					printf("El archivo no existe, por favor creelo \n");
				}
			}
		}
		else if (!strcmp(linea, "exit")) {
			printf("salio\n");
			free(linea);
			break;
		} else
			printf("no se conoce el comando\n");
		free(linea);
	}
}
void mostrar_directorios_hijos(char *path) {
	char *ruta = calloc(1, strlen("/home/utnso/metadata/directorios.dat") + 1);
	strcpy(ruta, "/home/utnso/metadata/directorios.dat");
	int fd_directorio = open(ruta, O_RDWR);
	if (fd_directorio == -1) {
		printf("Error al intentar abrir el archivo");
	} else {
		struct stat mystat;
		void *directorios;
		if (fstat(fd_directorio, &mystat) < 0) {
			printf("Error al establecer fstat\n");
			close(fd_directorio);
		} else {
			directorios = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ,
					MAP_SHARED, fd_directorio, 0);
			if (directorios == MAP_FAILED) {
				printf("Error al mapear a memoria: %s\n", strerror(errno));
			} else {
				t_directory aux[100];
				memmove(aux, directorios, 100 * sizeof(t_directory));
				char **separado_por_barras = string_split(path, "/");
				int cantidad = 0;
				while (separado_por_barras[cantidad]) {
					cantidad++;
				}
				int i = 0;
				int padre_anterior = 0;
				int index_actual;
				while (separado_por_barras[i]) {
					if (!(separado_por_barras[i + 1])) {
						index_actual = obtener_index_directorio(
								separado_por_barras[i], padre_anterior, aux);
					} else {
						padre_anterior = obtener_index_directorio(
								separado_por_barras[i], padre_anterior, aux);
					}
					i++;
				}
				mostrar_directorios(index_actual, aux);
			}
		}
	}
}

void mostrar_directorios(int index, t_directory aux[100]) {
	int i;
	for (i = 0; i < 100; ++i) {
		if (aux[i].padre == index) {
			char *nombre = malloc(100);
			strcpy(nombre, aux[i].nombre);
			nombre = realloc(nombre, strlen(aux[i].nombre) + 1);
			printf("%s\n", nombre);
			free(nombre);
		}
	}
}
bool esta_disponible(info_datanode *un_datanode) {
	char *ruta = calloc(1, 120);
	strcpy(ruta, PUNTO_MONTAJE);
	strcat(ruta, "/nodos.bin");
	ruta = realloc(ruta, strlen(ruta) + 1);
	t_config *nodos = config_create(ruta);
	int libre;
	char *nombre_libre = calloc(1, 100);
	strcpy(nombre_libre, un_datanode->nodo);
	strcat(nombre_libre, "Libre");
	nombre_libre = realloc(nombre_libre, strlen(nombre_libre) + 1);
	libre = config_has_property(nodos, nombre_libre);
	free(nombre_libre);
	return libre > 0;
}

bool verificar_existencia_lista_archivos(t_list*lista_archivos, char*nombre) {
	int existe_nombre(t_archivo_actual *archivo) {
		if (strcmp(archivo->nombre_archivo, nombre) == 0) {
			return true;
		}
		return false;
	}
	return list_any_satisfy(lista_archivos, (void*) existe_nombre);
}
t_list *obtener(t_list *lista, char*nombre) {
	int existe_nombre(t_archivo_actual *archivo) {
		if (strcmp(archivo->nombre_archivo, nombre) == 0) {
			return true;
		}
		return false;
	}
	t_list *aux = list_filter(lista, (void*) existe_nombre);
	return aux;
}
t_datanode_a_enviar*datanodes_disponibles(info_datanode*datanode) {
	t_datanode_a_enviar *nuevo = malloc(sizeof(t_datanode_a_enviar));
	nuevo->nombre = malloc(100);
	strcpy(nuevo->nombre, datanode->nodo);
	nuevo->nombre = realloc(nuevo->nombre, strlen(nuevo->nombre) + 1);
	nuevo->cantidad = datanode->bloques_libres;
	nuevo->socket = datanode->socket;
	return nuevo;
}
int ceilDivision(int tamanio, int tamanioPagina) {
	double cantidadPaginas;
	int cantidadReal;
	cantidadPaginas = (tamanio + tamanioPagina - 1) / tamanioPagina;
	return cantidadPaginas;

}
int se_puede_enviar(t_list*datanodes, int bloques_a_enviar) {
	if (list_size(datanodes) >= 2) {
		int var;
		int contador = 0;
		for (var = list_size(datanodes); var >= 2; --var) {
			int i = 0;
			while (i < list_size(datanodes)) {
				t_datanode_a_enviar *un_datanode = list_get(datanodes, i);
				if (un_datanode->cantidad>= ceilDivision(bloques_a_enviar * 2, var)) {
					contador++;
				}
				i++;
			}
			if (contador >= var) {
				return var;
			} else {
				contador = 0;
			}
		}
		return 0;
	} else {
		return 0;
	}
}
t_list*obtener_disponibles(t_list*datanodes_a_enviar, int cantidad_datanodes,
		int cantidad_bloques) {
	t_list *lista = list_create();
	bool verificar_disponibilidad(t_datanode_a_enviar*elemento) {
		if (elemento->cantidad
				>= ceilDivision(cantidad_bloques, cantidad_datanodes)) {
			return true;
		} else {
			return false;
		}
	}
	lista = list_filter(datanodes_a_enviar, (void*) verificar_disponibilidad);
	return lista;
}
void actualizar_archivos_actuales(char*nombre_nodo, int index_directorio_padre,
		int tipo_archivo, char*nombre_archivo) {
	if (!dictionary_has_key(archivos_actuales, nombre_nodo)){
		//primera vez que se manda un archivo a este nodo
		t_list*lista_archivos = list_create();
		t_archivo_actual *un_archivo = malloc(sizeof(t_archivo_actual));
		un_archivo->index_padre = index_directorio_padre;
		un_archivo->total_bloques = 1;
		un_archivo->tipo = tipo_archivo;
		un_archivo->nombre_archivo = malloc(100);
		strcpy(un_archivo->nombre_archivo, nombre_archivo);
		un_archivo->nombre_archivo = realloc(un_archivo->nombre_archivo,strlen(un_archivo->nombre_archivo) + 1);
		pthread_mutex_lock(&mutex_archivos_actuales);
		list_add(lista_archivos, un_archivo);
		dictionary_put(archivos_actuales, nombre_nodo, lista_archivos);
		pthread_mutex_unlock(&mutex_archivos_actuales);
	} else {
		//ya existe la lista, y tiene algun archivo
		pthread_mutex_lock(&mutex_archivos_actuales);
		t_list *archivos = dictionary_get(archivos_actuales, nombre_nodo);
		if (verificar_existencia_lista_archivos(archivos, nombre_archivo)) {
			//existe el archivo, hay que aumentar el numero de bloque
			//t_archivo_actual*un_archivo = obtener_elemento(archivos,nombre_archivo);
			((t_archivo_actual*)obtener_elemento(archivos,nombre_archivo))->total_bloques += 1;
			pthread_mutex_unlock(&mutex_archivos_actuales);
		} else {
			//no existe el archivo, lo creamos
			t_archivo_actual *un_archivo = malloc(sizeof(t_archivo_actual));
			un_archivo->index_padre = index_directorio_padre;
			un_archivo->total_bloques = 1;
			un_archivo->nombre_archivo = malloc(100);
			strcpy(un_archivo->nombre_archivo, nombre_archivo);
			un_archivo->nombre_archivo = realloc(un_archivo->nombre_archivo,
					strlen(un_archivo->nombre_archivo) + 1);
			list_add(archivos, un_archivo);
			pthread_mutex_unlock(&mutex_archivos_actuales);
		}
	}
}
t_resultado_envio* enviar_bloque(t_list*bloques_a_enviar,
		int i_bloques_a_enviar, int index_copia, char *nombre_archivo,
		int index_directorio_padre, int tipo_archivo, int numero_copia) {
	char *nombre = malloc(100);
	strcpy(nombre,
			((info_datanode*) list_get(datanodes, index_copia))->nodo);
	nombre = realloc(nombre, strlen(nombre) + 1);

	//Se obtiene el numero de bloque del datanode
	char *ruta = malloc(100);
	strcpy(ruta, PUNTO_MONTAJE);
	strcat(ruta, "/bitmaps/");
	strcat(ruta, nombre);
	strcat(ruta, ".dat");
	ruta = realloc(ruta, strlen(ruta) + 1);
	t_config *nodos = config_create(RUTA_NODOS);
	int total;
	char *nombre_total = malloc(100);
	strcpy(nombre_total, nombre);
	strcat(nombre_total, "Total");
	nombre_total = realloc(nombre_total, strlen(nombre_total) + 1);
	total = config_get_int_value(nodos, nombre_total);
	int bitmap = open(ruta, O_RDWR);
	struct stat mystat;
	void *bmap;
	if (fstat(bitmap, &mystat) < 0) {
		printf("Error al establecer fstat\n");
		close(bitmap);
	} else {
		bmap = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED,
				bitmap, 0);
		if (bmap == MAP_FAILED) {
			printf("Error al mapear a memoria: %s\n", strerror(errno));
		} else {
			int size_bitarray =total % 8 > 0 ? ((int) (total / 8)) + 1 : (int) (total / 8);
			t_bitarray *bitarray = bitarray_create_with_mode(bmap,size_bitarray, MSB_FIRST);
			int j;
			for (j = 0; j < total; j++) {
				if (bitarray_test_bit(bitarray, j) == 0) {
					bitarray_set_bit(bitarray, j);
					break;
				}
			}
			int numero_bloque = j;
			munmap(bmap, mystat.st_size);
			close(bitmap);
			bitarray_destroy(bitarray);

			//creamos tipo de dato a enviar y lo inicializamos
			bloque *primer_bloque;
			primer_bloque = list_get(bloques_a_enviar, i_bloques_a_enviar);
			primer_bloque->numero = numero_bloque;
			primer_bloque->copia = numero_copia;
			primer_bloque->nombre_archivo = malloc(100);
			strcpy(primer_bloque->nombre_archivo, nombre_archivo);
			primer_bloque->nombre_archivo = realloc(primer_bloque->nombre_archivo,strlen(primer_bloque->nombre_archivo) + 1);

			//enviamos al datanode el bloque y el tama��o de bloque
			int tamanio = sizeof(uint32_t) * 4 + primer_bloque->tamanio+ sizeof(char) * strlen(primer_bloque->nombre_archivo) + 1;
			void *datos = malloc(tamanio);
			*((uint32_t*) datos) = primer_bloque->numero;
			datos += sizeof(uint32_t);
			*((uint32_t*) datos) = primer_bloque->copia;
			datos += sizeof(uint32_t);
			*((uint32_t*) datos) = primer_bloque->tamanio;
			datos += sizeof(uint32_t);
			*((uint32_t*) datos) = i_bloques_a_enviar;
			datos += sizeof(uint32_t);
			memmove(datos, primer_bloque->datos, primer_bloque->tamanio);
			datos += primer_bloque->tamanio;
			strcpy(datos, primer_bloque->nombre_archivo);
			datos += strlen(primer_bloque->nombre_archivo) + 1;
			datos -= tamanio;
			//actualizo archivos actuales
			actualizar_archivos_actuales(nombre, index_directorio_padre,tipo_archivo, nombre_archivo);
			//enviamos datos
			int socket = ((info_datanode*) list_get(datanodes,index_copia))->socket;
			t_resultado_envio*resultado = malloc(sizeof(t_resultado_envio));
			resultado->bloque = numero_bloque;
			resultado->nombre_nodo = malloc(100);
			strcpy(resultado->nombre_nodo, nombre);
			resultado->nombre_nodo = realloc(resultado->nombre_nodo,strlen(resultado->nombre_nodo) + 1);
			//resultado->nombre_nodo=string_duplicate(nombre);
			resultado->resultado = EnviarDatosTipo(socket, FILESYSTEM, datos,tamanio, SETBLOQUE);
			free(ruta);
			free(nombre);
			config_destroy(nodos);
			free(nombre_total);
			free(datos);
			return resultado;
		}
	}
}
void limpiar_bitarrays(t_list*lista) {
	int var;
	for (var = 0; var < list_size(lista); ++var) {
		char *ruta = malloc(100);
		strcpy(ruta, PUNTO_MONTAJE);
		strcat(ruta, "/bitmaps/");
		strcat(ruta, ((t_bloque_enviado*) list_get(lista, var))->nombre_nodo);
		strcat(ruta, ".dat");
		ruta = realloc(ruta, strlen(ruta) + 1);
		t_config *nodos = config_create(RUTA_NODOS);
		int total;
		char *nombre_total = malloc(100);
		strcpy(nombre_total,
				((t_bloque_enviado*) list_get(lista, var))->nombre_nodo);
		strcat(nombre_total, "Total");
		nombre_total = realloc(nombre_total, strlen(nombre_total) + 1);
		total = config_get_int_value(nodos, nombre_total);
		int bitmap = open(ruta, O_RDWR);
		struct stat mystat;
		void *bmap;
		if (fstat(bitmap, &mystat) < 0) {
			printf("Error al establecer fstat\n");
			close(bitmap);
		} else {
			bmap = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ,
					MAP_SHARED, bitmap, 0);
			if (bmap == MAP_FAILED) {
				printf("Error al mapear a memoria: %s\n", strerror(errno));
			} else {
				int size_bitarray =
						total % 8 > 0 ?
								((int) (total / 8)) + 1 : (int) (total / 8);
				t_bitarray *bitarray = bitarray_create_with_mode(bmap,
						size_bitarray, MSB_FIRST);
				bitarray_clean_bit(bitarray,
						((t_bloque_enviado*) list_get(lista, var))->bloque);
				munmap(bmap, mystat.st_size);
				close(bitmap);
			}
		}
	}
}
void actualizar_datanodes(char *nombre_nodo){
	void restar_uno(info_datanode*elemento){
		if(strcmp(elemento->nodo,nombre_nodo)==0){
			elemento->bloques_libres-=1;
		}
	}
	list_iterate(datanodes,(void*) restar_uno);
}
void actualizar_luego_de_cpfrom(){
	int var;
	for (var = 0; var < list_size(datanodes); ++var) {
		info_datanode*elemento=list_get(datanodes,var);
		char *ruta_nodos = malloc(100);
		strcpy(ruta_nodos, RUTA_NODOS);
		ruta_nodos = realloc(ruta_nodos, strlen(ruta_nodos) + 1);
		t_config *nodos = config_create(ruta_nodos);
		char*string_tamanio_actual;
		char*string_libre;
		if(var==0){
			string_tamanio_actual=integer_to_string(string_tamanio_actual,elemento->bloques_totales);
			config_set_value(nodos,"TAMANIO",string_tamanio_actual);
			string_libre=integer_to_string(string_libre,elemento->bloques_libres);
			config_set_value(nodos,"LIBRE",string_libre);
		}else{
			int tamanio_actual;
			tamanio_actual = config_get_int_value(nodos, "TAMANIO");
			tamanio_actual =tamanio_actual == 0 ? elemento->bloques_totales :tamanio_actual + elemento->bloques_totales;
			string_tamanio_actual = malloc(100);
			sprintf(string_tamanio_actual, "%i", tamanio_actual);
			string_tamanio_actual = realloc(string_tamanio_actual,strlen(string_tamanio_actual) + 1);
			config_set_value(nodos, "TAMANIO", string_tamanio_actual);
			config_save(nodos);
			int libre;
			libre = config_get_int_value(nodos, "LIBRE");
			libre = libre == 0 ? elemento->bloques_libres : libre + elemento->bloques_libres;
			string_libre = malloc(100);
			sprintf(string_libre, "%i", libre);
			string_libre = realloc(string_libre, strlen(string_libre) + 1);
			config_set_value(nodos, "LIBRE", string_libre);
			config_save(nodos);
		}
		char *nodo_actual_total = calloc(1, 100);
		strcpy(nodo_actual_total, elemento->nodo);
		strcat(nodo_actual_total, "Total");
		char *string_bloques_totales = calloc(1, 100);
		sprintf(string_bloques_totales, "%i", elemento->bloques_totales);
		string_bloques_totales = realloc(string_bloques_totales,strlen(string_bloques_totales) + 1);
		config_set_value(nodos, nodo_actual_total, string_bloques_totales);
		config_save(nodos);
		char *nodo_actual_libre = calloc(1, 100);
		strcpy(nodo_actual_libre, elemento->nodo);
		strcat(nodo_actual_libre, "Libre");
		char *string_bloques_libres = calloc(1, 100);
		sprintf(string_bloques_libres, "%i", elemento->bloques_libres);
		string_bloques_libres = realloc(string_bloques_libres,strlen(string_bloques_libres) + 1);
		config_set_value(nodos, nodo_actual_libre, string_bloques_libres);
		config_save(nodos);
		config_save_in_file(nodos, ruta_nodos);
		free(ruta_nodos);
		free(string_tamanio_actual);
		free(string_bloques_libres);
		free(nodo_actual_libre);
		free(string_bloques_totales);
		free(nodo_actual_total);
		free(string_libre);
	}
}
void actualizar_datanodes_global(t_list* datanodes_a_enviar){
	void actualizar(t_dtdisponible*elemento){
		((info_datanode*)list_get(datanodes,elemento->index))->bloques_libres=elemento->cantidad;
	}
	list_iterate(datanodes_a_enviar,(void*) actualizar);
}
int enviarBloques(t_list *bloques_a_enviar, char *nombre_archivo,int index_directorio_padre, int tipo_archivo) {

	t_list *datanodes_a_enviar;
	t_list *disponibles = list_create();
	t_list *bloques_enviados = list_create();
	t_list *index_a_enviar=list_create();
	int index=0;
	t_dtdisponible*datanodes_disponibles(info_datanode*elemento){
		t_dtdisponible*aux=malloc(sizeof(t_dtdisponible));
		aux->socket=elemento->socket;
		aux->cantidad=elemento->bloques_libres;
		aux->index=index;
		aux->nombre=malloc(10);
		strcpy(aux->nombre,elemento->nodo);
		index++;
		return aux;
	}
	bool se_puede_enviar(){
		int i=0;
		int aux=list_size(bloques_a_enviar);
		int primera_copia;
		int segunda_copia;
		bool primera_vez=true;
		bool error=false;
		while(aux>0){
			if(!primera_vez){
				i=i+1>list_size(datanodes_a_enviar)-1 ? 0 : i+1;
			}
			if(((t_dtdisponible*)list_get(datanodes_a_enviar,i))->cantidad>0){
				primera_copia=((t_dtdisponible*)list_get(datanodes_a_enviar,i))->index;
				list_add(index_a_enviar,((t_dtdisponible*)list_get(datanodes_a_enviar,i))->index);
				((t_dtdisponible*)list_get(datanodes_a_enviar,i))->cantidad--;
			}else{
				i=i+1>list_size(bloques_a_enviar)-1 ? -1 : i+1;
				if(i==-1){
					error=true;
					break;
				}else{
					if(((t_dtdisponible*)list_get(datanodes_a_enviar,i))->cantidad>0){
						primera_copia=((t_dtdisponible*)list_get(datanodes_a_enviar,i))->index;
						list_add(index_a_enviar,((t_dtdisponible*)list_get(datanodes_a_enviar,i))->index);
						((t_dtdisponible*)list_get(datanodes_a_enviar,i))->cantidad--;
					}else{
						i=i+1>list_size(datanodes_a_enviar)-1 ? -1 : i+1;
						if(i==-1){
							error=true;
							break;;
						}else{
							if(((t_dtdisponible*)list_get(datanodes_a_enviar,i))->cantidad>0){
								primera_copia=((t_dtdisponible*)list_get(datanodes_a_enviar,i))->index;
								list_add(index_a_enviar,((t_dtdisponible*)list_get(datanodes_a_enviar,i))->index);
								((t_dtdisponible*)list_get(datanodes_a_enviar,i))->cantidad--;
							}else{
								i=i+1>list_size(datanodes_a_enviar)-1 ? -1 : i+1;
								if(i==-1){
									error=true;
									break;
								}else{
									if(((t_dtdisponible*)list_get(datanodes_a_enviar,i))->cantidad>0){
										primera_copia=((t_dtdisponible*)list_get(datanodes_a_enviar,i))->index;
										list_add(index_a_enviar,((t_dtdisponible*)list_get(datanodes_a_enviar,i))->index);
										((t_dtdisponible*)list_get(datanodes_a_enviar,i))->cantidad--;
									}else{
										error=true;
										break;
									}
								}
							}
						}
					}
				}
			}
			i=i+1>list_size(datanodes_a_enviar)-1 ? 0 : i+1;
			if(((t_dtdisponible*)list_get(datanodes_a_enviar,i))->cantidad>0){
				segunda_copia=((t_dtdisponible*)list_get(datanodes_a_enviar,i))->index;
				list_add(index_a_enviar,((t_dtdisponible*)list_get(datanodes_a_enviar,i))->index);
				((t_dtdisponible*)list_get(datanodes_a_enviar,i))->cantidad--;
			}else{
				i=i+1>list_size(datanodes_a_enviar)-1 ? -1 : i+1;
				if(i==-1){
					error=true;
					break;
				}else{
					if(((t_dtdisponible*)list_get(datanodes_a_enviar,i))->cantidad>0){
						segunda_copia=((t_dtdisponible*)list_get(datanodes_a_enviar,i))->index;
						list_add(index_a_enviar,((t_dtdisponible*)list_get(datanodes_a_enviar,i))->index);
						((t_dtdisponible*)list_get(datanodes_a_enviar,i))->cantidad--;
					}else{
						i=i+1>list_size(bloques_a_enviar)-1 ? -1 : i+1;
						if(i==-1 ){
							error=true;
							break;;
						}else{
							if(((t_dtdisponible*)list_get(datanodes_a_enviar,i))->cantidad>0){
								segunda_copia=((t_dtdisponible*)list_get(datanodes_a_enviar,i))->index;
								list_add(index_a_enviar,((t_dtdisponible*)list_get(datanodes_a_enviar,i))->index);
								((t_dtdisponible*)list_get(datanodes_a_enviar,i))->cantidad--;
							}else{
								i=i+1>list_size(datanodes_a_enviar)-1 ? -1 : i+1;
								if(i==-1){
									error=true;
									break;
								}else{
									if(((t_dtdisponible*)list_get(datanodes_a_enviar,i))->cantidad>0){
										segunda_copia=((t_dtdisponible*)list_get(datanodes_a_enviar,i))->index;
										list_add(index_a_enviar,((t_dtdisponible*)list_get(datanodes_a_enviar,i))->index);
										((t_dtdisponible*)list_get(datanodes_a_enviar,i))->cantidad--;
									}else{
										error=true;
										break;
									}
								}
							}
						}
					}
				}
			}
			if(primera_copia==segunda_copia){
				error=true;
				break;
			}
			if(primera_vez){
				primera_vez=false;
			}
			aux--;
		}
		if(error){
			return false;
		}else{
			return true;
		}
	}
	pthread_mutex_lock(&mutex_datanodes);
	datanodes_a_enviar = list_map(datanodes, (void*) datanodes_disponibles);
	pthread_mutex_unlock(&mutex_datanodes);
	bool error = false;
	if(!se_puede_enviar()){
		printf("Error, no hay lugar disponible");
		error=true;
	}
	int size_bloques = list_size(bloques_a_enviar);
	int i_lista_disponibles = 0;
	int i_bloques_a_enviar = 0;
	int index_lista=0;
	if (!error) {
		while (size_bloques){
			//obtenemos datos del  datanode para enviarle la primer copia del bloque
			//primer copia es i_lista_disponibles
			int index_primer_copia = list_get(index_a_enviar,index_lista);
			index_lista++;
			int index_segunda_copia =list_get(index_a_enviar,index_lista);
			t_resultado_envio*resultado_primer_copia;
			t_resultado_envio*resultado_segunda_copia;
			pthread_mutex_lock(&mutex_cpfrom);
			resultado_primer_copia = enviar_bloque(bloques_a_enviar, i_bloques_a_enviar, index_primer_copia,nombre_archivo, index_directorio_padre, tipo_archivo, 0);
			pthread_mutex_lock(&mutex_cpfrom);
			resultado_segunda_copia = enviar_bloque(bloques_a_enviar, i_bloques_a_enviar, index_segunda_copia,nombre_archivo, index_directorio_padre, tipo_archivo, 1);
			if (!(resultado_primer_copia->resultado)|| !(resultado_segunda_copia->resultado)) {
				//limpio todos los bloques enviados
				limpiar_bitarrays(bloques_enviados);
				pthread_mutex_lock(&mutex_archivos_erroneos);
				dictionary_put(archivos_erroneos, nombre_archivo,nombre_archivo);
				pthread_mutex_unlock(&mutex_archivos_erroneos);
				break;
			} else {
				//agrego resultado_primer_copia y resultado_segunda_copia
				t_bloque_enviado*primer_copia = malloc(sizeof(t_bloque_enviado));
				primer_copia->bloque = resultado_primer_copia->bloque;
				primer_copia->nombre_nodo = malloc(100);
				strcpy(primer_copia->nombre_nodo,resultado_primer_copia->nombre_nodo);
				primer_copia->nombre_nodo = realloc(primer_copia->nombre_nodo,strlen(primer_copia->nombre_nodo) + 1);
				actualizar_datanodes(primer_copia->nombre_nodo);
				list_add(bloques_enviados, primer_copia);
				t_bloque_enviado*segunda_copia = malloc(sizeof(t_bloque_enviado));
				segunda_copia->bloque = resultado_segunda_copia->bloque;
				segunda_copia->nombre_nodo = malloc(100);
				strcpy(segunda_copia->nombre_nodo,resultado_segunda_copia->nombre_nodo);
				segunda_copia->nombre_nodo = realloc(segunda_copia->nombre_nodo,strlen(segunda_copia->nombre_nodo) + 1);
				actualizar_datanodes(primer_copia->nombre_nodo);
				list_add(bloques_enviados, segunda_copia);
			}
			size_bloques--;
			i_bloques_a_enviar++;
			index_lista++;
			if (size_bloques <= 0) {
				dictionary_put(archivos_terminados, nombre_archivo,nombre_archivo);
			}
		}
		actualizar_datanodes_global(datanodes_a_enviar);
		pthread_mutex_lock(&mutex_datanodes);
		actualizar_luego_de_cpfrom();
		pthread_mutex_unlock(&mutex_datanodes);
	}
	list_destroy(datanodes_a_enviar);
	list_destroy(disponibles);
	void hacer_free(t_bloque_enviado*elemento){
		free(elemento->nombre_nodo);
		free(elemento);
	}
	void hacer_free_bloques(bloque*elemento){
		free(elemento->datos);
		free(elemento->nombre_archivo);
		free(elemento);
	}
	list_destroy_and_destroy_elements(bloques_enviados,(void*) hacer_free);
	list_destroy_and_destroy_elements(bloques_a_enviar,(void*) hacer_free_bloques);
	if (error) {
		return -1;
	} else {
		return 1;
	}
}

void crear_directorio() {
	t_directory **directorios = malloc(sizeof(t_directory*));
	(*directorios) = malloc(100 * sizeof(t_directory));
	t_directory *aux = (*directorios);
	int i;
	for (i = 1; i < 100; ++i) {
		strcpy(aux[i].nombre, "/0");
	}
	truncate(RUTA_DIRECTORIOS, 100 * sizeof(t_directory));
	guardar_directorios(directorios);
	pthread_mutex_lock(&mutex_directorio);
	index_directorio = 1;
	pthread_mutex_unlock(&mutex_directorio);
	free((*directorios));
	free(directorios);
}

void setear_directorio(int index_array, int index, char *nombre, int padre) {

	t_directory **directorios = obtener_directorios();
	t_directory *aux = (*directorios);
	aux[index_array].index = index;
	strcpy(aux[index_array].nombre, nombre);
	aux[index_array].padre = padre;
	directorios_ocupados[index_array] = true;
	guardar_directorios(directorios);
	free((*directorios));
	free(directorios);
}

int main(int argc, char* argv[]) {
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();
	RUTA_DIRECTORIOS = malloc(strlen(PUNTO_MONTAJE) + strlen("/directorios.dat") + 1);
	strcpy(RUTA_DIRECTORIOS, PUNTO_MONTAJE);
	strcat(RUTA_DIRECTORIOS, "/directorios.dat");
	RUTA_NODOS = malloc(strlen(PUNTO_MONTAJE) + strlen("/nodos.bin") + 1);
	strcpy(RUTA_NODOS, PUNTO_MONTAJE);
	strcat(RUTA_NODOS, "/nodos.bin");
	RUTA_ARCHIVOS = malloc(strlen(PUNTO_MONTAJE) + strlen("/archivos") + 1);
	strcpy(RUTA_ARCHIVOS, PUNTO_MONTAJE);
	strcat(RUTA_ARCHIVOS, "/archivos");
	RUTA_BITMAPS = malloc(strlen(PUNTO_MONTAJE) + strlen("/bitmaps") + 1);
	strcpy(RUTA_BITMAPS, PUNTO_MONTAJE);
	strcat(RUTA_BITMAPS, "/bitmaps");
	char*argumento=malloc(strlen("--clean")+1);
	if(argv[1]){
		strcpy(argumento,argv[1]);
	}else{
		strcpy(argumento,"nada");
	}
	//TODO ACLARACION -----> CODIGO PARA PROBAR LOS DIRECTORIOS PERSISTIDOS

	 /*t_directory **directorios=obtener_directorios();
	 t_directory *aux=(*directorios);
	 int var;
	 for (var = 0; var < 10; ++var) {
	 printf("%i,%i,%s\n",aux[var].index,aux[var].padre,aux[var].nombre);
	 }
	 return 0;
	  */
	if(strcmp(argumento,"--clean")==0){
		rmtree(RUTA_ARCHIVOS);
		rmtree(RUTA_BITMAPS);
		remove(RUTA_NODOS);
		remove(RUTA_DIRECTORIOS);
		mkdir(RUTA_ARCHIVOS,0700);
		mkdir(RUTA_BITMAPS,0700);
		if(access(RUTA_NODOS,F_OK)==-1){
			FILE*f=fopen(RUTA_NODOS,"w");
			fclose(f);
			t_config*archivo=config_create(RUTA_NODOS);
			char*string=integer_to_string(string,0);
			config_set_value(archivo,"TAMANIO",string);
			config_set_value(archivo,"LIBRE",string);
			config_save_in_file(archivo,RUTA_NODOS);
			config_destroy(archivo);
			free(string);
			sin_datanodes=true;
		}
	}else{
		mkdir(RUTA_ARCHIVOS,0700);
		mkdir(RUTA_BITMAPS,0700);
		remove(RUTA_NODOS);
		if(access(RUTA_NODOS,F_OK)==-1){
			FILE*f=fopen(RUTA_NODOS,"w");
			fclose(f);
			t_config*archivo=config_create(RUTA_NODOS);
			char*string=integer_to_string(string,0);
			config_set_value(archivo,"TAMANIO",string);
			config_set_value(archivo,"LIBRE",string);
			config_save_in_file(archivo,RUTA_NODOS);
			config_destroy(archivo);
			free(string);
			sin_datanodes=true;
		}
		datanodes_anteriores=list_create();
		char *ruta_bitmaps_dir=malloc(strlen(RUTA_BITMAPS)+1);
		strcpy(ruta_bitmaps_dir,RUTA_BITMAPS);
		DIR*d;
		d=opendir(ruta_bitmaps_dir);
		struct dirent *entry;
		int cantArch=0;
		while ((entry = readdir(d)) != NULL) {
			if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")){
				continue;
			}else{
				char*nombre=malloc(strlen(entry->d_name)+1);
				strcpy(nombre,entry->d_name);
				nombre=strtok(nombre,".");
				list_add(datanodes_anteriores,nombre);
				cantArch++;;
			}
		}
		closedir(d);
		if(cantArch>0){
			formateado=true;
			estable=false;
			printf("Iniciando FileSystem en estado no seguro \n");
		}
	}
	free(argumento);
	datanodes = list_create();
	archivos_actuales = dictionary_create();
	archivos_terminados = dictionary_create();
	archivos_erroneos = dictionary_create();
	pthread_t hiloConsola;
	pthread_mutex_init(&mutex_datanodes, NULL);
	pthread_mutex_init(&mutex_directorio, NULL);
	pthread_mutex_init(&mutex_directorios_ocupados, NULL);
	pthread_mutex_init(&mutex_archivos_actuales, NULL);
	pthread_mutex_init(&mutex_archivos_erroneos, NULL);
	pthread_mutex_init(&mutex_escribir_archivo, NULL);
	pthread_mutex_init(&mutex_md5, NULL);
	pthread_mutex_init(&mutex_cpfrom,NULL);
	pthread_mutex_init(&mutex_respuesta_solicitud,NULL);
	pthread_mutex_init(&mutex_orden_md5,NULL);
	pthread_mutex_init(&mutex_respuesta_solicitud_cpto,NULL);
	pthread_mutex_init(&mutex_cpto,NULL);
	pthread_mutex_init(&orden_cpto,NULL);
	pthread_mutex_init(&orden_cpblock,NULL);
	pthread_mutex_lock(&orden_cpto);
	pthread_mutex_lock(&mutex_orden_md5);
	pthread_mutex_lock(&orden_cpblock);
	pthread_create(&hiloConsola, NULL, (void*) consola, NULL);
	ServidorConcurrente(IP, PUERTO, FILESYSTEM, &listaHilos, &end, accion);
	pthread_join(hiloConsola, NULL);
	free(PUNTO_MONTAJE);
	free(RUTA_ARCHIVOS);
	free(RUTA_BITMAPS);
	free(RUTA_DIRECTORIOS);
	free(IP);
	return 0;
}
