#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "mode_semaphore.h"
#include "bridge.h"
#include "vehicle.h"

static pthread_mutex_t mutex_semaforo = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cond_semaforo  = PTHREAD_COND_INITIALIZER;

static int verde_oe = 1;
static int verde_eo = 0;
static int simulacion_activa = 1;
static Config config_global;

static int direccion_ambu_congelando = -1;
static int id_ambu_autorizada        = -1;

#define CAPACIDAD_COLA 4096
typedef struct { int id; int tipo; } EntradaCola;
static EntradaCola cola_oe[CAPACIDAD_COLA], cola_eo[CAPACIDAD_COLA];
static int cab_oe = 0, fin_oe = 0;
static int cab_eo = 0, fin_eo = 0;
static int tamanio_oe(void){ return (fin_oe - cab_oe + CAPACIDAD_COLA) % CAPACIDAD_COLA; }
static int tamanio_eo(void){ return (fin_eo - cab_eo + CAPACIDAD_COLA) % CAPACIDAD_COLA; }

void *hilo_puerta_semaforo(void *arg) {
    DatosVehiculo *vehiculo = (DatosVehiculo *)arg;

    pthread_mutex_lock(&mutex_semaforo);


    if (vehiculo->direccion == DIR_OE) {
        cola_oe[fin_oe % CAPACIDAD_COLA] = (EntradaCola){ vehiculo->id, vehiculo->tipo };
        fin_oe++;
        puente_global.carros_esperando_oe++;
    } else {
        cola_eo[fin_eo % CAPACIDAD_COLA] = (EntradaCola){ vehiculo->id, vehiculo->tipo };
        fin_eo++;
        puente_global.carros_esperando_eo++;
    }
    printf("[WAIT  ] %s #%d (%s) at traffic light.\n",
           vehiculo->tipo == TIPO_AMBULANCIA ? "AMBULANCE" : "Car",
           vehiculo->id, vehiculo->direccion == DIR_OE ? "W->E" : "E->W");
    pthread_cond_broadcast(&cond_semaforo);
  

    while (1) {
        int mi_verde  = (vehiculo->direccion == DIR_OE) ? verde_oe : verde_eo;
        int puente_vacio  = (puente_global.carros_en_puente == 0);
        int mismo_sentido = !puente_vacio && (puente_global.direccion_actual == vehiculo->direccion);
        int puente_ok     = puente_vacio || mismo_sentido;

       
        int hay_espacio   = (puente_global.carros_en_puente < puente_global.num_espacios);

        int en_la_puerta = (vehiculo->direccion == DIR_OE)
            ? (tamanio_oe() > 0 && cola_oe[cab_oe % CAPACIDAD_COLA].id == vehiculo->id)
            : (tamanio_eo() > 0 && cola_eo[cab_eo % CAPACIDAD_COLA].id == vehiculo->id);

        int soy_ambu_autorizada = (id_ambu_autorizada == vehiculo->id);
        int bloqueado_por_freeze = (direccion_ambu_congelando >= 0) && !soy_ambu_autorizada;

        if (soy_ambu_autorizada) {
            if (puente_vacio) break;
        } else if (en_la_puerta && !bloqueado_por_freeze && hay_espacio) {
            if (mi_verde) {
                if (puente_ok) break;
            } else if (vehiculo->tipo == TIPO_AMBULANCIA) {
                if (puente_ok) {
                    break;
                } else {
                    if (direccion_ambu_congelando < 0) {
                        direccion_ambu_congelando = vehiculo->direccion;
                        id_ambu_autorizada        = vehiculo->id;
                        printf("[LIGHT ] AMBULANCE #%d (%s) at gate on RED – freezing.\n",
                               vehiculo->id, vehiculo->direccion == DIR_OE ? "W->E" : "E->W");
                        pthread_cond_broadcast(&cond_semaforo);
                    }
                    if (puente_vacio) break;
                }
            }
        }
        pthread_cond_wait(&cond_semaforo, &mutex_semaforo);
    }

    if (id_ambu_autorizada == vehiculo->id) {
        direccion_ambu_congelando = -1;
        id_ambu_autorizada        = -1;
        printf("[LIGHT ] AMBULANCE #%d cleared freeze.\n", vehiculo->id);
    }

    if (vehiculo->direccion == DIR_OE) { cab_oe++; puente_global.carros_esperando_oe--; }
    else                               { cab_eo++; puente_global.carros_esperando_eo--; }

    printf("[GO    ] %s #%d (%s) entering bridge.\n",
           vehiculo->tipo == TIPO_AMBULANCIA ? "AMBULANCE" : "Car",
           vehiculo->id, vehiculo->direccion == DIR_OE ? "W->E" : "E->W");

   
    puente_entrar(vehiculo->direccion);

    pthread_cond_broadcast(&cond_semaforo);
    pthread_mutex_unlock(&mutex_semaforo);

    hilo_vehiculo(vehiculo);

    pthread_mutex_lock(&mutex_semaforo);
    pthread_cond_broadcast(&cond_semaforo);
    pthread_mutex_unlock(&mutex_semaforo);

    return NULL;
}

