#include "helper.h"


char* integer_to_string(char*string,int x) {
	string = malloc(10);
	if (string) {
		sprintf(string, "%d", x);
	}
	string=realloc(string,strlen(string)+1);
	return string; // caller is expected to invoke free() on this buffer to release memory
}

size_t getFileSize(const char* filename) {
    struct stat st;
    if(stat(filename, &st) != 0) {
        return 0;
    }
    return st.st_size;
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

char *obtener_nombre_archivo(char*path){
	char **separado_por_barras=string_split(path,"/");
	int cantidad=0;
	while(separado_por_barras[cantidad]){
		cantidad++;
	}
	return separado_por_barras[cantidad-1];
}
