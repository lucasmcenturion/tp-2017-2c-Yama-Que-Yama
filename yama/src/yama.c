#include "sockets.h"

char *IP, *FS_IP, *ALGORITMO_BALANCEO;
int PUERTO, FS_PUERTO, RETARDO_PLANIFICACION;
int socketFS;

void obtenerValoresArchivoConfiguracion() {
	t_config* arch = config_create("yamaCFG.txt");
	IP = string_duplicate(config_get_string_value(arch, "IP"));
	PUERTO = config_get_int_value(arch, "PUERTO");
	FS_IP = string_duplicate(config_get_string_value(arch, "FS_IP"));
	FS_PUERTO = config_get_int_value(arch, "FS_PUERTO");
	RETARDO_PLANIFICACION = config_get_int_value(arch, "RETARDO_PLANIFICACION");
	ALGORITMO_BALANCEO = string_duplicate(config_get_string_value(arch, "ALGORITMO_BALANCEO"));
	config_destroy(arch);
}

void accion(){

}

void imprimirArchivoConfiguracion(){
	printf(
				"Configuración:\n"
				"IP=%s\n"
				"PUERTO=%d\n"
				"FS_IP=%s\n"
				"FS_PUERTO=%d\n"
				"RETARDO_PLANIFICACION=%d\n"
				"ALGORITMO_BALANCEO=%s\n",
				IP,
				PUERTO,
				FS_IP,
				FS_PUERTO,
				RETARDO_PLANIFICACION,
				ALGORITMO_BALANCEO
				);
}

int main(){
	obtenerValoresArchivoConfiguracion();
	imprimirArchivoConfiguracion();
	Servidor(IP, PUERTO, YAMA, accion, RecibirPaqueteCliente);
	socketFS = ConectarAServidor(FS_PUERTO, FS_IP, FILESYSTEM, YAMA, RecibirHandshake);

	return 0;
} 
