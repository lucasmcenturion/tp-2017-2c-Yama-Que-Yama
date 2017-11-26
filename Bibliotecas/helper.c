#include "helper.h"


char* integer_to_string(int x) {
	char* buffer = malloc(sizeof(char) * sizeof(int) * 4 + 1);
	if (buffer) {
		sprintf(buffer, "%d", x);
	}
	return buffer; // caller is expected to invoke free() on this buffer to release memory
}

size_t getFileSize(const char* filename) {
    struct stat st;
    if(stat(filename, &st) != 0) {
        return 0;
    }
    return st.st_size;
}



void eliminar_ea_nodos(char*nombre){
	t_config *nodos=config_create("/home/utnso/metadata/nodos.bin");
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
	strcat(&libre,"Libre");
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

int primer_lugar_disponible(){
	char *ruta=calloc(1,strlen("/home/utnso/metadata/directorios.dat")+1);
	strcpy(ruta,"/home/utnso/metadata/directorios.dat");
	int fd_directorio = open(ruta,O_RDWR);
	if(fd_directorio==-1){
		printf("Error al intentar abrir el archivo");
	}
	struct stat mystat;
	void *directorios;
	if (fstat(fd_directorio, &mystat) < 0) {
		printf("Error al establecer fstat\n");
		close(fd_directorio);
	}
	directorios = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd_directorio, 0);
	if (directorios == MAP_FAILED) {
				printf("Error al mapear a memoria: %s\n", strerror(errno));
	}else{
		int i;
		for (i = 1; i < 100; ++i) {
			t_directory element=((t_directory*)directorios)[i];
			if(strcmp(element.nombre,"/0")==0){
				munmap(directorios,mystat.st_size);
				return i;
			}
		}
		munmap(directorios,mystat.st_size);
		return -1;
	}
}
int obtener_index(char *nombre,int index_padre){
	char *ruta=string_new();
	string_append(&ruta,"/home/utnso/metadata/directorios.dat");
	int fd_directorio = open(ruta,O_RDWR);
	if(fd_directorio==-1){
		printf("Error al intentar abrir el archivo");
	}
	struct stat mystat;
	void *directorios;
	if (fstat(fd_directorio, &mystat) < 0) {
		printf("Error al establecer fstat\n");
		close(fd_directorio);
	}
	directorios = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd_directorio, 0);
	if (directorios == MAP_FAILED) {
				printf("Error al mapear a memoria: %s\n", strerror(errno));
	}else{
		t_directory aux[100];
		memmove(aux,directorios,100*sizeof(t_directory));
		int i;
		for (i=1; i< 100; ++i) {
			if(strcmp(aux[i].nombre,nombre)==0 && aux[i].padre==index_padre){
				memmove(directorios,aux,100*sizeof(t_directory));
				munmap(directorios,mystat.st_size);
				return aux[i].index;
			}
		}
		memmove(directorios,aux,100*sizeof(t_directory));
		munmap(directorios,mystat.st_size);
		return -1;
	}
}
void mostrar_directorio(char * nombre){
	char *ruta=string_new();
	string_append(&ruta,"/home/utnso/metadata/directorios.dat");
	int fd_directorio = open(ruta,O_RDWR);
	if(fd_directorio==-1){
		printf("Error al intentar abrir el archivo");
	}
	struct stat mystat;
	void *directorios;
	if (fstat(fd_directorio, &mystat) < 0) {
		printf("Error al establecer fstat\n");
		close(fd_directorio);
	}
	directorios = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd_directorio, 0);
	if (directorios == MAP_FAILED) {
				printf("Error al mapear a memoria: %s\n", strerror(errno));
	}else{
		int i;
		for (i=0; i< 100; ++i) {
			t_directory element=((t_directory*)directorios)[i];
			if(strcmp(element.nombre,nombre)==0){
				printf("Index : %i\n",element.index);
				printf("Index Padre: %i\n",element.padre);
				printf("Nombre: %s\n",element.nombre);
				fflush(stdout);
			}
		}

	}
}
int existe_algun_directorio(char *nombre){
	char *ruta=calloc(1,strlen("/home/utnso/metadata/directorios.dat")+1);
	strcpy(ruta,"/home/utnso/metadata/directorios.dat");
	int fd_directorio = open(ruta,O_RDWR);
	printf("%i\n",fd_directorio);
	if(fd_directorio==-1){
		printf("Error al intentar abrir el archivo");
		return 0;
	}else{
		struct stat mystat;
		void *directorios;
		if (fstat(fd_directorio, &mystat) < 0) {
			printf("Error al establecer fstat\n");
			close(fd_directorio);
		}
		directorios = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd_directorio, 0);
		if (directorios == MAP_FAILED) {
					printf("Error al mapear a memoria: %s\n", strerror(errno));
		}else{
			t_directory aux[100];
			memmove(aux,directorios,100*sizeof(t_directory));
			int i=0;
			for (i = 1; i < 100; ++i) {
				if(strcmp(aux[i].nombre,nombre)==0){
					munmap(directorios,mystat.st_size);
					return 1;
				}
			}
			munmap(directorios,mystat.st_size);
			return 2;
		}
	}
}

char **obtener_nombre_archivo(char*path){
	char **separado_por_barras=string_split(path,"/");
	int cantidad=0;
	while(separado_por_barras[cantidad]){
		cantidad++;
	}
	return separado_por_barras[cantidad-1];
}
