#ifndef VEHICLE_H
#define VEHICLE_H

#include <pthread.h>
#include "config.h"


#define TIPO_CARRO      0
#define TIPO_AMBULANCIA 1


typedef struct {
    int    id;              
    int    direccion;       
    int    tipo;            
    double velocidad;       
    int    espacio_entrada; 
} DatosVehiculo;


void *hilo_vehiculo(void *arg);

double tiempo_exponencial(double media);

double valor_uniforme(double minimo, double maximo);

#endif 

typedef struct {
    Config  cfg;
    int     direccion;
    void   *(*funcion_hilo)(void *); 
} ArgGenerador;

void *hilo_generador_vehiculos(void *arg);


void vehiculo_resetear_contador(void);
