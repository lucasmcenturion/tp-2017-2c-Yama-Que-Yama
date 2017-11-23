#include "sockets.h"
#include <sys/mman.h>
#define TAMBLOQUE (1024*1024)
char *IP;
char *PUNTO_MONTAJE;
char *RUTA_DIRECTORIOS;
char *RUTA_NODOS;
int PUERTO;
int socketFS;
int socketYAMA;
t_list* listaHilos;
t_list* datanodes;
bool directorios_ocupados[100];
t_dictionary *archivos_actuales;
int index_directorio=1;
bool end;
pthread_mutex_t mutex_datanodes;
pthread_mutex_t mutex_directorio;
pthread_mutex_t mutex_directorios_ocupados;

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
	t_list *aux=list_filter(datanodes,(void*) tiene_socket);
	datanodes=aux;
	pthread_mutex_unlock(&mutex_datanodes);
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
					data->nodo = string_new();
					strcpy(data->nodo, datos);
					datos-=sizeof(uint32_t);
					data->socket=socketFD;
					char *path=string_new();
					string_append(&path,"/home/utnso/metadata/bitmaps/");
					string_append(&path,data->nodo);
					string_append(&path,".dat");
					printf("%s\n",path);
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
						}
						bmap = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, bitmap, 0);
						if (bmap == MAP_FAILED) {
									printf("Error al mapear a memoria: %s\n", strerror(errno));
						}
						t_bitarray *bitarray = bitarray_create_with_mode(bmap, size_bitarray, MSB_FIRST);
						//cuando se lo crea, se inicializa todos con 0, es decir, todos los bloques libres
						munmap(bmap,mystat.st_size);
						close(bitmap);
						bitarray_destroy(bitarray);
						data->bloques_libres=(int)(size_databin/TAMBLOQUE);
						data->bloques_totales=(int)(size_databin/TAMBLOQUE);
						actualizar_nodos_bin(data);
					}else{
						//ya existe un bitmap, alguna vez este nodo se conecto
						char *path=string_new();
						string_append(&path,"/home/utnso/metadata/bitmaps/");
						string_append(&path,data->nodo);
						string_append(&path,".dat");
						printf("%s\n",path);
						int bitmap = open(path,O_RDWR);
						struct stat mystat;
						void *bmap;
						if (fstat(bitmap, &mystat) < 0) {
							printf("Error al establecer fstat\n");
							close(bitmap);
						}
						bmap = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, bitmap, 0);
						if (bmap == MAP_FAILED) {
									printf("Error al mapear a memoria: %s\n", strerror(errno));
						}
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
						actualizar_nodos_bin(data);
					}
					pthread_mutex_lock(&mutex_datanodes);
					list_add(datanodes,data);
					pthread_mutex_unlock(&mutex_datanodes);
				}
				break;
				case RESULOPERACION:
				{
					void *datos=paquete.Payload;
					uint32_t numeroDeBloque;
					uint32_t resultado;
					*((uint32_t*)datos)=numeroDeBloque;
					datos+=sizeof(uint32_t);
					*((uint32_t*)datos)=resultado; //1 resultado exitoso -1 resulado erroneo
					char *nombre_nodo=string_new();
					strcpy(nombre_nodo,datos);
					if(resultado==-1){
						printf("Error en datanode de %s, no se pudo guardar el bloque %i\n",nombre_nodo,numeroDeBloque);
					}else{
						char*ruta=string_new();
						string_append(&ruta,"/home/utnso/metadata/bitmaps/");
						strcat(ruta,nombre_nodo);
						string_append(&ruta,".bin");
						if(access(ruta, F_OK ) != -1){
							int bitmap = open(ruta,O_RDWR);
							struct stat mystat;
							void *bmap;
							t_config *nodos=config_create("/home/utnso/metadata/nodos.bin");
							int total;
							char *nombre_total=string_new();
							string_append(&nombre_total,nombre_nodo);
							string_append(&nombre_total,"Total");
							total=config_has_property(nodos,nombre_total);
							int size_bitarray=total%8>0 ? total+1 : total;
							if (fstat(bitmap, &mystat) < 0) {
								printf("Error al establecer fstat\n");
								close(bitmap);
							}
							bmap = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, bitmap, 0);
							if (bmap == MAP_FAILED) {
										printf("Error al mapear a memoria: %s\n", strerror(errno));
							}
							t_bitarray *bitarray = bitarray_create_with_mode(bmap, size_bitarray, MSB_FIRST);
							bitarray_set_bit(bitarray,numeroDeBloque);
						}else{
							printf("No existe el bitmap del %s\n",nombre_nodo);
						}
						//poner bitarray como ese bloque ocupado
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
	int padre_anterior=-1;
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
				int index_padre_anterior=-1;
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
void consola() {
	char * linea;
	while(true) {
		linea = readline(">> ");
		if(linea)
		  add_history(linea);
		if(!strcmp(linea, "format"))
			printf("logica de format\n");
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
						int padre_anterior=-1;
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
			char *nombre_archivo=obtener_nombre_archivo(path_archivo);
			char *directorio_yama=malloc(strlen(array_input[2])+1);
			strcpy(directorio_yama,array_input[2]);
			if(!existe_ruta(directorio_yama)){
				printf("El directorio no existe, se creará");
				fflush(stdout);
				crear_y_actualizar_ocupados(directorio_yama);
			}
			t_directory **directorios=obtener_directorios();
			t_directory *aux=(*directorios);
			char **separado_por_barras=string_split(directorio_yama,"/");
			int iterador=0;
			int padre_anterior=-1;
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
			enviarBloques((t_list*)bloques_a_enviar,nombre_archivo,index_directorio_padre);
			//TODO tambien destroy elements;
			list_destroy(bloques_a_enviar);
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
				int padre_anterior=-1;
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
		}
	}
}
bool esta_disponible(info_datanode *un_datanode){
	t_config *nodos=config_create("/home/utnso/metadata/nodos.bin");
	int libre;
	char *nombre_libre=calloc(1,strlen(un_datanode->nodo)+strlen("Libre")+1);
	string_append(&nombre_libre,un_datanode->nodo);
	string_append(&nombre_libre,"Libre");
	libre=config_has_property(nodos,nombre_libre);
	free(nombre_libre);
	return libre>0;
}