typedef struct { int direccion; } ArgSemaforo;

static void dormir_duracion(double segundos) {
    pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t  cv = PTHREAD_COND_INITIALIZER;
    struct timespec abs_time;
    clock_gettime(CLOCK_REALTIME, &abs_time);
    abs_time.tv_sec  += (time_t)segundos;
    abs_time.tv_nsec += (long)((segundos - (long)segundos) * 1e9);
    if (abs_time.tv_nsec >= 1000000000L) {
        abs_time.tv_sec++;
        abs_time.tv_nsec -= 1000000000L;
    }
    pthread_mutex_lock(&mx);
    pthread_cond_timedwait(&cv, &mx, &abs_time);
    pthread_mutex_unlock(&mx);
    pthread_mutex_destroy(&mx);
    pthread_cond_destroy(&cv);
}

static void *hilo_semaforo(void *arg) {
    ArgSemaforo *as = (ArgSemaforo *)arg;
    int mi_dir = as->direccion;
    free(as);

    if (mi_dir == DIR_EO)
        dormir_duracion(config_global.duracion_verde);

    while (simulacion_activa) {
        pthread_mutex_lock(&mutex_semaforo);
        if (mi_dir == DIR_OE) verde_oe = 1;
        else                   verde_eo = 1;
        printf("[LIGHT ] %s is now GREEN (%.0f s)\n",
               mi_dir == DIR_OE ? "W->E" : "E->W", config_global.duracion_verde);
        pthread_cond_broadcast(&cond_semaforo);
        pthread_mutex_unlock(&mutex_semaforo);

        dormir_duracion(config_global.duracion_verde);

        pthread_mutex_lock(&mutex_semaforo);
        if (mi_dir == DIR_OE) verde_oe = 0;
        else                   verde_eo = 0;
        printf("[LIGHT ] %s is now RED\n", mi_dir == DIR_OE ? "W->E" : "E->W");
        pthread_cond_broadcast(&cond_semaforo);
        pthread_mutex_unlock(&mutex_semaforo);

        dormir_duracion(config_global.duracion_verde);
    }
    return NULL;
}

void modo_semaforo_ejecutar(const Config *cfg) {
    config_global             = *cfg;
    verde_oe                  = 1;
    verde_eo                  = 0;
    simulacion_activa         = 1;
    direccion_ambu_congelando = -1;
    id_ambu_autorizada        = -1;
    cab_oe = fin_oe           = 0;
    cab_eo = fin_eo           = 0;

    puente_registrar_condicion(&mutex_semaforo, &cond_semaforo);
    vehiculo_resetear_contador();
    printf("\n=== MODE 2: TRAFFIC LIGHTS ===\n");
    printf("Green duration: %.0f s per side.  Bridge slots: %d.\n\n",
           cfg->duracion_verde, cfg->num_slots);

    pthread_t hilo_luz_oe, hilo_luz_eo;
    ArgSemaforo *luz_oe = malloc(sizeof(ArgSemaforo)); luz_oe->direccion = DIR_OE;
    ArgSemaforo *luz_eo = malloc(sizeof(ArgSemaforo)); luz_eo->direccion = DIR_EO;
    pthread_create(&hilo_luz_oe, NULL, hilo_semaforo, luz_oe);
    pthread_create(&hilo_luz_eo, NULL, hilo_semaforo, luz_eo);

    pthread_t hilo_gen_oe, hilo_gen_eo;
    ArgGenerador *gen_oe = malloc(sizeof(ArgGenerador));
    gen_oe->cfg = *cfg; gen_oe->direccion = DIR_OE; gen_oe->funcion_hilo = hilo_puerta_semaforo;
    ArgGenerador *gen_eo = malloc(sizeof(ArgGenerador));
    gen_eo->cfg = *cfg; gen_eo->direccion = DIR_EO; gen_eo->funcion_hilo = hilo_puerta_semaforo;
    pthread_create(&hilo_gen_oe, NULL, hilo_generador_vehiculos, gen_oe);
    pthread_create(&hilo_gen_eo, NULL, hilo_generador_vehiculos, gen_eo);

    pthread_join(hilo_gen_oe, NULL);
    pthread_join(hilo_gen_eo, NULL);

    simulacion_activa = 0;
    pthread_mutex_lock(&mutex_semaforo);
    pthread_cond_broadcast(&cond_semaforo);
    pthread_mutex_unlock(&mutex_semaforo);
    pthread_join(hilo_luz_oe, NULL);
    pthread_join(hilo_luz_eo, NULL);

    sleep(2);
    printf("\n=== SIMULATION ENDED ===\n");
    puente_imprimir_estado();
}
