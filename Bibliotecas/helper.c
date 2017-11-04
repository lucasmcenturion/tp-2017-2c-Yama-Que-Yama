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

void actualizar_nodos_bin(info_datanode *data){
	t_config *nodos=config_create("/home/utnso/metadata/nodos.bin");
	int tamanio_actual;
	tamanio_actual=config_get_int_value(nodos,"TAMANIO");
	tamanio_actual = tamanio_actual == 0 ? data->bloques_totales : tamanio_actual+data->bloques_totales;
	int nDigits = floor(log10(abs(tamanio_actual))) + 1;
	char *string_tamanio_actual=malloc(nDigits*sizeof(char));
	sprintf(string_tamanio_actual,"%i",tamanio_actual);
	config_set_value(nodos,"TAMANIO",string_tamanio_actual);
	config_save_in_file(nodos,"/home/utnso/metadata/nodos.bin");
	int libre;
	libre=config_get_int_value(nodos,"LIBRE");
	libre= libre==0 ? data->bloques_libres : libre+data->bloques_libres;
	nDigits = floor(log10(abs(libre))) + 1;
	char *string_libre=malloc(nDigits*sizeof(char));
	sprintf(string_libre,"%i",libre);
	config_set_value(nodos,"LIBRE",string_libre);
	config_save_in_file(nodos,"/home/utnso/metadata/nodos.bin");
	char *nodos_actuales=config_get_string_value(nodos,"NODOS");
	if(nodos_actuales){
		nodos_actuales=config_get_string_value(nodos,"NODOS");
		char **separado_por_comas=string_split(nodos_actuales,",");
		int cantidad_comas=0;
		while(separado_por_comas[cantidad_comas]){
			cantidad_comas++;
		}
		int iterate=0;
		char *nuevos_nodos=string_new();
		while(iterate<cantidad_comas){
			if(cantidad_comas==1){
				char *substring=string_substring(separado_por_comas[(cantidad_comas-1)],0,strlen(separado_por_comas[(cantidad_comas-1)])-1);
				string_append(&nuevos_nodos,substring);
				string_append(&nuevos_nodos,",");
				string_append(&nuevos_nodos,data->nodo);
				string_append(&nuevos_nodos,"]");
			}else{
				if(iterate==0){
					string_append(&nuevos_nodos,separado_por_comas[iterate]);
					string_append(&nuevos_nodos,",");
					}else{
						if(iterate==(cantidad_comas-1)){
							char *substring=string_substring(separado_por_comas[(cantidad_comas-1)],0,strlen(separado_por_comas[(cantidad_comas-1)])-1);
							string_append(&nuevos_nodos,substring);
							string_append(&nuevos_nodos,",");
							string_append(&nuevos_nodos,data->nodo);
							string_append(&nuevos_nodos,"]");
						}else{
							string_append(&nuevos_nodos,separado_por_comas[iterate]);
							string_append(&nuevos_nodos,",");
					}
				}
			}
			iterate++;
		}
		config_set_value(nodos,"NODOS",nuevos_nodos);
		config_save_in_file(nodos,"/home/utnso/metadata/nodos.bin");
	}else{
		char* nodo_nuevo=string_new();
		string_append(&nodo_nuevo,"[");
		string_append(&nodo_nuevo,data->nodo);
		string_append(&nodo_nuevo,"]");
		config_set_value(nodos,"NODOS",nodo_nuevo);
		config_save_in_file(nodos,"/home/utnso/metadata/nodos.bin");
	}
	char *nodo_actual_total=string_new();
	string_append(&nodo_actual_total,data->nodo);
	string_append(&nodo_actual_total,"Total");
	nDigits = floor(log10(abs(data->bloques_totales))) + 1;
	char *string_bloques_totales=malloc(nDigits*sizeof(char));
	sprintf(string_bloques_totales,"%i",data->bloques_totales);
	config_set_value(nodos,nodo_actual_total,string_bloques_totales);
	config_save_in_file(nodos,"/home/utnso/metadata/nodos.bin");
	char *nodo_actual_libre=string_new();
	string_append(&nodo_actual_libre,data->nodo);
	string_append(&nodo_actual_libre,"Libre");
	nDigits = floor(log10(abs(data->bloques_libres))) + 1;
	char *string_bloques_libres=malloc(nDigits*sizeof(char));
	sprintf(string_bloques_libres,"%i",data->bloques_libres);
	config_set_value(nodos,nodo_actual_libre,string_bloques_libres);
	config_save_in_file(nodos,"/home/utnso/metadata/nodos.bin");
}