int primer_disponible(t_list *datanodes, int index_actual){
	//esta funcion retorna el index de la lista
	pthread_mutex_lock(&mutex_datanodes);
	int total=list_size(datanodes);
	pthread_mutex_unlock(&mutex_datanodes);
	int proximo;
	bool disponible=false;
	int aux;
	pthread_mutex_lock(&mutex_datanodes);
	int disponibilidad=esta_disponible(list_get(datanodes,index_actual));
	pthread_mutex_unlock(&mutex_datanodes);
	if(disponibilidad && index_actual<=(total-1)){
		return index_actual;
	}
	if((index_actual+1)==(total-1)){
		//proximo=0;
		proximo=total-1;
	}else{
		proximo=index_actual+1;
	}
	proximo=aux;
	while(1){
		pthread_mutex_lock(&mutex_datanodes);
		disponibilidad=esta_disponible(list_get(datanodes,proximo));
		pthread_mutex_unlock(&mutex_datanodes);
		if(disponibilidad){
			//existe un nodo que tiene bloques libres para guardar
			disponible=true;
			break;
		}
		proximo++;
		if(proximo==aux){
			//realizo un ciclo y no hay disponible,debe romper el ciclo
			break;
		}
		if(proximo==total-1){
			proximo=0;
		}
	}
	if(disponible){
		if(proximo==index_actual){
			//existe un nodo con bloques libres,pero es el mismo que el actual(tienen = index)
			return -1;
		}else{
			//existeu un nodo con bloques libres, diferente al actual(tienen != index)
			return proximo;
		}
	}else{
		return -1;
	}
}

