#include "sockets.h"
#include <sys/mman.h>
#define TAMBLOQUE (1024*1024)
char *IP;
int PUERTO;
int socketFS;
int socketYAMA;
t_list* listaHilos;
t_list* datanodes;
bool end;

void obtenerValoresArchivoConfiguracion() {
	t_config* arch = config_create("filesystemCFG.txt");
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


bool no_tiene_socket(info_datanode *datanode,int *socket){
	return datanode->socket!=socket;
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
						data->bloques_libres=(int)(size_databin/TAMBLOQUE);
						data->bloques_totales=(int)(size_databin/TAMBLOQUE);
						// TODO	falta cambiar los datos del nodo
					}else{
						//ya existe un bitmap, alguna vez este nodo se conecto
						//TODO 	obtener del bitmap los bloques ocupados
					}
					list_add(datanodes,data);
				}
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
	//list_remove_by_condition(datanodes,(void*) no_tiene_socket,&socketFD);

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
		else if(!strncmp(linea, "mkdir ", 6))
			printf("crea directorio\n");
		else if(!strncmp(linea, "cpfrom ", 7)){
			/*
			 * cpfrom	[path_archivo_origen]	[directorio_yamafs]	 [tipo_archivo]	-	Copiar	un	archivo	local	al	yamafs,
				siguiendo	los	lineamientos	en	la	operaciòn	Almacenar	Archivo,	de	la	Interfaz	del
				FileSystem.
			 * */
			char **array_input=string_split(linea," ");
			char *path_archivo=malloc(sizeof(array_input[1]));
			path_archivo=array_input[1];
			char *directorio=malloc(sizeof(array_input[2]));
			directorio=array_input[2];
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
						void *bloque=calloc(1,i+1);
						memcpy(bloque,data,i+1);
						bloques *bloque_a_enviar=calloc(1,sizeof(bloques));
						bloque_a_enviar->datos=bloque;
						bloque_a_enviar->tamano=i+1;
						list_add(bloques_a_enviar,bloque_a_enviar);
						data+=i+1;
						size_aux-=i+1;
					}else{
						i--;
					}
				}
				if(condicion){
					void *bloque=calloc(1,size_aux);
					memcpy(bloque,data,size_aux);
					bloques *bloque_a_enviar=calloc(1,sizeof(bloques));
					bloque_a_enviar->datos=bloque;
					bloque_a_enviar->tamano=size_aux;
					list_add(bloques_a_enviar,bloque_a_enviar);
				}
			}else{
				//archivo binario
				if(size_aux>TAMBLOQUE){
					//tamaño archivo mayor al de un bloque
					while(size_aux>TAMBLOQUE){
						void *bloque=calloc(1,TAMBLOQUE);
						memcpy(bloque,data,TAMBLOQUE);
						bloques *bloque_a_enviar=calloc(1,sizeof(bloques));
						bloque_a_enviar->datos=bloque;
						bloque_a_enviar->tamano=TAMBLOQUE;
						list_add(bloques_a_enviar,bloque_a_enviar);
						size_aux-=TAMBLOQUE;
						data+=TAMBLOQUE;
					}
					if(!(size_aux>TAMBLOQUE)){
						void *bloque=calloc(1,size_aux);
						memcpy(bloque,data,size_aux);
						bloques *bloque_a_enviar=calloc(1,sizeof(bloques));
						bloque_a_enviar->datos=bloque;
						bloque_a_enviar->tamano=size_aux;
						list_add(bloques_a_enviar,bloque_a_enviar);
					}
				}else{
					//tamaño archivo menor a un bloque
					void *bloque=calloc(1,size_aux);
					memcpy(bloque,data,size_aux);
					bloques *bloque_a_enviar=calloc(1,sizeof(bloques));
					bloque_a_enviar->datos=bloque;
					bloque_a_enviar->tamano=size_aux;
					list_add(bloques_a_enviar,bloque_a_enviar);
				}
			}
			enviarBloques(bloques_a_enviar);
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
	return un_datanode->bloques_libres>0;
}

int primer_disponible(t_list *datanodes, int index_actual){
	//esta funcion retorna el index de la lista
	int total=list_size(datanodes);
	int proximo;
	bool disponible=false;
	if(((info_datanode*)list_get(datanodes,index_actual))->bloques_libres>0 && index_actual<total){
		return index_actual;
	}
	if(index_actual+1==total-1){
		//proximo=0;
		proximo=total-1;
	}else{
		proximo=index_actual+1;
	}
	while(1){
		if(((info_datanode*)list_get(datanodes,proximo))->bloques_libres>0){
			//existe un nodo que tiene bloques libres para guardar
			disponible=true;
			break;
		}
		proximo++;
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
	t_list *disponibles=list_filter(datanodes, (void*) esta_disponible);
	int size_bloques=list_size(bloques_a_enviar);
	int datanodes_disponibles=list_size(disponibles);
	int i=0;
	if(datanodes_disponibles<2){
		perror("No hay datanodes disponibles");
	}else{
		//hay por lo menos 2 datanodes disponibles
		while(size_bloques){
			int index_primer_copia= primer_disponible(disponibles,i);
			int index_segunda_copia= primer_disponible(disponibles,i+1);
			if(index_primer_copia>=0 && index_segunda_copia>=0 && index_primer_copia!=index_segunda_copia){
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
		}
		list_destroy(disponibles);
	}
}

int main(){
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();
	datanodes=list_create();
	pthread_t hiloConsola;
	pthread_create(&hiloConsola, NULL, (void*)consola,NULL);
	ServidorConcurrente(IP, PUERTO, FILESYSTEM, &listaHilos, &end, accion);
	pthread_join(hiloConsola, NULL);
	return 0;
}
