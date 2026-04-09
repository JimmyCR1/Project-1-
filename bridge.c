#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bridge.h"

Puente puente_global;

void puente_inicializar(const Config *cfg) {
    puente_global.num_espacios        = cfg->num_slots;
    puente_global.direccion_actual    = DIR_OE;
    puente_global.carros_en_puente    = 0;
    puente_global.carros_esperando_oe = 0;
    puente_global.carros_esperando_eo = 0;
    puente_global.total_cruzados      = 0;

    puente_global.espacios = calloc(cfg->num_slots, sizeof(EspacioPuente));
    if (!puente_global.espacios) { perror("puente_inicializar"); exit(1); }

    puente_global.contenido_espacio = calloc(cfg->num_slots, sizeof(int));
    if (!puente_global.contenido_espacio) { perror("puente_inicializar contenido"); exit(1); }

    for (int i = 0; i < cfg->num_slots; i++) {
        sem_init(&puente_global.espacios[i].semaforo, 0, 1);
        puente_global.contenido_espacio[i] = ESPACIO_LIBRE;
    }

    pthread_mutex_init(&puente_global.mutex_estado, NULL);
}

void puente_destruir(void) {
    for (int i = 0; i < puente_global.num_espacios; i++)
        sem_destroy(&puente_global.espacios[i].semaforo);
    free(puente_global.espacios);
    free(puente_global.contenido_espacio);
    pthread_mutex_destroy(&puente_global.mutex_estado);
}

void puente_entrar(int direccion) {
    pthread_mutex_lock(&puente_global.mutex_estado);
    puente_global.direccion_actual = direccion;
    puente_global.carros_en_puente++;
    pthread_mutex_unlock(&puente_global.mutex_estado);
}

void puente_salir(int direccion) {
    pthread_mutex_lock(&puente_global.mutex_estado);
    puente_global.carros_en_puente--;
    puente_global.total_cruzados++;
    pthread_mutex_unlock(&puente_global.mutex_estado);
    (void)direccion;
    if (cond_salida_puente && mutex_salida_puente) {
        pthread_mutex_lock(mutex_salida_puente);
        pthread_cond_broadcast(cond_salida_puente);
        pthread_mutex_unlock(mutex_salida_puente);
    }
}

void puente_ocupar_espacio(int indice, int tipo_vehiculo) {
    pthread_mutex_lock(&puente_global.mutex_estado);
    if (indice >= 0 && indice < puente_global.num_espacios)
        puente_global.contenido_espacio[indice] = tipo_vehiculo;
    pthread_mutex_unlock(&puente_global.mutex_estado);
}

void puente_liberar_espacio(int indice) {
    pthread_mutex_lock(&puente_global.mutex_estado);
    if (indice >= 0 && indice < puente_global.num_espacios)
        puente_global.contenido_espacio[indice] = ESPACIO_LIBRE;
    pthread_mutex_unlock(&puente_global.mutex_estado);
}

void puente_imprimir_estado(void) {
    pthread_mutex_lock(&puente_global.mutex_estado);
    printf("[BRIDGE] slots=%d on_bridge=%d dir=%s waiting(W->E)=%d (E->W)=%d crossed=%ld\n",
           puente_global.num_espacios,
           puente_global.carros_en_puente,
           puente_global.direccion_actual == DIR_OE ? "W->E" : "E->W",
           puente_global.carros_esperando_oe,
           puente_global.carros_esperando_eo,
           puente_global.total_cruzados);
    pthread_mutex_unlock(&puente_global.mutex_estado);
}

pthread_cond_t  *cond_salida_puente  = NULL;
pthread_mutex_t *mutex_salida_puente = NULL;

void puente_registrar_condicion(pthread_mutex_t *mx, pthread_cond_t *cv) {
    mutex_salida_puente = mx;
    cond_salida_puente  = cv;
}