void enviarBloques(t_list *bloques_a_enviar){
	//filtramos los que tengan bloques libres
	//hay que filtrar en la lista de datanodes los que tengan bloques libres>0
	//una vez obtenidos,los enviamos
	pthread_mutex_lock(&mutex_datanodes);
	t_list *disponibles=list_filter(datanodes, (void*) esta_disponible);
	pthread_mutex_unlock(&mutex_datanodes);
	int size_bloques=list_size(bloques_a_enviar);
	pthread_mutex_lock(&mutex_datanodes);
	int datanodes_disponibles=list_size(disponibles);
	pthread_mutex_unlock(&mutex_datanodes);
	int i=0;
	int i_bloques=0;
	if(datanodes_disponibles<2){
		perror("No hay datanodes disponibles");
	}else{
		//hay por lo menos 2 datanodes disponibles
		while(size_bloques){
			int index_primer_copia= primer_disponible(disponibles,i);
			int index_segunda_copia= primer_disponible(disponibles,i+1);
			if(index_primer_copia>=0 && index_segunda_copia>=0 && index_primer_copia!=index_segunda_copia){
				//obtenemos datos del primer datanode para enviarle el bloque
				char *ruta=string_new();
				string_append(&ruta,"/home/utnso/metadata/bitmaps/");
				pthread_mutex_lock(&mutex_datanodes);
				char *nombre=((info_datanode*)list_get(datanodes,index_primer_copia))->nodo;
				pthread_mutex_unlock(&mutex_datanodes);
				//string_append(&ruta,&nombre);
				ruta=realloc(ruta,strlen(ruta)+strlen(nombre)+1);
				strcat(ruta, nombre);
				ruta=realloc(ruta,strlen(ruta)+strlen(".dat")+1);
				string_append(&ruta,".dat");
				t_config *nodos=config_create("/home/utnso/metadata/nodos.bin");
				int total;
				char *nombre_total=string_new();
				string_append(&nombre_total,nombre);
				string_append(&nombre_total,"Total");
				total=config_get_int_value(nodos,nombre_total);
				int size_bitarray=total%8>0 ? ((int)(total/8))+1 : (int)(total/8);
				if(access(ruta, F_OK ) != -1){
					int bitmap = open(ruta,O_RDWR);
					struct stat mystat;
					void *bmap;
					if (fstat(bitmap, &mystat) < 0) {
						printf("Error al establecer fstat\n");
						close(bitmap);
					}
					bmap = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, bitmap, 0);
					if (bmap == MAP_FAILED) {
								printf("Error al mapear a memoria: %s\n", strerror(errno));
					}
					t_bitarray *bitarray = bitarray_create_with_mode(bmap, size_bitarray, MSB_FIRST);
					int j;
					for (j=0; j < total; j++) {
						if(bitarray_test_bit(bitarray,j)==0){
							break;
						}
					}
					int numero_bloque=j;
					bloque *primer_bloque=malloc(sizeof(bloque));
					primer_bloque=list_get(bloques_a_enviar,i_bloques);
					primer_bloque->numero=numero_bloque;
					//enviamos al datanode el bloque y el tamaño de bloque
					int tamanio = sizeof(uint32_t) * 2  + primer_bloque->tamanio;
					void *datos = malloc(tamanio);
					*((uint32_t*)datos) = primer_bloque->numero;
					datos += sizeof(uint32_t);
					*((uint32_t*)datos) = primer_bloque->tamanio;
					datos += sizeof(uint32_t);
					memcpy(datos,primer_bloque->datos,primer_bloque->tamanio);
					datos += primer_bloque->tamanio;
					datos -= tamanio;
					//aca envias
					pthread_mutex_lock(&mutex_datanodes);
					int socket=((info_datanode*)list_get(datanodes,index_primer_copia))->socket;
					pthread_mutex_unlock(&mutex_datanodes);
					t_dictionary *info_archivo=dictionary_create();

					/*EnviarDatosTipo(socket, FILESYSTEM, datos, tamanio, SETBLOQUE);
					free(datos);
					*/

				}else{
					printf("No existe el bitmap de %s\n",nombre);
				}

				//enviar datos a list_get(bloques,primer_copia)
				//enviar datos a list_get(bloques,segunda_copia)
			}else{
				if(index_primer_copia!=index_segunda_copia){
					perror("Se intenta copiar en el mismo datanodes dos copias");
				}else{
					perror("No hay mas lugares para guardar alguna de sus copias");
				}
			}
			if(i+1==datanodes_disponibles-1){
				i=0;
			}else{
				i++;
			}
			size_bloques--;
			i_bloques++;
		}
		list_destroy(disponibles);
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
	RUTA_NODOS=malloc(strlen(PUNTO_MONTAJE)+strlen("/nodos.bin"));
	strcpy(RUTA_NODOS,PUNTO_MONTAJE);
	strcat(RUTA_NODOS,"/nodos.bin");
	if(access(RUTA_DIRECTORIOS, F_OK ) == -1 ){
		FILE *f=fopen(RUTA_DIRECTORIOS,"w");
		fclose(f);
		crear_directorio();
		setear_directorio(0,0,"root",-1);
	}
	datanodes=list_create();
	archivos_actuales=dictionary_create();
	pthread_t hiloConsola;
	pthread_mutex_init(&mutex_datanodes,NULL);
	pthread_mutex_init(&mutex_directorio,NULL);
	pthread_mutex_init(&mutex_directorios_ocupados,NULL);
	pthread_create(&hiloConsola, NULL, (void*)consola,NULL);
	ServidorConcurrente(IP, PUERTO, FILESYSTEM, &listaHilos, &end, accion);
	pthread_join(hiloConsola, NULL);
	return 0;
}
