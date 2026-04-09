#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "mode_officer.h"
#include "bridge.h"
#include "vehicle.h"

static pthread_mutex_t mutex_oficial = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cond_oficial  = PTHREAD_COND_INITIALIZER;

static int direccion_oficial = DIR_OE;
static int pases_restantes   = 0;
static int puente_abierto    = 0;
static int simulacion_activa = 1;
static Config config_global;

static int deuda_oe = 0, deuda_eo = 0;
static int pases_guardados = 0;
static int id_ambu_contraria_autorizada = -1;

#define CAPACIDAD_COLA 4096
typedef struct { int id; int tipo; } EntradaCola;
static EntradaCola cola_oe[CAPACIDAD_COLA], cola_eo[CAPACIDAD_COLA];
static int cab_oe = 0, fin_oe = 0;
static int cab_eo = 0, fin_eo = 0;

static int tamanio_oe(void){ return (fin_oe - cab_oe + CAPACIDAD_COLA) % CAPACIDAD_COLA; }
static int tamanio_eo(void){ return (fin_eo - cab_eo + CAPACIDAD_COLA) % CAPACIDAD_COLA; }
static int tam_actual(void)  { return direccion_oficial==DIR_OE ? tamanio_oe() : tamanio_eo(); }
static int tam_opuesto(void) { return direccion_oficial==DIR_OE ? tamanio_eo() : tamanio_oe(); }
static EntradaCola cabeza_opuesta(void) {
    return direccion_oficial==DIR_OE ? cola_eo[cab_eo%CAPACIDAD_COLA]
                                     : cola_oe[cab_oe%CAPACIDAD_COLA];
}

void *hilo_puerta_oficial(void *arg) {
    DatosVehiculo *vehiculo = (DatosVehiculo *)arg;

    pthread_mutex_lock(&mutex_oficial);


    if (vehiculo->direccion == DIR_OE) {
        cola_oe[fin_oe % CAPACIDAD_COLA] = (EntradaCola){ vehiculo->id, vehiculo->tipo };
        fin_oe++;
        puente_global.carros_esperando_oe++;
    } else {
        cola_eo[fin_eo % CAPACIDAD_COLA] = (EntradaCola){ vehiculo->id, vehiculo->tipo };
        fin_eo++;
        puente_global.carros_esperando_eo++;
    }
    printf("[WAIT  ] %s #%d (%s) waiting for officer.\n",
           vehiculo->tipo == TIPO_AMBULANCIA ? "AMBULANCE" : "Car",
           vehiculo->id, vehiculo->direccion == DIR_OE ? "W->E" : "E->W");
    pthread_cond_broadcast(&cond_oficial);


    while (1) {
        int mi_turno  = (direccion_oficial == vehiculo->direccion);
        int puente_ok = (puente_global.carros_en_puente == 0)
                     || (puente_global.direccion_actual == vehiculo->direccion);


        int hay_espacio = (puente_global.carros_en_puente < puente_global.num_espacios);

        int en_la_puerta = (vehiculo->direccion == DIR_OE)
            ? (tamanio_oe() > 0 && cola_oe[cab_oe % CAPACIDAD_COLA].id == vehiculo->id)
            : (tamanio_eo() > 0 && cola_eo[cab_eo % CAPACIDAD_COLA].id == vehiculo->id);

        if (id_ambu_contraria_autorizada == vehiculo->id) {
            if (puente_abierto && puente_global.carros_en_puente == 0) break;
        } else if (mi_turno && en_la_puerta && hay_espacio) {
            int tiene_pase = (pases_restantes > 0);
            if (vehiculo->tipo == TIPO_AMBULANCIA) {
                if (puente_abierto && puente_ok && id_ambu_contraria_autorizada < 0) break;
            } else {
                if (puente_abierto && tiene_pase && puente_ok && id_ambu_contraria_autorizada < 0) break;
            }
        }
        pthread_cond_wait(&cond_oficial, &mutex_oficial);
    }

    if (id_ambu_contraria_autorizada == vehiculo->id) {
        id_ambu_contraria_autorizada = -1;
        puente_abierto = 0;
        printf("[OFFICER] AMBULANCE #%d (opposite, %s) entering bridge alone.\n",
               vehiculo->id, vehiculo->direccion == DIR_OE ? "W->E" : "E->W");
    } else if (vehiculo->tipo == TIPO_AMBULANCIA) {
        pases_restantes--;
        if (pases_restantes < 0) {
            if (vehiculo->direccion == DIR_OE) deuda_oe++;
            else                               deuda_eo++;
            printf("[OFFICER] AMBULANCE #%d (%s) at gate, K=0 → enters (debt=%d).\n",
                   vehiculo->id, vehiculo->direccion == DIR_OE ? "W->E" : "E->W",
                   vehiculo->direccion == DIR_OE ? deuda_oe : deuda_eo);
        } else {
            printf("[OFFICER] AMBULANCE #%d (%s) entering. Passes left: %d\n",
                   vehiculo->id, vehiculo->direccion == DIR_OE ? "W->E" : "E->W", pases_restantes);
        }
    } else {
        pases_restantes--;
        printf("[OFFICER] Car #%d (%s) authorized. Passes left: %d\n",
               vehiculo->id, vehiculo->direccion == DIR_OE ? "W->E" : "E->W", pases_restantes);
    }

    if (vehiculo->direccion == DIR_OE) { cab_oe++; puente_global.carros_esperando_oe--; }
    else                               { cab_eo++; puente_global.carros_esperando_eo--; }


    puente_entrar(vehiculo->direccion);

    pthread_cond_broadcast(&cond_oficial);
    pthread_mutex_unlock(&mutex_oficial);

    hilo_vehiculo(vehiculo);

    pthread_mutex_lock(&mutex_oficial);
    pthread_cond_broadcast(&cond_oficial);
    pthread_mutex_unlock(&mutex_oficial);

    return NULL;
}

