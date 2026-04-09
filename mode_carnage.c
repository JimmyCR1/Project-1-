#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include "mode_carnage.h"
#include "bridge.h"
#include "vehicle.h"

static pthread_mutex_t mutex_puerta = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cond_puerta  = PTHREAD_COND_INITIALIZER;

#define CAPACIDAD_COLA 4096
typedef struct { int id; int tipo; int orden_llegada; } EntradaCola;
static EntradaCola cola_oe[CAPACIDAD_COLA], cola_eo[CAPACIDAD_COLA];
static int cab_oe = 0, fin_oe = 0;
static int cab_eo = 0, fin_eo = 0;
static int contador_llegada = 0;
static int puente_congelado = 0;

static int tamanio_oe(void){ return (fin_oe - cab_oe + CAPACIDAD_COLA) % CAPACIDAD_COLA; }
static int tamanio_eo(void){ return (fin_eo - cab_eo + CAPACIDAD_COLA) % CAPACIDAD_COLA; }

void *hilo_puerta_carnage(void *arg) {
    DatosVehiculo *vehiculo = (DatosVehiculo *)arg;

    pthread_mutex_lock(&mutex_puerta);


    pthread_mutex_lock(&puente_global.mutex_estado);
    if (vehiculo->direccion == DIR_OE) puente_global.carros_esperando_oe++;
    else                               puente_global.carros_esperando_eo++;
    pthread_mutex_unlock(&puente_global.mutex_estado);

    int mi_orden = contador_llegada++;
    if (vehiculo->direccion == DIR_OE) {
        cola_oe[fin_oe % CAPACIDAD_COLA] = (EntradaCola){ vehiculo->id, vehiculo->tipo, mi_orden };
        fin_oe++;
    } else {
        cola_eo[fin_eo % CAPACIDAD_COLA] = (EntradaCola){ vehiculo->id, vehiculo->tipo, mi_orden };
        fin_eo++;
    }

    printf("[WAIT  ] %s #%d (%s) waiting at gate.\n",
           vehiculo->tipo == TIPO_AMBULANCIA ? "AMBULANCE" : "Car",
           vehiculo->id,
           vehiculo->direccion == DIR_OE ? "W->E" : "E->W");

    pthread_cond_broadcast(&cond_puerta);


    while (1) {
        int puente_vacio  = (puente_global.carros_en_puente == 0);
        int mismo_sentido = !puente_vacio &&
                            (puente_global.direccion_actual == vehiculo->direccion);
        int puente_ok     = puente_vacio || mismo_sentido;


        int hay_espacio = (puente_global.carros_en_puente < puente_global.num_espacios);

        int en_la_puerta = (vehiculo->direccion == DIR_OE)
            ? (tamanio_oe() > 0 &&
               cola_oe[cab_oe % CAPACIDAD_COLA].id == vehiculo->id)
            : (tamanio_eo() > 0 &&
               cola_eo[cab_eo % CAPACIDAD_COLA].id == vehiculo->id);


        if (puente_vacio && puente_congelado) {
            puente_congelado = 0;
            pthread_cond_broadcast(&cond_puerta);
        }

        int bloqueado = puente_congelado;

        if (en_la_puerta && !bloqueado && hay_espacio) {

            if (vehiculo->tipo == TIPO_AMBULANCIA) {

                if (puente_ok) {

                    EntradaCola *cola_opp =
                        (vehiculo->direccion == DIR_OE) ? cola_eo : cola_oe;

                    int tam_opp =
                        (vehiculo->direccion == DIR_OE) ? tamanio_eo() : tamanio_oe();

                    int cab_opp =
                        (vehiculo->direccion == DIR_OE) ? cab_eo : cab_oe;

                    int opp_first = 0;

                    if (puente_vacio && tam_opp > 0 &&
                        cola_opp[cab_opp % CAPACIDAD_COLA].tipo == TIPO_AMBULANCIA) {

                        if (cola_opp[cab_opp % CAPACIDAD_COLA].orden_llegada < mi_orden)
                            opp_first = 1;
                    }

                    if (!opp_first)
                        break;
                }


                else if (!puente_ok && !puente_congelado) {
                    puente_congelado = 1;
                    printf("[CARNAGE] AMBULANCE #%d (%s) at gate – freezing, waiting to drain.\n",
                           vehiculo->id,
                           vehiculo->direccion == DIR_OE ? "W->E" : "E->W");

                    pthread_cond_broadcast(&cond_puerta);
                }

            } else {
                if (puente_ok)
                    break;
            }
        }

        pthread_cond_wait(&cond_puerta, &mutex_puerta);
    }


    if (vehiculo->direccion == DIR_OE) {
        cab_oe++;
        puente_global.carros_esperando_oe--;
    } else {
        cab_eo++;
        puente_global.carros_esperando_eo--;
    }

    printf("[GO    ] %s #%d (%s) entering bridge.\n",
           vehiculo->tipo == TIPO_AMBULANCIA ? "AMBULANCE" : "Car",
           vehiculo->id,
           vehiculo->direccion == DIR_OE ? "W->E" : "E->W");


    puente_entrar(vehiculo->direccion);

    pthread_cond_broadcast(&cond_puerta);
    pthread_mutex_unlock(&mutex_puerta);

    hilo_vehiculo(vehiculo);

    pthread_mutex_lock(&mutex_puerta);
    pthread_cond_broadcast(&cond_puerta);
    pthread_mutex_unlock(&mutex_puerta);

    return NULL;
}

void modo_carnage_ejecutar(const Config *cfg) {
    puente_congelado = 0;
    contador_llegada = 0;
    cab_oe = fin_oe  = 0;
    cab_eo = fin_eo  = 0;

    puente_registrar_condicion(&mutex_puerta, &cond_puerta);
    vehiculo_resetear_contador();
    printf("\n=== MODE 1: CARNAGE (FIFO) ===\n");
    printf("Bridge has %d slots. Simulation runs %.0f seconds.\n\n",
           cfg->num_slots, cfg->duracion_simulacion);

    pthread_t hilo_gen_oe, hilo_gen_eo;

    ArgGenerador *gen_oe = malloc(sizeof(ArgGenerador));
    gen_oe->cfg = *cfg; gen_oe->direccion = DIR_OE; gen_oe->funcion_hilo = hilo_puerta_carnage;
    pthread_create(&hilo_gen_oe, NULL, hilo_generador_vehiculos, gen_oe);

    ArgGenerador *gen_eo = malloc(sizeof(ArgGenerador));
    gen_eo->cfg = *cfg; gen_eo->direccion = DIR_EO; gen_eo->funcion_hilo = hilo_puerta_carnage;
    pthread_create(&hilo_gen_eo, NULL, hilo_generador_vehiculos, gen_eo);

    pthread_join(hilo_gen_oe, NULL);
    pthread_join(hilo_gen_eo, NULL);

    sleep(2);
    printf("\n=== SIMULATION ENDED ===\n");
    puente_imprimir_estado();
}
