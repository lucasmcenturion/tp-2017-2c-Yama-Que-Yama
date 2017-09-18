#include "sockets.h"

char *YAMA_IP;
int YAMA_PUERTO;
int socketYAMA;

void obtenerValoresArchivoConfiguracion() {
	t_config* arch = config_create("masterCFG.txt");
	YAMA_IP = string_duplicate(config_get_string_value(arch, "YAMA_IP"));
	YAMA_PUERTO = config_get_int_value(arch, "YAMA_PUERTO");
	config_destroy(arch);
}

void imprimirArchivoConfiguracion(){
	printf(
				"Configuración:\n"
				"YAMA_IP=%s\n"
				"YAMA_PUERTO=%d\n",
				YAMA_IP,
				YAMA_PUERTO
				);
}

int main(){
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();

	socketYAMA = ConectarAServidor(YAMA_PUERTO, YAMA_IP, YAMA, MASTER, RecibirHandshake);
	socketWorker = ConectarAServidor(YAMA_PUERTO, YAMA_IP, YAMA, MASTER, RecibirHandshake);
	
	return 0;
}