static void esperar_drenaje(void) {
    puente_abierto = 0;
    if (puente_global.carros_en_puente > 0) {
        printf("[OFFICER] Draining bridge (%d vehicles)...\n", puente_global.carros_en_puente);
        while (puente_global.carros_en_puente > 0 && simulacion_activa)
            pthread_cond_wait(&cond_oficial, &mutex_oficial);
        printf("[OFFICER] Bridge drained.\n");
    }
}

static int calcular_pases(int dir) {
    int k = (dir==DIR_OE) ? config_global.k1 : config_global.k2;
    int d = (dir==DIR_OE) ? deuda_oe : deuda_eo;
    int p = k - d;
    if (dir==DIR_OE) deuda_oe=0; else deuda_eo=0;
    return (p < 1) ? 1 : p;
}



typedef struct { int mi_dir; } ArgOficial;

static void *hilo_oficial_transito(void *arg) {
    ArgOficial *ao = (ArgOficial *)arg;
    int mi_dir = ao->mi_dir;  
    free(ao);

    while (simulacion_activa) {
        pthread_mutex_lock(&mutex_oficial);


        if (direccion_oficial != mi_dir) {
            pthread_cond_wait(&cond_oficial, &mutex_oficial);
            pthread_mutex_unlock(&mutex_oficial);
            continue;
        }


        if (tam_actual()==0 && tam_opuesto()==0 && puente_global.carros_en_puente==0) {
            puente_abierto = 0;
            pthread_cond_wait(&cond_oficial, &mutex_oficial);
            pthread_mutex_unlock(&mutex_oficial);
            continue;
        }


        int ambu_opp = (tam_opuesto()>0) && (cabeza_opuesta().tipo==TIPO_AMBULANCIA);
        int ambu_cur = (tam_actual()>0) && ((direccion_oficial==DIR_OE
                        ? cola_oe[cab_oe%CAPACIDAD_COLA]
                        : cola_eo[cab_eo%CAPACIDAD_COLA]).tipo==TIPO_AMBULANCIA);

        if (ambu_opp && id_ambu_contraria_autorizada<0) {
            int debo_interrumpir = (pases_restantes<=0 && !ambu_cur) || (tam_actual()==0);
            if (debo_interrumpir) {
                int id_ambu = cabeza_opuesta().id;
                pases_guardados = pases_restantes;
                pases_restantes = 0;
                id_ambu_contraria_autorizada = id_ambu;
                puente_abierto = 0;
                printf("[OFFICER-%s] Opposite AMBULANCE #%d at gate. Freezing.\n",
                       mi_dir==DIR_OE?"W->E":"E->W", id_ambu);
                esperar_drenaje();
                direccion_oficial = (direccion_oficial==DIR_OE) ? DIR_EO : DIR_OE;
                puente_abierto = 1;
                printf("[OFFICER-%s] AMBULANCE #%d may enter (%s).\n",
                       mi_dir==DIR_OE?"W->E":"E->W",
                       id_ambu, direccion_oficial==DIR_OE ? "W->E" : "E->W");
                pthread_cond_broadcast(&cond_oficial);
                while ((id_ambu_contraria_autorizada>=0 || puente_global.carros_en_puente>0)
                       && simulacion_activa)
                    pthread_cond_wait(&cond_oficial, &mutex_oficial);
                direccion_oficial = (direccion_oficial==DIR_OE) ? DIR_EO : DIR_OE;
                pases_restantes = pases_guardados;
                puente_abierto = (pases_restantes>0) ? 1 : 0;
                printf("[OFFICER-%s] Ambulance done. Resuming %s with %d passes.\n",
                       mi_dir==DIR_OE?"W->E":"E->W",
                       direccion_oficial==DIR_OE ? "W->E" : "E->W", pases_restantes);
                pthread_cond_broadcast(&cond_oficial);
                pthread_mutex_unlock(&mutex_oficial);
                continue;
            }
        }


        if (pases_restantes<=0) {
            if (tam_opuesto()==0) {
               
                pases_restantes = calcular_pases(direccion_oficial);
                puente_abierto = 1;
                printf("[OFFICER-%s] Other side empty. New batch: %d passes for %s.\n",
                       mi_dir==DIR_OE?"W->E":"E->W",
                       pases_restantes, direccion_oficial==DIR_OE ? "W->E" : "E->W");
                pthread_cond_broadcast(&cond_oficial);
                pthread_mutex_unlock(&mutex_oficial);
                continue;
            }
           
            esperar_drenaje();
            direccion_oficial = (direccion_oficial==DIR_OE) ? DIR_EO : DIR_OE;
            pases_restantes = calcular_pases(direccion_oficial);
            puente_abierto = (tam_actual()>0) ? 1 : 0;
            printf("[OFFICER-%s] Switched to %s. Granting %d passes.\n",
                   mi_dir==DIR_OE?"W->E":"E->W",
                   direccion_oficial==DIR_OE ? "W->E" : "E->W", pases_restantes);
            pthread_cond_broadcast(&cond_oficial);
            pthread_mutex_unlock(&mutex_oficial);
            continue;
        }

       
        if (tam_actual()==0 && tam_opuesto()>0 && puente_global.carros_en_puente==0) {
            printf("[OFFICER-%s] Current side empty mid-batch. Early switch.\n",
                   mi_dir==DIR_OE?"W->E":"E->W");
            direccion_oficial = (direccion_oficial==DIR_OE) ? DIR_EO : DIR_OE;
            pases_restantes = calcular_pases(direccion_oficial);
            puente_abierto = 1;
            printf("[OFFICER-%s] Switched to %s. Granting %d passes.\n",
                   mi_dir==DIR_OE?"W->E":"E->W",
                   direccion_oficial==DIR_OE ? "W->E" : "E->W", pases_restantes);
            pthread_cond_broadcast(&cond_oficial);
            pthread_mutex_unlock(&mutex_oficial);
            continue;
        }

        if (tam_actual()>0) puente_abierto = 1;
        pthread_cond_broadcast(&cond_oficial);
        pthread_cond_wait(&cond_oficial, &mutex_oficial);
        pthread_mutex_unlock(&mutex_oficial);
    }
    return NULL;
}

