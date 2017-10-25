#include "sockets.h"
#define TAM_BLOQUE 1048576;
char *IP;
int PUERTO;
int socketFS;
int socketYAMA;
t_list* listaHilos;
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

void accion(void* socket){
	int socketFD = *(int*)socket;
	Paquete paquete;
	while (RecibirPaqueteServidor(socketFD, FILESYSTEM, &paquete) > 0) {
		if(!strcmp(paquete.header.emisor, DATANODE))
		{
			switch (paquete.header.tipoMensaje)
			{
				case NUEVOWORKER:
					{
						void* datos = paquete.Payload;
						int tamanio = paquete.header.tamPayload;
						EnviarDatosTipo(socketYAMA, FILESYSTEM, datos, tamanio, NUEVOWORKER);
					}
					break;
				case ESDATOS:
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
			//char *cadena=calloc(255,sizeof(char));
			//char cadena[255];
			//fgets(cadena,255,stdin);
			//printf("%s",linea);
			char **array_input=string_split(linea," ");
			char *path_archivo=malloc(sizeof(array_input[1]));
			path_archivo=array_input[1];
			char *directorio=malloc(sizeof(array_input[2]));
			directorio=array_input[2];
			int tipo_archivo = atoi(array_input[3]);
			printf("%s,%s,%i\n",path_archivo,directorio,tipo_archivo);
			FILE* archivo = fopen("/home/utnso/Escritorio/test.dat", "r");
			t_list* bloques  = list_create();
			if (tipo_archivo == 0)
			{
				//texto

				    if(archivo == NULL)
				    {
				        printf("error, el archivo no existe");
				    }

				    fseek(archivo, 0, SEEK_END);
				    long int size = ftell(archivo);
				    rewind(archivo);

				    char* content = calloc(size + 1, 1);

				    fread(content,1,size,archivo);
				    char** array_oraciones = string_split(content, "\n");
				    char *buffer = string_new();
				    string_append(&buffer, array_oraciones[0]);
				    int size_array_oraciones=0;
				    while( array_oraciones[size_array_oraciones] ) {
				    	size_array_oraciones++;
				    }
				    int i=0;
				    while (i<(size_array_oraciones-1)){
				    	if ((string_length(buffer)+string_length(array_oraciones[i]))< 1048576){
				    		string_append(&buffer, array_oraciones[i]);
				    	}
				    	else
				    	{
				    		printf("%i\n",string_length(buffer));
				    		list_add(bloques,buffer);
				    		buffer=string_new();

				    	}
				    	i++;
				    }
				    list_add(bloques,buffer);
				    printf("%i\n",list_size(bloques));

			}
			else
			{
				/*if(archivo == NULL)
				{
					printf("error, el archivo no existe");
				}

				fseek(archivo, 0, SEEK_END);
				long int  size = ftell(archivo);
				rewind(archivo);
				int a=true;
				while(a){
					if(size<1048576){
						char* content = calloc(size + 1, 1);
						fread(content,1,size,archivo);
						list_add(bloques,content);
						a=false;
					}else{
						char *content=malloc(1048576);

					}
				}*/
			}

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

int main(){
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();
	pthread_t hiloConsola;
	pthread_create(&hiloConsola, NULL, (void*)consola,NULL);
	ServidorConcurrente(IP, PUERTO, FILESYSTEM, &listaHilos, &end, accion);
	pthread_join(hiloConsola, NULL);
	return 0;
}
