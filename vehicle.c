#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include "vehicle.h"
#include "bridge.h"

#define LONGITUD_CARRO 4.5

double tiempo_exponencial(double media) {
    double u;
    do { u = (double)rand() / ((double)RAND_MAX + 1.0); } while (u <= 0.0);
    return -media * log(1.0 - u);
}

double valor_uniforme(double minimo, double maximo) {
    double u = (double)rand() / ((double)RAND_MAX + 1.0);
    return minimo + u * (maximo - minimo);
}

static void dormir_segundos(double segundos) {
    if (segundos <= 0.0) return;
    struct timespec ts;
    ts.tv_sec  = (time_t)segundos;
    ts.tv_nsec = (long)((segundos - (double)ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
}

void *hilo_vehiculo(void *arg) {
    DatosVehiculo *vehiculo = (DatosVehiculo *)arg;

    int total_espacios = puente_global.num_espacios;
    int espacio_inicio, espacio_fin, incremento;

    if (vehiculo->direccion == DIR_OE) {
        espacio_inicio = 0; espacio_fin = total_espacios; incremento = 1;
    } else {
        espacio_inicio = total_espacios - 1; espacio_fin = -1; incremento = -1;
    }

    double tiempo_por_espacio = LONGITUD_CARRO / vehiculo->velocidad;

    int tipo_visual;
    if (vehiculo->tipo == TIPO_AMBULANCIA)
        tipo_visual = (vehiculo->direccion == DIR_OE) ? ESPACIO_AMBU_OE : ESPACIO_AMBU_EO;
    else
        tipo_visual = (vehiculo->direccion == DIR_OE) ? ESPACIO_CARRO_OE : ESPACIO_CARRO_EO;

    const char *texto_dir  = (vehiculo->direccion == DIR_OE) ? "W->E" : "E->W";
    const char *texto_tipo = (vehiculo->tipo == TIPO_AMBULANCIA) ? "AMBULANCE" : "Car";

    printf("[ENTER ] %s #%d (%s) entering bridge. speed=%.1f m/s slot_time=%.2fs\n",
           texto_tipo, vehiculo->id, texto_dir,
           vehiculo->velocidad, tiempo_por_espacio);



    int espacio_actual    = espacio_inicio;
    int espacio_siguiente = espacio_actual + incremento;

    printf("[BRIDGE] %s #%d acquiring slot %d\n", texto_tipo, vehiculo->id, espacio_actual);
    sem_wait(&puente_global.espacios[espacio_actual].semaforo);
    puente_ocupar_espacio(espacio_actual, tipo_visual);
    printf("[BRIDGE] %s #%d occupying slot %d\n", texto_tipo, vehiculo->id, espacio_actual);

    while (espacio_siguiente != espacio_fin) {
        printf("[BRIDGE] %s #%d waiting for slot %d\n",
               texto_tipo, vehiculo->id, espacio_siguiente);
        sem_wait(&puente_global.espacios[espacio_siguiente].semaforo);

        puente_liberar_espacio(espacio_actual);
        sem_post(&puente_global.espacios[espacio_actual].semaforo);
        puente_ocupar_espacio(espacio_siguiente, tipo_visual);

        printf("[BRIDGE] %s #%d jumped: slot %d -> slot %d (%.2fs)\n",
               texto_tipo, vehiculo->id,
               espacio_actual, espacio_siguiente, tiempo_por_espacio);
        dormir_segundos(tiempo_por_espacio);

        espacio_actual    = espacio_siguiente;
        espacio_siguiente = espacio_actual + incremento;
    }

    puente_liberar_espacio(espacio_actual);
    sem_post(&puente_global.espacios[espacio_actual].semaforo);
    puente_salir(vehiculo->direccion);

    printf("[EXIT  ] %s #%d (%s) crossed the bridge successfully.\n",
           texto_tipo, vehiculo->id, texto_dir);

    puente_imprimir_estado();
    free(vehiculo);
    return NULL;
}

static int contador_id_global = 0;

void vehiculo_resetear_contador(void) { contador_id_global = 0; }

void *hilo_generador_vehiculos(void *arg) {
    ArgGenerador *generador = (ArgGenerador *)arg;

    double media_llegada = generador->direccion == DIR_OE
                           ? generador->cfg.media_llegada_oeste
                           : generador->cfg.media_llegada_este;
    double vel_min       = generador->direccion == DIR_OE
                           ? generador->cfg.velocidad_min_oeste
                           : generador->cfg.velocidad_min_este;
    double vel_max       = generador->direccion == DIR_OE
                           ? generador->cfg.velocidad_max_oeste
                           : generador->cfg.velocidad_max_este;

    struct timespec inicio, ahora;
    clock_gettime(CLOCK_MONOTONIC, &inicio);

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &ahora);
        double transcurrido = (ahora.tv_sec  - inicio.tv_sec)
                            + (ahora.tv_nsec - inicio.tv_nsec) / 1e9;
        if (transcurrido >= generador->cfg.duracion_simulacion) break;

        double espera = tiempo_exponencial(media_llegada);
        clock_gettime(CLOCK_MONOTONIC, &ahora);
        transcurrido = (ahora.tv_sec  - inicio.tv_sec)
                     + (ahora.tv_nsec - inicio.tv_nsec) / 1e9;
        double restante = generador->cfg.duracion_simulacion - transcurrido;
        if (espera > restante) espera = restante;
        if (espera <= 0.0) break;

        struct timespec ts;
        ts.tv_sec  = (time_t)espera;
        ts.tv_nsec = (long)((espera - ts.tv_sec) * 1e9);
        nanosleep(&ts, NULL);

        clock_gettime(CLOCK_MONOTONIC, &ahora);
        transcurrido = (ahora.tv_sec  - inicio.tv_sec)
                     + (ahora.tv_nsec - inicio.tv_nsec) / 1e9;
        if (transcurrido >= generador->cfg.duracion_simulacion) break;

        DatosVehiculo *nuevo = malloc(sizeof(DatosVehiculo));
        nuevo->id            = __sync_fetch_and_add(&contador_id_global, 1);
        nuevo->direccion     = generador->direccion;
        nuevo->tipo          = ((double)rand() / RAND_MAX) < generador->cfg.porcentaje_ambulancias
                               ? TIPO_AMBULANCIA : TIPO_CARRO;
        nuevo->espacio_entrada = 0;

        if (nuevo->tipo == TIPO_AMBULANCIA)
            nuevo->velocidad = vel_max;
        else
            nuevo->velocidad = valor_uniforme(vel_min, vel_max);

     
        pthread_t hilo;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&hilo, &attr, generador->funcion_hilo, nuevo);
        pthread_attr_destroy(&attr);
    }

    free(generador);
    return NULL;
}
