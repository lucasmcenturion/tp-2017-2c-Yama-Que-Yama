#include "sockets.h"

char *IP_FILESYSTEM,*NOMBRE_NODO,*RUTA_DATABIN;
int PUERTO_FILESYSTEM, PUERTO_WORKER, PUERTO_DATANODE;
int socketFS;

void obtenerValoresArchivoConfiguracion() {
	t_config* arch = config_create("../nodoCFG.txt");
	IP_FILESYSTEM = string_duplicate(config_get_string_value(arch, "IP_FILESYSTEM"));
	PUERTO_FILESYSTEM = config_get_int_value(arch, "PUERTO_FILESYSTEM");
	NOMBRE_NODO = string_duplicate(config_get_string_value(arch, "NOMBRE_NODO"));
	PUERTO_WORKER = config_get_int_value(arch, "PUERTO_WORKER");
	PUERTO_DATANODE = config_get_int_value(arch, "PUERTO_DATANODE");
	RUTA_DATABIN = string_duplicate(config_get_string_value(arch, "RUTA_DATABIN"));
	config_destroy(arch);
}

void imprimirArchivoConfiguracion(){
	printf(
				"Configuración:\n"
				"IP_FILESYSTEM=%s\n"
				"PUERTO_FILESYSTEM=%d\n"
				"NOMBRE_NODO=%s\n"
				"PUERTO_WORKER=%d\n"
				"PUERTO_DATANODE=%d\n"
				"RUTA_DATABIN=%s\n",
				IP_FILESYSTEM,
				PUERTO_FILESYSTEM,
				NOMBRE_NODO,
				PUERTO_WORKER,
				PUERTO_DATANODE,
				RUTA_DATABIN
				);
}

int main(){
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();

	socketFS = ConectarAServidor(PUERTO_FILESYSTEM, IP_FILESYSTEM, FILESYSTEM, DATANODE, RecibirHandshake);

	return 0;
}
