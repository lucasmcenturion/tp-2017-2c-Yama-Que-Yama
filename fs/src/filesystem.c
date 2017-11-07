#include "sockets.h"
#include <sys/mman.h>
#define TAMBLOQUE (1024*1024)
char *IP;
int PUERTO;
int socketFS;
int socketYAMA;
t_list* listaHilos;
t_list* datanodes;
int index_directorio=1;
bool end;
pthread_mutex_t mutex_datanodes;
pthread_mutex_t mutex_directorio;
pthread_mutex_t mutex_index;

void obtenerValoresArchivoConfiguracion() {
	t_config* arch = config_create("/home/utnso/workspace/tp-2017-2c-Yama-Que-Yama/fs/filesystemCFG.txt");
	IP = string_duplicate(config_get_string_value(arch, "IP"));
	PUERTO = config_get_int_value(arch, "PUERTO");
	config_destroy(arch);
}

void imprimirArchivoConfiguracion(){
	printf(
				"Configuración:\n"
				"IP=%s\n"
				"PUERTO=%d\n",
				IP,
				PUERTO
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
int existe_directorio(char *nombre,int index_padre){
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
			return 0;
		}
		directorios = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd_directorio, 0);
		if (directorios == MAP_FAILED) {
					printf("Error al mapear a memoria: %s\n", strerror(errno));
					return 0;
		}else{
			t_directory aux[100];
			memmove(aux,directorios,100*sizeof(t_directory));
			int i=0;
			for (i = 1; i < 100; ++i) {
				if(aux[i].padre==index_padre && (strcmp(aux[i].nombre,nombre)==0)){
					munmap(directorios,mystat.st_size);
					return 1;
				}
			}
			munmap(directorios,mystat.st_size);
			return 2;
		}
	}
}
bool existe_rutafs(char *ruta){
	//TODO
	char **separado_por_barras=calloc(1,strlen(ruta)+1);
	separado_por_barras=string_split(ruta,"/");
	int cantidad=0;
	while(separado_por_barras[cantidad]){
		cantidad++;
	}
	cantidad--;
	int i=0;
	bool condicion=true;
	while(i<=cantidad){
		if(cantidad==0){
			//ruta con un único directorio
			if(existe_directorio(separado_por_barras[i],-1)==1){
						//existe el directorio, por lo tanto existe la ruta
					return true;
			}else{
				//no existe, hay que crear este único directorio
				setear_directorio(primer_lugar_disponible(),index_directorio,separado_por_barras[i],-1);
				pthread_mutex_lock(&mutex_index);
				index_directorio++;
				pthread_mutex_unlock(&mutex_index);
			}
		}else{
			//ruta con más de un directorio
			int index_padre_anterior=existe_directorio(separado_por_barras[i],-1)==1 ? obtener_index(separado_por_barras[i],-1) : index_directorio ;
			if(i==0){
				//primer directorio de todos
				if(existe_directorio(separado_por_barras[i],-1)==1){
					//si existe este directorio con el index -1 como padre, entonces vamos bien
					//setear una variable en true para saber que vamos bien
					i++;
				}else{
					setear_directorio(primer_lugar_disponible(),index_directorio,separado_por_barras[i],-1);
					pthread_mutex_lock(&mutex_index);
					index_directorio++;
					pthread_mutex_unlock(&mutex_index);
					condicion=false;
					i++;
					//si ya no existe este directorio con el index -1, entonces tooodo lo demás no existe
					//setear una variable en false para saber que hay que crearlo a los siguientes, además de este
				}
			}
			if(condicion){
				if(existe_directorio(separado_por_barras[i],index_padre_anterior)==1){
					//si existe este directorio con el index del anterior como padre, entonces vamos bien
					//dejamos condicion con true
					index_padre_anterior=obtener_index(separado_por_barras[i],index_padre_anterior);
				}else{
					//si no existe este directorio, lo creamos y seteamos condicion en false para que a partir del proximo
					// se empiecen a crear
					setear_directorio(primer_lugar_disponible(),index_directorio,separado_por_barras[i],index_padre_anterior);
					index_padre_anterior=index_directorio;
					pthread_mutex_lock(&mutex_index);
					index_directorio++;
					pthread_mutex_unlock(&mutex_index);
					condicion=false;
				}
			}else{
				setear_directorio(primer_lugar_disponible(),index_directorio,separado_por_barras[i],index_padre_anterior);
				index_padre_anterior=index_directorio;
				pthread_mutex_lock(&mutex_index);
				index_directorio++;
				pthread_mutex_unlock(&mutex_index);
				//hay que crear todos los directorios restantes
			}
		}
	}
	if(condicion){
		return true;
	}else{
		return false;
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
			if(!strncmp(linea, "-d", 2))
				printf("remover directorio\n");
			else if(!strncmp(linea, "-b", 2))
				printf("remover bloque\n");
			else
				printf("remover archivo\n");
			linea -= 3;
		}
		else if(!strncmp(linea, "rename ", 7))
			printf("renombar\n");
		else if(!strncmp(linea, "mv ", 3))
			printf("mover\n");
		else if(!strncmp(linea, "cat ", 4))
			printf("muestra archivo\n");
		else if(!strncmp(linea, "mkdir ", 6)){
			printf("crea directorio\n");
			char **array_input=string_split(linea," ");
			if(!array_input[0] || array_input[2]){
				printf("Error, verificar parametros");
			}
			if(!existe_rutafs(array_input[1])){
				//crear directorio
			}else{
				printf("Existe lince");
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
			char *directorio_yama=malloc(strlen(array_input[2])+1);
			strcpy(directorio_yama,array_input[2]);
			if(existe_rutafs(directorio_yama)==true){
				printf("Existe el directorio :D\n");
			}else{
				printf("No existe el directorio, se lo creó");
			}
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
			enviarBloques((t_list*)bloques_a_enviar);
			//TODO tambien destroy elements;
			list_destroy(bloques_a_enviar);
		}
		else if(!strncmp(linea, "cpto ", 5))
			printf("copiando a\n");
		else if(!strncmp(linea, "cpblock ", 8))
			printf("copia de bloque\n");
		else if(!strncmp(linea, "md5 ", 4))
			printf("MD5\n");
		else if(!strncmp(linea, "ls ", 3))
			printf("lista de archivos\n");
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

bool esta_disponible(info_datanode *un_datanode){
	t_config *nodos=config_create("/home/utnso/metadata/nodos.bin");
	int libre;
	char *nombre_libre=string_new();
	string_append(&nombre_libre,un_datanode->nodo);
	string_append(&nombre_libre,"Libre");
	libre=config_has_property(nodos,nombre_libre);
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
				strcat(ruta, nombre);
				string_append(&ruta,".dat");
				t_config *nodos=config_create("/home/utnso/metadata/nodos.bin");
				int total;
				char *nombre_total=string_new();
				string_append(&nombre_total,nombre);
				string_append(&nombre_total,"Total");
				total=config_has_property(nodos,nombre_total);
				int size_bitarray=total%8>0 ? total+1 : total;
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
					EnviarDatosTipo(socket, FILESYSTEM, datos, tamanio, SETBLOQUE);
					free(datos);

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


int main(){
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();
	crear_directorio();
	setear_directorio(0,0,"root",-1);
	datanodes=list_create();
	pthread_t hiloConsola;
	pthread_mutex_init(&mutex_datanodes,NULL);
	pthread_mutex_init(&mutex_directorio,NULL);
	pthread_mutex_init(&mutex_index,NULL);
	pthread_create(&hiloConsola, NULL, (void*)consola,NULL);
	ServidorConcurrente(IP, PUERTO, FILESYSTEM, &listaHilos, &end, accion);
	pthread_join(hiloConsola, NULL);
	return 0;
}
