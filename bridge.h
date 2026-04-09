#ifndef BRIDGE_H
#define BRIDGE_H

#include <pthread.h>
#include <semaphore.h>
#include "config.h"


#define DIR_OE  0   
#define DIR_EO  1  


#define ESPACIO_LIBRE       0
#define ESPACIO_CARRO_OE    1   
#define ESPACIO_AMBU_OE     2   
#define ESPACIO_CARRO_EO    3   
#define ESPACIO_AMBU_EO     4   


typedef struct {
    sem_t semaforo;   
} EspacioPuente;


typedef struct {
    int            num_espacios;
    EspacioPuente *espacios;

    pthread_mutex_t mutex_estado;
    int  direccion_actual;
    int  carros_en_puente;
    int  carros_esperando_oe;
    int  carros_esperando_eo;
    long total_cruzados;

    int *contenido_espacio;  
} Puente;

extern Puente puente_global;

void puente_inicializar(const Config *cfg);
void puente_destruir(void);
void puente_entrar(int direccion);
void puente_salir(int direccion);
void puente_imprimir_estado(void);


void puente_ocupar_espacio(int indice, int tipo_vehiculo);
void puente_liberar_espacio(int indice);

#endif 

extern pthread_cond_t  *cond_salida_puente;
extern pthread_mutex_t *mutex_salida_puente;

void puente_registrar_condicion(pthread_mutex_t *mx, pthread_cond_t *cv);