void eliminar_ea_nodos(char*nombre){
	t_config *nodos=config_create("/home/utnso/metadata/nodos.bin");
	char *nodos_actuales=config_get_string_value(nodos,"NODOS");
	char **separado_por_comas=string_split(nodos_actuales,",");
	int cantidad_comas=0;
	int i=0;
	while(separado_por_comas[cantidad_comas]){
		cantidad_comas++;
	}
	char *nuevos_nodos=string_new();
	if(cantidad_comas==1){
		//tiene un solo elemento
		char *substring=string_substring(separado_por_comas[0],1,strlen(separado_por_comas[0])-2);
		if(strcmp(nombre,substring)==0){
			config_set_value(nodos,"NODOS","");
		}
	}else{
		//tiene mas de un elemento
		while(i<cantidad_comas){
			char * substring;
			if(i==0){
				substring=string_substring(separado_por_comas[i],1,strlen(separado_por_comas[i])-1);
				if(strcmp(nombre,substring)==0){
					string_append(&nuevos_nodos,"[");
				}else{
					string_append(&nuevos_nodos,separado_por_comas[i]);
				}
			}else{
				if(i==(cantidad_comas-1)){
					substring=string_substring(separado_por_comas[i],0,strlen(separado_por_comas[i])-1);
					if(strcmp(nombre,substring)==0){
						string_append(&nuevos_nodos,"]");
					}else{
						string_append(&nuevos_nodos,separado_por_comas[i]);
					}

				}else{
					substring=separado_por_comas[i];
					if(strcmp(nombre,substring)==0){
						string_append(&nuevos_nodos,",");
					}else{
						string_append(&nuevos_nodos,",");
						string_append(&nuevos_nodos,separado_por_comas[i]);
					}
				}
			}
			i++;
		}
	}
	config_set_value(nodos,"NODOS",nuevos_nodos);
	char *libre=string_new();
	string_append(&libre,nombre);
	string_append(&libre,"Libre");
	if(config_has_property(nodos,libre)){
		int libre_nodo_a_eliminar=config_get_int_value(nodos,libre);
		int libre_total=config_get_int_value(nodos,"LIBRE");
		int libre_total_a_actualizar= libre_total>libre_nodo_a_eliminar ? libre_total-libre_nodo_a_eliminar : 0;
		int nDigits = floor(log10(abs(libre_total_a_actualizar))) + 1;
		char *string_libre_total_a_actualizar=malloc(nDigits*sizeof(char));
		sprintf(string_libre_total_a_actualizar,"%i",libre_total_a_actualizar);
		config_set_value(nodos,"LIBRE",string_libre_total_a_actualizar);
		config_set_value(nodos,libre,"0");
		config_save_in_file(nodos,"/home/utnso/metadata/nodos.bin");
	}
	char *total=string_new();
	string_append(&total,nombre);
	string_append(&total,"Total");
	if(config_has_property(nodos,total)){
		int total_nodo_a_eliminar=config_get_int_value(nodos,total);
		int total_total=config_get_int_value(nodos,"TAMANIO");
		int total_total_a_actualizar= total_total>total_nodo_a_eliminar ? total_total-total_nodo_a_eliminar : 0;
		int nDigits = floor(log10(abs(total_total_a_actualizar))) + 1;
		char *string_total_total_a_actualizar=malloc(nDigits*sizeof(char));
		sprintf(string_total_total_a_actualizar,"%i",total_total_a_actualizar);
		config_set_value(nodos,total,"0");
		config_set_value(nodos,"TAMANIO",string_total_total_a_actualizar);
	}
	config_save_in_file(nodos,"/home/utnso/metadata/nodos.bin");

}
