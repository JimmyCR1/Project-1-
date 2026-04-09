#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"

#define LONGITUD_CARRO 4.5


static char *recortar_espacios(char *cadena) {
    while (isspace((unsigned char)*cadena)) cadena++;
    if (*cadena == '\0') return cadena;
    char *fin = cadena + strlen(cadena) - 1;
    while (fin > cadena && isspace((unsigned char)*fin)) fin--;
    *(fin + 1) = '\0';
    return cadena;
}


static void eliminar_comentario(char *cadena) {
    char *posicion_numeral = strchr(cadena, '#');
    if (posicion_numeral) *posicion_numeral = '\0';
}


#define NUM_CLAVES_REQUERIDAS 13
static const char *claves_requeridas[NUM_CLAVES_REQUERIDAS] = {
    "bridge_length",
    "west_arrival_mean", "west_speed_min", "west_speed_max",
    "east_arrival_mean", "east_speed_min", "east_speed_max",
    "green_duration",
    "k1", "k2",
    "ambulance_pct",
    "simulation_duration",
    NULL
};

int config_cargar(const char *nombre_archivo, Config *cfg) {
    FILE *archivo = fopen(nombre_archivo, "r");
    if (!archivo) {
        fprintf(stderr, "ERROR: Cannot open config file '%s'\n", nombre_archivo);
        return -1;
    }

    
    int claves_encontradas[NUM_CLAVES_REQUERIDAS];
    memset(claves_encontradas, 0, sizeof(claves_encontradas));

   
    cfg->longitud_puente        = 0.0;
    cfg->media_llegada_oeste    = 0.0;
    cfg->velocidad_min_oeste    = 0.0;
    cfg->velocidad_max_oeste    = 0.0;
    cfg->media_llegada_este     = 0.0;
    cfg->velocidad_min_este     = 0.0;
    cfg->velocidad_max_este     = 0.0;
    cfg->duracion_verde         = 0.0;
    cfg->k1                     = 0;
    cfg->k2                     = 0;
    cfg->porcentaje_ambulancias = 0.0;
    cfg->duracion_simulacion    = 0.0;
    cfg->num_slots              = 0;

    char linea[512];
    int  numero_linea = 0;
    int  num_errores  = 0;

    while (fgets(linea, sizeof(linea), archivo)) {
        numero_linea++;
        eliminar_comentario(linea);
        char *contenido = recortar_espacios(linea);
        if (*contenido == '\0') continue;

        
        char *signo_igual = strchr(contenido, '=');
        if (!signo_igual) {
            fprintf(stderr, "WARN  [line %d]: no '=' found, skipping\n", numero_linea);
            continue;
        }
        *signo_igual = '\0';
        char *clave = recortar_espacios(contenido);
        char *valor = recortar_espacios(signo_igual + 1);

        if (*clave == '\0' || *valor == '\0') {
            fprintf(stderr, "WARN  [line %d]: empty key or value, skipping\n", numero_linea);
            continue;
        }

        double valor_numerico = atof(valor);

        if (!strcmp(clave,"bridge_length"))       { cfg->longitud_puente        = valor_numerico; claves_encontradas[0]  = 1; continue; }
        if (!strcmp(clave,"west_arrival_mean"))   { cfg->media_llegada_oeste    = valor_numerico; claves_encontradas[1]  = 1; continue; }
        if (!strcmp(clave,"west_speed_min"))      { cfg->velocidad_min_oeste    = valor_numerico; claves_encontradas[2]  = 1; continue; }
        if (!strcmp(clave,"west_speed_max"))      { cfg->velocidad_max_oeste    = valor_numerico; claves_encontradas[3]  = 1; continue; }
        if (!strcmp(clave,"east_arrival_mean"))   { cfg->media_llegada_este     = valor_numerico; claves_encontradas[4]  = 1; continue; }
        if (!strcmp(clave,"east_speed_min"))      { cfg->velocidad_min_este     = valor_numerico; claves_encontradas[5]  = 1; continue; }
        if (!strcmp(clave,"east_speed_max"))      { cfg->velocidad_max_este     = valor_numerico; claves_encontradas[6]  = 1; continue; }
        if (!strcmp(clave,"green_duration"))      { cfg->duracion_verde         = valor_numerico; claves_encontradas[7]  = 1; continue; }
        if (!strcmp(clave,"k1"))                  { cfg->k1                     = (int)valor_numerico; claves_encontradas[8]  = 1; continue; }
        if (!strcmp(clave,"k2"))                  { cfg->k2                     = (int)valor_numerico; claves_encontradas[9]  = 1; continue; }
        if (!strcmp(clave,"ambulance_pct"))       { cfg->porcentaje_ambulancias = valor_numerico; claves_encontradas[10] = 1; continue; }
        if (!strcmp(clave,"simulation_duration")) { cfg->duracion_simulacion    = valor_numerico; claves_encontradas[11] = 1; continue; }

        fprintf(stderr, "WARN  [line %d]: unknown key '%s' (ignored)\n", numero_linea, clave);
    }
    fclose(archivo);

    
    for (int i = 0; i < NUM_CLAVES_REQUERIDAS - 1; i++) {
        if (!claves_encontradas[i]) {
            fprintf(stderr, "ERROR: Required parameter '%s' missing from '%s'\n",
                    claves_requeridas[i], nombre_archivo);
            num_errores++;
        }
    }
    if (num_errores) return -1;

   
    if (cfg->longitud_puente <= 0.0)                                                               { fprintf(stderr,"ERROR: bridge_length must be > 0\n");                 num_errores++; }
    if (cfg->velocidad_min_oeste<=0.0||cfg->velocidad_min_oeste>=cfg->velocidad_max_oeste)        { fprintf(stderr,"ERROR: west_speed_min invalid\n");                    num_errores++; }
    if (cfg->velocidad_min_este <=0.0||cfg->velocidad_min_este >=cfg->velocidad_max_este)         { fprintf(stderr,"ERROR: east_speed_min invalid\n");                    num_errores++; }
    if (cfg->media_llegada_oeste <= 0.0)                                                           { fprintf(stderr,"ERROR: west_arrival_mean must be > 0\n");            num_errores++; }
    if (cfg->media_llegada_este  <= 0.0)                                                           { fprintf(stderr,"ERROR: east_arrival_mean must be > 0\n");            num_errores++; }
    if (cfg->duracion_verde      <= 0.0)                                                           { fprintf(stderr,"ERROR: green_duration must be > 0\n");               num_errores++; }
    if (cfg->k1 < 1 || cfg->k2 < 1)                                                               { fprintf(stderr,"ERROR: k1 and k2 must be >= 1\n");                   num_errores++; }
    if (cfg->porcentaje_ambulancias<0.0||cfg->porcentaje_ambulancias>1.0)                         { fprintf(stderr,"ERROR: ambulance_pct must be between 0.0 and 1.0\n"); num_errores++; }
    if (cfg->duracion_simulacion <= 0.0)                                                           { fprintf(stderr,"ERROR: simulation_duration must be > 0\n");          num_errores++; }
    if (num_errores) return -1;

    
    cfg->num_slots = (int)(cfg->longitud_puente / LONGITUD_CARRO);
    if (cfg->num_slots < 1) {
        fprintf(stderr, "ERROR: Bridge too short (minimum %.1f m)\n", LONGITUD_CARRO);
        return -1;
    }
    return 0;
}

