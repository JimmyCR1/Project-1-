#ifndef CONFIG_H
#define CONFIG_H

typedef struct {

    double longitud_puente;         
    int    num_slots;               

   
    double media_llegada_oeste;     
    double velocidad_min_oeste;     
    double velocidad_max_oeste;     

    
    double media_llegada_este;
    double velocidad_min_este;
    double velocidad_max_este;

    
    double duracion_verde;          

    
    int k1;                         
    int k2;                         

   
    double porcentaje_ambulancias;  
    double duracion_simulacion;     
} Config;

int  config_cargar(const char *nombre_archivo, Config *cfg);
void config_imprimir(const Config *cfg);

#endif 