void modo_oficial_ejecutar(const Config *cfg) {
    config_global = *cfg;
    direccion_oficial = DIR_OE;
    pases_restantes = cfg->k1;
    puente_abierto = 1;
    simulacion_activa = 1;
    id_ambu_contraria_autorizada = -1;
    pases_guardados = 0;
    deuda_oe = deuda_eo = 0;
    cab_oe = fin_oe = cab_eo = fin_eo = 0;

    puente_registrar_condicion(&mutex_oficial, &cond_oficial);
    vehiculo_resetear_contador();
    printf("\n=== MODE 3: TRAFFIC OFFICER ===\n");
    printf("K1(W->E)=%d  K2(E->W)=%d.  Bridge slots: %d.\n\n",
           cfg->k1, cfg->k2, cfg->num_slots);

  
    pthread_t hilo_oficial_oe, hilo_oficial_eo;
    ArgOficial *ao_oe = malloc(sizeof(ArgOficial)); ao_oe->mi_dir = DIR_OE;
    ArgOficial *ao_eo = malloc(sizeof(ArgOficial)); ao_eo->mi_dir = DIR_EO;
    pthread_create(&hilo_oficial_oe, NULL, hilo_oficial_transito, ao_oe);
    pthread_create(&hilo_oficial_eo, NULL, hilo_oficial_transito, ao_eo);

    pthread_t hilo_gen_oe, hilo_gen_eo;
    ArgGenerador *gen_oe = malloc(sizeof(ArgGenerador));
    gen_oe->cfg = *cfg; gen_oe->direccion = DIR_OE; gen_oe->funcion_hilo = hilo_puerta_oficial;
    ArgGenerador *gen_eo = malloc(sizeof(ArgGenerador));
    gen_eo->cfg = *cfg; gen_eo->direccion = DIR_EO; gen_eo->funcion_hilo = hilo_puerta_oficial;
    pthread_create(&hilo_gen_oe, NULL, hilo_generador_vehiculos, gen_oe);
    pthread_create(&hilo_gen_eo, NULL, hilo_generador_vehiculos, gen_eo);

    pthread_join(hilo_gen_oe, NULL);
    pthread_join(hilo_gen_eo, NULL);

    simulacion_activa = 0;
    pthread_mutex_lock(&mutex_oficial);
    puente_abierto = 0;
    pthread_cond_broadcast(&cond_oficial);
    pthread_mutex_unlock(&mutex_oficial);
    pthread_join(hilo_oficial_oe, NULL);
    pthread_join(hilo_oficial_eo, NULL);

    sleep(2);
    printf("\n=== SIMULATION ENDED ===\n");
    puente_imprimir_estado();
}