void config_imprimir(const Config *cfg) {
    printf("╔══════════════════════════════════════════╗\n");
    printf("║         BRIDGE SIMULATION CONFIG         ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  Bridge length      : %6.1f m            ║\n", cfg->longitud_puente);
    printf("║  Car length         :    4.5 m            ║\n");
    printf("║  Bridge slots       : %6d               ║\n", cfg->num_slots);
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  WEST -> EAST                            ║\n");
    printf("║    Arrival mean     : %6.2f s            ║\n", cfg->media_llegada_oeste);
    printf("║    Speed range      : %4.1f - %4.1f m/s    ║\n", cfg->velocidad_min_oeste, cfg->velocidad_max_oeste);
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  EAST -> WEST                            ║\n");
    printf("║    Arrival mean     : %6.2f s            ║\n", cfg->media_llegada_este);
    printf("║    Speed range      : %4.1f - %4.1f m/s    ║\n", cfg->velocidad_min_este, cfg->velocidad_max_este);
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  Mode 2 green time  : %6.1f s            ║\n", cfg->duracion_verde);
    printf("║  Mode 3  K1 (W->E)  : %6d               ║\n", cfg->k1);
    printf("║  Mode 3  K2 (E->W)  : %6d               ║\n", cfg->k2);
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  Ambulance %%        : %5.0f%%              ║\n", cfg->porcentaje_ambulancias * 100.0);
    printf("║  Simulation time    : %6.1f s            ║\n", cfg->duracion_simulacion);
    printf("╚══════════════════════════════════════════╝\n");
}
