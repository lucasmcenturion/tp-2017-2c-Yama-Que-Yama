#include "sockets.h"
#include <sys/mman.h>
#define TAMBLOQUE (1024*1024)
char *IP;
char *PUNTO_MONTAJE;
char *RUTA_DIRECTORIOS;
char *RUTA_NODOS;
char *RUTA_ARCHIVOS;
char *RUTA_ARCHIVOS_ALMACENADOS;
char *ruta_a_guardar;
char *RUTA_BITMAPS;
int PUERTO;
int socketFS;
int socketYAMA;
t_list* listaHilos;
t_list* datanodes;
t_list* datanodes_anteriores;
char *nodo_desconectado;
bool directorios_ocupados[100];
t_dictionary *archivos_actuales;
t_dictionary *archivos_erroneos;
t_dictionary *archivos_terminados;
int index_directorio = 1;
int anterior_md5;
bool end;
bool md5=false;
bool formateado=false;
bool unico_bloque=false;
bool inseguro=false;
bool rechazado=false;
bool se_trabo;
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
//pthread_mutex_t mutex_archivos_almacenados;
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
				memmove((*directorios_yamafs), directorios,
						100 * sizeof(t_directory));
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
	char **separado_por_barras = string_split(ruta, "/");
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
	char **separado_por_comas = string_split(nodos_actuales, ",");
	int cantidad_comas = 0;
	int i = 0;
	while (separado_por_comas[cantidad_comas]) {
		cantidad_comas++;
	}
	char *nuevos_nodos = malloc(100);
	if (cantidad_comas == 1) {
		//tiene un solo elemento
		char *substring = malloc(100);
		substring = string_substring(separado_por_comas[0], 1,
				strlen(separado_por_comas[0]) - 2);
		substring = realloc(substring, strlen(substring) + 1);
		if (strcmp(nombre, substring) == 0) {
			config_set_value(nodos, "NODOS", "");
		}
	} else {
		//tiene mas de un elemento
		while (i < cantidad_comas) {
			char * substring = malloc(100);
			if (i == 0) {
				substring = string_substring(separado_por_comas[i], 1,
						strlen(separado_por_comas[i]) - 1);
				substring = realloc(substring, strlen(substring) + 1);
				if (strcmp(nombre, substring) == 0) {
					strcpy(nuevos_nodos, "[");
				} else {
					strcpy(nuevos_nodos, separado_por_comas[i]);
				}
			} else {
				if (i == (cantidad_comas - 1)) {
					substring = string_substring(separado_por_comas[i], 0,
							strlen(separado_por_comas[i]) - 1);
					if (strcmp(nombre, substring) == 0) {
						strcat(nuevos_nodos, "]");
					} else {
						strcat(nuevos_nodos, separado_por_comas[i]);
					}

				} else {
					substring = separado_por_comas[i];
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
	}
	config_set_value(nodos, "NODOS", nuevos_nodos);
	char *libre = malloc(100);
	strcpy(libre, nombre);
	strcat(libre, "Libre");
	if (config_has_property(nodos, libre)) {
		int libre_nodo_a_eliminar = config_get_int_value(nodos, libre);
		int libre_total = config_get_int_value(nodos, "LIBRE");
		int libre_total_a_actualizar =
				libre_total > libre_nodo_a_eliminar ?
						libre_total - libre_nodo_a_eliminar : 0;
		char *string_libre_total_a_actualizar = malloc(100);
		sprintf(string_libre_total_a_actualizar, "%i",
				libre_total_a_actualizar);
		string_libre_total_a_actualizar = realloc(
				string_libre_total_a_actualizar,
				strlen(string_libre_total_a_actualizar) + 1);
		config_set_value(nodos, "LIBRE", string_libre_total_a_actualizar);
		config_set_value(nodos, libre, "0");
		config_save_in_file(nodos, "/home/utnso/metadata/nodos.bin");
	}
	char *total = malloc(100);
	strcpy(total, nombre);
	strcat(total, "Total");
	if (config_has_property(nodos, total)) {
		int total_nodo_a_eliminar = config_get_int_value(nodos, total);
		int total_total = config_get_int_value(nodos, "TAMANIO");
		int total_total_a_actualizar =
				total_total > total_nodo_a_eliminar ?
						total_total - total_nodo_a_eliminar : 0;
		char *string_total_total_a_actualizar = malloc(100);
		sprintf(string_total_total_a_actualizar, "%i",
				total_total_a_actualizar);
		string_total_total_a_actualizar = realloc(
				string_total_total_a_actualizar,
				strlen(string_total_total_a_actualizar) + 1);
		config_set_value(nodos, total, "0");
		config_set_value(nodos, "TAMANIO", string_total_total_a_actualizar);
	}
	config_save_in_file(nodos, "/home/utnso/metadata/nodos.bin");
}

t_archivo_actual *obtener_elemento(t_list*lista, char*nombre_archivo) {
	bool nombre_igual(t_archivo_actual*elemento) {
		if (strcmp(elemento->nombre_archivo, nombre_archivo) == 0) {
			return true;
		}
		return false;
	}
	t_archivo_actual *elemento = malloc(sizeof(t_archivo_actual));
	elemento = list_find(lista, (void*) nombre_igual);
	return elemento;
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
	char *string_tamanio_actual = malloc(100);
	sprintf(string_tamanio_actual, "%i", tamanio_actual);
	string_tamanio_actual = realloc(string_tamanio_actual,strlen(string_tamanio_actual) + 1);
	config_set_value(nodos, "TAMANIO", string_tamanio_actual);
	config_save_in_file(nodos, ruta_nodos);
	int libre;
	libre = config_get_int_value(nodos, "LIBRE");
	libre = libre == 0 ? data->bloques_libres : libre + data->bloques_libres;
	char *string_libre = malloc(100);
	sprintf(string_libre, "%i", libre);
	string_libre = realloc(string_libre, strlen(string_libre) + 1);
	config_set_value(nodos, "LIBRE", string_libre);
	config_save_in_file(nodos, ruta_nodos);
	char *nodos_actuales = calloc(1, 100);
	if (config_has_property(nodos,"NODOS")){
		nodos_actuales = config_get_string_value(nodos, "NODOS");
		char **separado_por_comas = string_split(nodos_actuales, ",");
		int cantidad_comas = 0;
		while (separado_por_comas[cantidad_comas]) {
			cantidad_comas++;
		}
		int iterate = 0;
		char *nuevos_nodos = calloc(1, 300);
		while (iterate < cantidad_comas) {
			if (cantidad_comas == 1) {
				char *substring = calloc(1, 100);
				substring = string_substring(
						separado_por_comas[(cantidad_comas - 1)], 0,strlen(separado_por_comas[(cantidad_comas - 1)]) - 1);
				substring = realloc(substring, strlen(substring) + 1);
				strcpy(nuevos_nodos, substring);
				strcat(nuevos_nodos, ",");
				strcat(nuevos_nodos, data->nodo);
				strcat(nuevos_nodos, "]");
				nuevos_nodos = realloc(nuevos_nodos, strlen(nuevos_nodos) + 1);
			} else {
				if (iterate == 0) {
					strcpy(nuevos_nodos, separado_por_comas[iterate]);
					strcat(nuevos_nodos, ",");
				} else {
					if (iterate == (cantidad_comas - 1)) {
						char *substring = calloc(1, 200);
						substring = string_substring(
								separado_por_comas[(cantidad_comas - 1)], 0,
								strlen(separado_por_comas[(cantidad_comas - 1)])
										- 1);
						substring = realloc(substring, strlen(substring) + 1);
						strcat(nuevos_nodos, substring);
						strcat(nuevos_nodos, ",");
						strcat(nuevos_nodos, data->nodo);
						strcat(nuevos_nodos, "]");
						nuevos_nodos = realloc(nuevos_nodos,
								strlen(nuevos_nodos) + 1);
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
		char* nodo_nuevo = calloc(1, 120);
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
	string_bloques_totales = realloc(string_bloques_totales,
			strlen(string_bloques_totales) + 1);
	config_set_value(nodos, nodo_actual_total, string_bloques_totales);
	config_save_in_file(nodos, ruta_nodos);
	char *nodo_actual_libre = calloc(1, 100);
	strcpy(nodo_actual_libre, data->nodo);
	strcat(nodo_actual_libre, "Libre");
	char *string_bloques_libres = calloc(1, 100);
	sprintf(string_bloques_libres, "%i", data->bloques_libres);
	string_bloques_libres = realloc(string_bloques_libres,
			strlen(string_bloques_libres) + 1);
	config_set_value(nodos, nodo_actual_libre, string_bloques_libres);
	config_save_in_file(nodos, ruta_nodos);
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
	printf("Configuración:\n"
			"IP=%s\n"
			"PUERTO=%d\n"
			"PUNTO_MONTAJE=%s\n", IP, PUERTO, PUNTO_MONTAJE);
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
	index_directorio_padre = integer_to_string(elemento->index_padre);
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
	strcpy(key, "BLOQUE");
	strcat(key, integer_to_string(numero_bloque_enviado));
	strcat(key, "COPIA");
	strcat(key, integer_to_string(numero_copia));
	key = realloc(key, strlen(key) + 1);
	char *value = malloc(100);
	strcpy(value, "[");
	strcat(value, nombre_nodo);
	strcat(value, ",");
	strcat(value, integer_to_string(numero_bloque_datanode));
	strcat(value, "]");
	value = realloc(value, strlen(value) + 1);
	config_set_value(archivo, key, value);
	config_save(archivo);
	char *tamanio = malloc(100);
	strcpy(tamanio, "BLOQUE");
	strcat(tamanio, integer_to_string(numero_bloque_enviado));
	strcat(tamanio, "BYTES");
	int tamanio_anterior = 0;
	if (!config_has_property(archivo, tamanio)) {
		tamanio_anterior = tamanio_bloque;
		config_set_value(archivo, tamanio, integer_to_string(tamanio_bloque));
		config_save(archivo);
	}
	if (config_has_property(archivo, "TAMANIO")) {
		int tamanio_total = config_get_int_value(archivo, "TAMANIO");
		tamanio_total += tamanio_anterior;
		config_set_value(archivo, "TAMANIO", integer_to_string(tamanio_total));
		config_save(archivo);
	} else {
		config_set_value(archivo, "TAMANIO", integer_to_string(tamanio_bloque));
		config_save(archivo);
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
}
void eliminar_archivo(char*nombre_nodo, char*nombre_archivo) {
	t_list*lista = dictionary_get(archivos_actuales, nombre_nodo);
	bool tiene_nombre(t_archivo_actual* archivo) {
		if (strcmp(archivo->nombre_archivo, nombre_archivo) == 0) {
			return true;
		}
		return false;
	}
	t_archivo_actual *archivo = malloc(sizeof(t_archivo_actual));
	archivo = list_remove_by_condition(lista, (void*) tiene_nombre);
	//free(archivo);
	dictionary_put(archivos_actuales, nombre_nodo, lista);
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
		bmap+=anterior_md5;
		if (bmap == MAP_FAILED) {
			printf("Error al mapear a memoria: %s\n", strerror(errno));
			close(temporal);
		} else {
			memmove(bmap, bloque, tamanio_bloque);
			munmap(bmap, mystat.st_size);
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
		strcat(bytes,integer_to_string(i));
		strcat(bytes,"BYTES");
		bytes=realloc(bytes,strlen(bytes)+1);
		char *primera_copia=malloc(100);
		strcpy(primera_copia,"BLOQUE");
		strcat(primera_copia,integer_to_string(i));
		strcat(primera_copia,"COPIA0");
		char *segunda_copia=malloc(100);
		strcpy(segunda_copia,"BLOQUE");
		strcat(segunda_copia,integer_to_string(i));
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
	}
	return bloques;
}


void accion(void* socket) {
	int socketFD = *(int*) socket;
	Paquete paquete;
	void* datos;
	while (RecibirPaqueteServidorFS(socketFD, FILESYSTEM, &paquete,inseguro,formateado) > 0) {
		if (!strcmp(paquete.header.emisor, DATANODE)) {
			switch (paquete.header.tipoMensaje) {
			case NUEVOWORKER: {
				datos = paquete.Payload;
				int tamanio = paquete.header.tamPayload;
				EnviarDatosTipo(socketYAMA, FILESYSTEM, datos, tamanio,
						NUEVOWORKER);
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
					if(!se_trabo){
						sleep(1);
					}
					pthread_mutex_unlock(&mutex_md5);
				}else{
					pthread_mutex_unlock(&mutex_md5);
				}
				pthread_mutex_unlock(&mutex_respuesta_solicitud);
			}
				break;
			case IDENTIFICACIONDATANODE: {
				datos = paquete.Payload;
				info_datanode *data = calloc(1, sizeof(info_datanode));
				uint32_t size_databin = *((uint32_t*) datos);
				datos += sizeof(uint32_t);
				data->nodo = malloc(110);
				strcpy(data->nodo, datos);
				data->nodo = realloc(data->nodo, strlen(data->nodo) + 1);
				datos -= sizeof(uint32_t);
				data->socket = socketFD;
				char *path = malloc(200);
				strcpy(path, PUNTO_MONTAJE);
				strcat(path, "/bitmaps");
				DIR*d;
				struct dirent *dir;
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
							munmap(bmap, mystat.st_size);
							close(bitmap);
						}
					}
				}
				actualizar_nodos_bin(data);
				pthread_mutex_lock(&mutex_datanodes);
				list_add(datanodes, data);
				pthread_mutex_unlock(&mutex_datanodes);
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
				char *nombre_archivo = malloc(100);

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
				t_archivo_actual *elemento = obtener_elemento(archivos,nombre_archivo);
				pthread_mutex_unlock(&mutex_archivos_actuales);
				if (dictionary_has_key(archivos_terminados, nombre_archivo)) {
					//termino el archivo,hay que eliminarlo y sacarlo de la lista de archivos actuales
					pthread_mutex_lock(&mutex_archivos_actuales);
					if(unico_bloque){
						crear_y_actualizar_archivo(elemento,numero_bloque_enviado, numero_bloque_datanode,tamanio_bloque, numero_copia, nombre_nodo,nombre_archivo);
						unico_bloque=false;
					}
					crear_y_actualizar_archivo(elemento,numero_bloque_enviado, numero_bloque_datanode,tamanio_bloque, numero_copia, nombre_nodo,nombre_archivo);
					eliminar_archivo(nombre_nodo, nombre_archivo);
					pthread_mutex_unlock(&mutex_archivos_actuales);
				} else {
					if (resultado == 1 && !dictionary_has_key(archivos_erroneos,nombre_archivo)) {
						//escribir y crear directorio y archivo
						pthread_mutex_lock(&mutex_archivos_actuales);
						elemento->total_bloques -= 1;
						crear_y_actualizar_archivo(elemento,numero_bloque_enviado, numero_bloque_datanode,tamanio_bloque, numero_copia, nombre_nodo,nombre_archivo);
						pthread_mutex_unlock(&mutex_archivos_actuales);
					} else {
						pthread_mutex_lock(&mutex_archivos_actuales);
						elemento->total_bloques = -1;
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
				if(formateado){
					close(socketFD);
					rechazado=true;
				}

				/*bool comparar(void*nombre){
						return !(strcmp(datos,nombre));
					}
				if(list_find(datanodes_formateados,(void*) comparar)==NULL){
					printf("Se rechazo la conexion, nunca se conecto este nodo");
					fflush(stdout);
					close(socketFD);
				}*/
				}
				break;
			}
		} else if (!strcmp(paquete.header.emisor, YAMA)) {
			socketYAMA = socketFD;
			if(inseguro){
				break;
			}else{
				switch (paquete.header.tipoMensaje) {
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
						strcat(ruta_archivo_en_metadata,integer_to_string(index_padre));
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
					}
					break;
				}
			}
		} else if (!strcmp(paquete.header.emisor, WORKER)) {
			if(inseguro){
				close(socketFD);
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
	if(!rechazado && !inseguro){
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
				&& (aux[i].index > index_directorio)) {
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
	char **separado_por_barras = string_split(ruta, "/");
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
			char **separado_por_barras = string_split(ruta_fs, "/");
			int cantidad = 0;
			while (separado_por_barras[cantidad]) {
				cantidad++;
			}
			if (cantidad == 1) {
				//ruta con un solo directorio
				return existe(separado_por_barras[cantidad - 1], -1, aux);
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
	}
}
bool existe_archivo(char *nombre_archivo, int index_directorio_padre) {
	char*ruta = malloc(200);
	strcpy(ruta, PUNTO_MONTAJE);
	strcat(ruta, "/archivos/");
	strcat(ruta, integer_to_string(index_directorio_padre));
	strcat(ruta, "/");
	strcat(ruta, nombre_archivo);
	ruta = realloc(ruta, strlen(ruta) + 1);
	if (access(ruta, F_OK) != -1) {
		free(ruta);
		return true;
	}
	free(ruta);
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
int obtener_socket(char*nombre) {
	bool nombre_igual(info_datanode*datanode) {
		if (strcmp(datanode->nodo, nombre) == 0) {
			return true;
		}
		return false;
	}
	return ((info_datanode*) list_find(datanodes, (void*) nombre_igual))->socket;
}
void solicitar_bloques(char*nombre_archivo, int index_padre) {
	//TODO SOLICITAR_BLOQUES
	char*ruta = malloc(100);
	strcpy(ruta, RUTA_ARCHIVOS);
	strcat(ruta, "/");
	strcat(ruta, integer_to_string(index_padre));
	strcat(ruta, "/");
	strcat(ruta, nombre_archivo);
	t_config*archivo = config_create(ruta);
	t_dictionary*datanodes_dictionary = dictionary_create();
	int cantidad = 0;
	while (1) {
		char*bloque_bytes = calloc(1, 50);
		strcpy(bloque_bytes, "BLOQUE");
		strcat(bloque_bytes, integer_to_string(cantidad));
		strcat(bloque_bytes, "BYTES");
		bloque_bytes = realloc(bloque_bytes, strlen(bloque_bytes) + 1);
		if (config_has_property(archivo, bloque_bytes)) {
			cantidad++;
		} else {
			break;
		}
		free(bloque_bytes);
	}
	int aux = cantidad;
	int i = 0;
	while (aux > 0) {
		pthread_mutex_lock(&mutex_md5);
		char*bloque_bytes = calloc(1, 50);
		strcpy(bloque_bytes, "BLOQUE");
		strcat(bloque_bytes, integer_to_string(i));
		strcat(bloque_bytes, "BYTES");
		bloque_bytes = realloc(bloque_bytes, strlen(bloque_bytes) + 1);
		char*primer_bloque = calloc(1, 50);
		strcpy(primer_bloque, "BLOQUE");
		strcat(primer_bloque, integer_to_string(i));
		strcat(primer_bloque, "COPIA0");
		primer_bloque = realloc(primer_bloque, strlen(primer_bloque) + 1);
		char*segundo_bloque = calloc(1, 50);
		strcpy(segundo_bloque, "BLOQUE");
		strcat(segundo_bloque, integer_to_string(i));
		strcat(segundo_bloque, "COPIA1");
		segundo_bloque = realloc(segundo_bloque, strlen(segundo_bloque) + 1);
		t_solicitud_bloque*bloque = malloc(sizeof(t_solicitud_bloque));
		if (config_has_property(archivo, primer_bloque)
				&& config_has_property(archivo, segundo_bloque)) {
			char**datos_primer_bloque = config_get_array_value(archivo,
					primer_bloque);
			if (cantidad == 0) {
				dictionary_put(datanodes_dictionary, datos_primer_bloque[0], 1);
				bloque->bloque_nodo = atoi(datos_primer_bloque[1]);
				bloque->nombre_nodo = malloc(20);
				strcpy(bloque->nombre_nodo, datos_primer_bloque[0]);
				bloque->nombre_nodo = realloc(bloque->nombre_nodo,
						strlen(bloque->nombre_nodo) + 1);
			} else {
				char**datos_segundo_bloque = config_get_array_value(archivo,
						segundo_bloque);
				if (dictionary_has_key(datanodes_dictionary,
						datos_primer_bloque[0])
						&& dictionary_has_key(datanodes_dictionary,
								datos_segundo_bloque[0])) {
					if (dictionary_get(datanodes_dictionary,
							datos_primer_bloque[0])
							<= dictionary_get(datanodes_dictionary,
									datos_segundo_bloque[0])) {
						dictionary_put(datanodes_dictionary,
								datos_primer_bloque[0],
								dictionary_get(datanodes_dictionary,
										datos_primer_bloque[0]) + 1);
						bloque->bloque_nodo = atoi(datos_primer_bloque[1]);
						bloque->nombre_nodo = malloc(20);
						strcpy(bloque->nombre_nodo, datos_primer_bloque[0]);
						bloque->nombre_nodo = realloc(bloque->nombre_nodo,
								strlen(bloque->nombre_nodo) + 1);
					} else {
						dictionary_put(datanodes_dictionary,
								datos_segundo_bloque[0],
								dictionary_get(datanodes_dictionary,
										datos_segundo_bloque[0]) + 1);
						bloque->bloque_nodo = atoi(datos_segundo_bloque[1]);
						bloque->nombre_nodo = malloc(20);
						strcpy(bloque->nombre_nodo, datos_segundo_bloque[0]);
						bloque->nombre_nodo = realloc(bloque->nombre_nodo,
								strlen(bloque->nombre_nodo) + 1);
					}
				} else {
					//alguno de los dos nunca se seteo
					if (dictionary_has_key(datanodes_dictionary,datos_primer_bloque[0])) {
						dictionary_put(datanodes_dictionary,datos_segundo_bloque,
								dictionary_get(datanodes_dictionary,datos_segundo_bloque) + 1);
						bloque->bloque_nodo = atoi(datos_segundo_bloque[1]);
						bloque->nombre_nodo = malloc(20);
						strcpy(bloque->nombre_nodo, datos_segundo_bloque[0]);
						bloque->nombre_nodo = realloc(bloque->nombre_nodo,
								strlen(bloque->nombre_nodo) + 1);
					} else {
						dictionary_put(datanodes_dictionary,
								datos_primer_bloque,
								dictionary_get(datanodes_dictionary,
										datos_primer_bloque) + 1);
						bloque->bloque_nodo = atoi(datos_primer_bloque[1]);
						bloque->nombre_nodo = malloc(20);
						strcpy(bloque->nombre_nodo, datos_primer_bloque[0]);
						bloque->nombre_nodo = realloc(bloque->nombre_nodo,
								strlen(bloque->nombre_nodo) + 1);
					}
				}
			}
		} else {
			if (config_has_property(archivo, primer_bloque)) {
				char**datos_primer_bloque = config_get_array_value(archivo,
						primer_bloque);
				dictionary_put(datanodes_dictionary, datos_primer_bloque,
						dictionary_get(datanodes_dictionary,
								datos_primer_bloque) + 1);
				bloque->bloque_nodo = atoi(datos_primer_bloque[1]);
				bloque->nombre_nodo = malloc(20);
				strcpy(bloque->nombre_nodo, datos_primer_bloque[0]);
				bloque->nombre_nodo = realloc(bloque->nombre_nodo,
						strlen(bloque->nombre_nodo) + 1);
			} else {
				char**datos_segundo_bloque = config_get_array_value(archivo,
						segundo_bloque);
				dictionary_put(datanodes_dictionary, datos_segundo_bloque,
						dictionary_get(datanodes_dictionary,
								datos_segundo_bloque) + 1);
				bloque->bloque_nodo = atoi(datos_segundo_bloque[1]);
				bloque->nombre_nodo = malloc(20);
				strcpy(bloque->nombre_nodo, datos_segundo_bloque[0]);
				bloque->nombre_nodo = realloc(bloque->nombre_nodo,
						strlen(bloque->nombre_nodo) + 1);
			}
		}
		bloque->bloque_archivo = i;
		bloque->bloques_totales = cantidad;
		bloque->index_directorio = index_padre;
		bloque->tamanio_bloque = config_get_int_value(archivo, bloque_bytes);
		bloque->nombre_archivo = malloc(70);
		strcpy(bloque->nombre_archivo, nombre_archivo);
		bloque->nombre_archivo = realloc(bloque->nombre_archivo,
				strlen(nombre_archivo) + 1);
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
		i++;
		aux--;
		cantidad--;
	}
	free(ruta);
	config_destroy(archivo);
	dictionary_destroy(datanodes_dictionary);

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
	config_set_value(nodos,libre_nodo,integer_to_string(tamanio_nodo));
	config_save(nodos);
	if(primera_vez){
		config_set_value(nodos,"LIBRE",integer_to_string(tamanio_nodo));
	}else{
		int tamanio_anterior=config_get_int_value(nodos,"LIBRE");
		tamanio_anterior+=tamanio_nodo;
		config_set_value(nodos,"LIBRE",integer_to_string(tamanio_anterior));
	}
	config_save(nodos);
	config_save_in_file(nodos,RUTA_NODOS);
	free(total_nodo);
	free(libre_nodo);
	free(nombre_nodo);
	config_destroy(nodos);
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
		}
	}
}
void igualar(info_datanode*elemento){
	//printf(elemento->bloques_libres);
	memmove(elemento->bloques_libres,elemento->bloques_totales,sizeof(int));
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
			formateado=true;
		}
		else if (!strncmp(linea, "rm ", 3)) {
			linea += 3;
			if (!strncmp(linea, "-d", 2)) {

			} else if (!strncmp(linea, "-b", 2))
				printf("remover bloque\n");
			else{
				linea -= 3;
				char **array_input=string_split(linea," ");
				if(!array_input[0] || !array_input[1] ){
				}else{
					char *nombre_archivo=malloc(100);
					strcpy(nombre_archivo,obtener_nombre_archivo(array_input[1]));
					nombre_archivo=realloc(nombre_archivo,strlen(nombre_archivo)+1);
					int index_directorio_padre=index_ultimo_directorio(array_input[1],"a");
					char *ruta_archivo_yamafs=malloc(100);
					strcpy(ruta_archivo_yamafs,RUTA_ARCHIVOS);
					strcat(ruta_archivo_yamafs,"/");
					strcat(ruta_archivo_yamafs,integer_to_string(index_directorio_padre));
					strcat(ruta_archivo_yamafs,"/");
					strcat(ruta_archivo_yamafs,nombre_archivo);
					ruta_archivo_yamafs=realloc(ruta_archivo_yamafs,strlen(ruta_archivo_yamafs)+1);
					remove(ruta_archivo_yamafs);
					char *directorio=malloc(100);
					strcpy(directorio,RUTA_ARCHIVOS);
					strcat(directorio,"/");
					strcat(directorio,integer_to_string(index_directorio_padre));
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
				}
			}
		}else if (!strncmp(linea, "rename ", 7)) {
			char **array_input = string_split(linea, " ");
			//TODO rename
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
					strcat(ruta_vieja, integer_to_string(padre_anterior));
					strcat(ruta_vieja, "/");
					strcat(ruta_vieja, separado_por_barras[cantidad]);
					ruta_vieja = realloc(ruta_vieja, strlen(ruta_vieja) + 1);
					char *ruta_nueva = malloc(100);
					strcpy(ruta_nueva, RUTA_ARCHIVOS);
					strcat(ruta_nueva, "/");
					strcat(ruta_nueva, integer_to_string(padre_anterior));
					strcat(ruta_nueva, "/");
					strcat(ruta_nueva, array_input[2]);
					ruta_nueva = realloc(ruta_nueva, strlen(ruta_nueva) + 1);
					rename(ruta_vieja, ruta_nueva);
					free(ruta_vieja);
					free(ruta_nueva);
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
				printf("Error, verificar parámetros \n");
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
					int index_directorio_padre_original=index_ultimo_directorio(ruta_original,"a");
					int index_directorio_padre_destino=index_ultimo_directorio(ruta_destino,"d");
					strcpy(nombre_archivo,obtener_nombre_archivo(ruta_original));
					nombre_archivo=realloc(nombre_archivo,strlen(nombre_archivo)+1);
					if(!existe_archivo(nombre_archivo,index_directorio_padre_original)){
						printf("Error,no existe el archivo original \n");
					}else{
						strcpy(ruta_archivo_original,RUTA_ARCHIVOS);
						strcat(ruta_archivo_original,"/");
						strcat(ruta_archivo_original,integer_to_string(index_directorio_padre_original));
						strcat(ruta_archivo_original,"/");
						strcat(ruta_archivo_original,nombre_archivo);
						ruta_archivo_original=realloc(ruta_archivo_original,strlen(ruta_archivo_original)+1);
						t_config*archivo_original=config_create(ruta_archivo_original);
						strcpy(directorio_destino,RUTA_ARCHIVOS);
						strcat(directorio_destino,"/");
						strcat(directorio_destino,integer_to_string(index_directorio_padre_destino));
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
						strcat(directorio_destino,integer_to_string(index_directorio_padre_destino));
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
						strcat(directorio_original,integer_to_string(index_directorio_padre_original));
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
		else if (!strncmp(linea, "cat ", 4))
			printf("muestra archivo\n");
		else if (!strncmp(linea, "mkdir ", 6)) {
			printf("crea directorio\n");
			char **array_input = string_split(linea, " ");
			//TODO
			if (!array_input[0] || !array_input[1]) {
				printf("Error, verificar parametros\n");
				fflush(stdout);
			}
			if (existe_ruta(array_input[1])) {
				printf("El directorio ya existe!\n");
				fflush(stdout);
			} else {
				printf("El directorio no existe, se creará\n");
				fflush(stdout);
				crear_y_actualizar_ocupados(array_input[1]);
				printf("Directorio creado correctamente\n");
				fflush(stdout);
			}
		} else if (!strncmp(linea, "cpfrom ", 7)) {
			/*
			 * cpfrom	[path_archivo_origen]	[directorio_yamafs]	 [tipo_archivo]	-	Copiar	un	archivo	local	al	yamafs,
			 siguiendo	los	lineamientos	en	la	operaciòn	Almacenar	Archivo,	de	la	Interfaz	del
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
				printf("Error, verifique parámetros\n");
				fflush(stdout);
			}
			char *path_archivo = malloc(strlen(array_input[1]) + 1);
			strcpy(path_archivo, array_input[1]);
			char *nombre_archivo = malloc(100);
			nombre_archivo = obtener_nombre_archivo(path_archivo);
			char *directorio_yama = malloc(strlen(array_input[2]) + 1);
			strcpy(directorio_yama, array_input[2]);
			if (!existe_ruta(directorio_yama)) {
				printf("El directorio no existe, se creará\n");
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
						void *datos_bloque = calloc(1, i + 1);
						memcpy(datos_bloque, data, i + 1);
						bloque *bloque_add = calloc(1, sizeof(bloque));
						bloque_add->datos = datos_bloque;
						bloque_add->tamanio = i + 1;
						list_add(bloques_a_enviar, bloque_add);
						data += i + 1;
						size_aux -= i + 1;
					} else {
						i--;
					}
				}
				if (condicion) {
					void *datos_bloque = calloc(1, size_aux);
					memcpy(datos_bloque, data, size_aux);
					bloque *bloque_add = calloc(1, sizeof(bloque));
					bloque_add->datos = datos_bloque;
					bloque_add->tamanio = size_aux;
					list_add(bloques_a_enviar, bloque_add);
				}
			} else {
				//archivo binario
				if (size_aux > TAMBLOQUE) {
					//tamaño archivo mayor al de un bloque
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
					//tamaño archivo menor a un bloque
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
					md5=false;
					ruta_a_guardar=malloc(100);
					strcpy(ruta_a_guardar,array_input[2]);
					ruta_a_guardar=realloc(ruta_a_guardar,strlen(ruta_a_guardar)+1);
					solicitar_bloques(separado_por_barras[i], index_padre);
					printf("Archivo creado satisfactoriamente \n");
				}
			}
		}
		else if (!strncmp(linea, "cpblock ", 8))
			printf("copia de bloque\n");
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
					pthread_mutex_lock(&mutex_md5);
					//trabate con el semaforo
					se_trabo=true;
					pthread_mutex_lock(&mutex_md5);
					pthread_mutex_unlock(&mutex_md5);
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
							"El directorio no existe,por favor ingrese un directorio válido \n");
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
					strcat(ruta, integer_to_string(ultimo_index));
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
					strcat(ruta_archivo,integer_to_string(index_directorio_padre));
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
					while(condicion){
						primera_copia=malloc(50);
						segunda_copia=malloc(50);
						tamanio_bloque=malloc(50);
						strcpy(primera_copia,"BLOQUE");
						strcpy(segunda_copia,"BLOQUE");
						strcpy(tamanio_bloque,"BLOQUE");
						strcat(primera_copia,integer_to_string(i));
						strcat(segunda_copia,integer_to_string(i));
						strcat(tamanio_bloque,integer_to_string(i));
						strcat(primera_copia,"COPIA0");
						strcat(segunda_copia,"COPIA1");
						strcat(tamanio_bloque,"BYTES");
						primera_copia=realloc(primera_copia,strlen(primera_copia)+1);
						segunda_copia=realloc(segunda_copia,strlen(segunda_copia)+1);
						tamanio_bloque=realloc(tamanio_bloque,strlen(tamanio_bloque)+1);
						if(config_has_property(archivo,tamanio_bloque)){
							printf("Bloque %s tamanio %s bytes \n",integer_to_string(i),config_get_string_value(archivo,tamanio_bloque));
							if(config_has_property(archivo,primera_copia) && config_has_property(archivo,segunda_copia)){
								char**array_primera_copia=config_get_array_value(archivo,primera_copia);
								printf("Bloque %s, copia 0 esta en %s en el bloque %s\n",integer_to_string(i),array_primera_copia[0],array_primera_copia[1]);
								char**array_segunda_copia=config_get_array_value(archivo,segunda_copia);
								printf("Bloque %s, copia 1 esta en %s en el bloque %s\n",integer_to_string(i),array_segunda_copia[0],array_segunda_copia[1]);
							}else{
								if(config_has_property(archivo,primera_copia)){
									char**array_primera_copia=config_get_array_value(archivo,primera_copia);
									printf("Bloque %s, copia 0 esta en %s en el bloque %s\n",integer_to_string(i),array_primera_copia[0],array_primera_copia[1]);
								}else{
									char**array_segunda_copia=config_get_array_value(archivo,segunda_copia);
									printf("Bloque %s, copia 1 esta en %s en el bloque %s\n",integer_to_string(i),array_segunda_copia[0],array_segunda_copia[1]);
								}
							}
						}else{
							condicion=false;
						}
						i++;
						free(primera_copia);
						free(segunda_copia);
						free(tamanio_bloque);
					}
					free(ruta_archivo);
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
				if (un_datanode->cantidad
						>= ceilDivision(bloques_a_enviar * 2, var)) {
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
	if (!dictionary_has_key(archivos_actuales, nombre_nodo)) {
		//primera vez que se manda un archivo a este nodo
		t_list*lista_archivos = list_create();
		t_archivo_actual *un_archivo = malloc(sizeof(t_archivo_actual));
		un_archivo->index_padre = index_directorio_padre;
		un_archivo->total_bloques = 1;
		un_archivo->tipo = tipo_archivo;
		un_archivo->nombre_archivo = malloc(100);
		strcpy(un_archivo->nombre_archivo, nombre_archivo);
		un_archivo->nombre_archivo = realloc(un_archivo->nombre_archivo,
				strlen(un_archivo->nombre_archivo) + 1);
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
			t_archivo_actual*un_archivo = obtener_elemento(archivos,
					nombre_archivo);
			un_archivo->total_bloques += 1;
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
t_resultado_envio* enviar_bloque(t_list *disponibles, t_list*bloques_a_enviar,
		int i_bloques_a_enviar, int index_copia, char *nombre_archivo,
		int index_directorio_padre, int tipo_archivo, int numero_copia) {
	char *nombre = malloc(100);
	strcpy(nombre,
			((t_datanode_a_enviar*) list_get(disponibles, index_copia))->nombre);
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
			int size_bitarray =
					total % 8 > 0 ? ((int) (total / 8)) + 1 : (int) (total / 8);
			t_bitarray *bitarray = bitarray_create_with_mode(bmap,
					size_bitarray, MSB_FIRST);
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

			//creamos tipo de dato a enviar y lo inicializamos
			bloque *primer_bloque = malloc(sizeof(bloque));
			primer_bloque = list_get(bloques_a_enviar, i_bloques_a_enviar);
			primer_bloque->numero = numero_bloque;
			primer_bloque->copia = numero_copia;
			primer_bloque->nombre_archivo = malloc(100);
			strcpy(primer_bloque->nombre_archivo, nombre_archivo);
			primer_bloque->nombre_archivo = realloc(
					primer_bloque->nombre_archivo,
					strlen(primer_bloque->nombre_archivo) + 1);

			//enviamos al datanode el bloque y el tamaño de bloque
			int tamanio = sizeof(uint32_t) * 4 + primer_bloque->tamanio
					+ sizeof(char) * strlen(primer_bloque->nombre_archivo) + 1;
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
			actualizar_archivos_actuales(nombre, index_directorio_padre,
					tipo_archivo, nombre_archivo);
			//enviamos datos
			int socket = ((t_datanode_a_enviar*) list_get(disponibles,
					index_copia))->socket;
			t_resultado_envio*resultado = malloc(sizeof(t_resultado_envio));
			resultado->bloque = numero_bloque;
			resultado->nombre_nodo = malloc(100);
			strcpy(resultado->nombre_nodo, nombre);
			resultado->nombre_nodo = realloc(resultado->nombre_nodo,
					strlen(resultado->nombre_nodo) + 1);
			resultado->resultado = EnviarDatosTipo(socket, FILESYSTEM, datos,tamanio, SETBLOQUE);
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
int enviarBloques(t_list *bloques_a_enviar, char *nombre_archivo,
		int index_directorio_padre, int tipo_archivo) {
	//filtramos los que tengan bloques libres
	//hay que filtrar en la lista de datanodes los que tengan bloques libres>0
	//una vez obtenidos,los enviamos
	//TODO ----> ENVIAR BLOQUES

	t_list *datanodes_a_enviar = list_create();
	t_list *disponibles = list_create();
	t_list *bloques_enviados = list_create();
	pthread_mutex_lock(&mutex_datanodes);
	datanodes_a_enviar = list_map(datanodes, (void*) datanodes_disponibles);
	pthread_mutex_unlock(&mutex_datanodes);
	int cantidad_datanodes_a_enviar;
	bool error = false;
	if (!(cantidad_datanodes_a_enviar = se_puede_enviar(datanodes_a_enviar,
			list_size(bloques_a_enviar)))) {
		printf("No hay lugar disponbile\n");
		fflush(stdout);
		error = true;

	} else {
		//datanodes que tengo que enviar
		disponibles = obtener_disponibles(datanodes_a_enviar,cantidad_datanodes_a_enviar, list_size(bloques_a_enviar) * 2);
	}
	int size_bloques = list_size(bloques_a_enviar);
	int i_lista_disponibles = 0;
	int i_bloques_a_enviar = 0;
	if (!error) {
		while (size_bloques) {
			//obtenemos datos del  datanode para enviarle la primer copia del bloque
			//primer copia es i_lista_disponibles
			int index_primer_copia = i_lista_disponibles + 1 > (list_size(disponibles) - 1) ? 0 : i_lista_disponibles + 1;
			int index_segunda_copia =index_primer_copia + 1 > (list_size(disponibles) - 1) ? 0 : index_primer_copia + 1;
			t_resultado_envio*resultado_primer_copia;
			t_resultado_envio*resultado_segunda_copia;
			pthread_mutex_lock(&mutex_cpfrom);
			resultado_primer_copia = enviar_bloque(disponibles,bloques_a_enviar, i_bloques_a_enviar, index_primer_copia,nombre_archivo, index_directorio_padre, tipo_archivo, 0);
			pthread_mutex_lock(&mutex_cpfrom);
			resultado_segunda_copia = enviar_bloque(disponibles,bloques_a_enviar, i_bloques_a_enviar, index_segunda_copia,nombre_archivo, index_directorio_padre, tipo_archivo, 1);
			if (!(resultado_primer_copia->resultado)|| !(resultado_segunda_copia->resultado)) {
				//limpio todos los bloques enviados
				limpiar_bitarrays(bloques_enviados);
				//TODO HACER SEMAFOROS
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
				list_add(bloques_enviados, primer_copia);
				t_bloque_enviado*segunda_copia = malloc(sizeof(t_bloque_enviado));
				segunda_copia->bloque = resultado_segunda_copia->bloque;
				segunda_copia->nombre_nodo = malloc(100);
				strcpy(segunda_copia->nombre_nodo,resultado_segunda_copia->nombre_nodo);
				segunda_copia->nombre_nodo = realloc(segunda_copia->nombre_nodo,strlen(segunda_copia->nombre_nodo) + 1);
				list_add(bloques_enviados, segunda_copia);
			}
			i_lista_disponibles =	i_lista_disponibles + 1 > (list_size(disponibles) - 1) ?0 : i_lista_disponibles + 1;
			size_bloques--;
			i_bloques_a_enviar++;
			if (size_bloques <= 0) {
				dictionary_put(archivos_terminados, nombre_archivo,nombre_archivo);
			}
		}
		//pthread_mutex_unlock(&mutex_cpfrom);
		//pthread_mutex_unlock(&mutex_cpfrom);
	}
	list_destroy(datanodes_a_enviar);
	list_destroy(disponibles);
	list_destroy(bloques_enviados);
	list_destroy_and_destroy_elements(bloques_a_enviar, free);
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

int main() {
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
	RUTA_ARCHIVOS_ALMACENADOS=malloc(strlen(PUNTO_MONTAJE) + strlen("/archivos.bin") + 1);
	strcpy(RUTA_ARCHIVOS_ALMACENADOS, PUNTO_MONTAJE);
	strcat(RUTA_ARCHIVOS_ALMACENADOS, "/archivos.bin");
	//TODO ACLARACION -----> CODIGO PARA PROBAR LOS DIRECTORIOS PERSISTIDOS

	 /*t_directory **directorios=obtener_directorios();
	 t_directory *aux=(*directorios);
	 int var;
	 for (var = 0; var < 10; ++var) {
	 printf("%i,%i,%s\n",aux[var].index,aux[var].padre,aux[var].nombre);
	 }
	 return 0;
	*/
	if(access(RUTA_NODOS,F_OK)==-1){
		FILE*f=fopen(RUTA_NODOS,"w");
		fclose(f);
		t_config*archivo=config_create(RUTA_NODOS);
		config_set_value(archivo,"TAMANIO",integer_to_string(0));
		config_set_value(archivo,"LIBRE",integer_to_string(0));
		config_save_in_file(archivo,RUTA_NODOS);
	}
	datanodes = list_create();
	archivos_actuales = dictionary_create();
	archivos_terminados = dictionary_create();
	archivos_erroneos = dictionary_create();
	pthread_t hiloConsola;
	datanodes_anteriores=list_create();
	//verificar_estado
	pthread_mutex_init(&mutex_datanodes, NULL);
	pthread_mutex_init(&mutex_directorio, NULL);
	pthread_mutex_init(&mutex_directorios_ocupados, NULL);
	pthread_mutex_init(&mutex_archivos_actuales, NULL);
	pthread_mutex_init(&mutex_archivos_erroneos, NULL);
	pthread_mutex_init(&mutex_escribir_archivo, NULL);
	pthread_mutex_init(&mutex_md5, NULL);
	pthread_mutex_init(&mutex_cpfrom,NULL);
	pthread_mutex_init(&mutex_respuesta_solicitud,NULL);
	//pthread_mutex_init(&mutex_archivos_almacenados,NULL);
	pthread_create(&hiloConsola, NULL, (void*) consola, NULL);
	ServidorConcurrente(IP, PUERTO, FILESYSTEM, &listaHilos, &end, accion);
	pthread_join(hiloConsola, NULL);
	return 0;
}
