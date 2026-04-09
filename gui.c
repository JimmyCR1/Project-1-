#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "gui.h"
#include "bridge.h"
#include "config.h"


#define COL_BG          {  30,  30,  40, 255 }
#define COL_PUENTE      {  60,  60,  80, 255 }
#define COL_LIBRE       {  80,  80, 100, 255 }
#define COL_CARRO_OE    {  60, 180, 255, 255 }   
#define COL_CARRO_EO    { 255, 180,  40, 255 }   
#define COL_AMBU_OE     {  60, 255, 120, 255 }   
#define COL_AMBU_EO     { 255,  60,  80, 255 }   
#define COL_TEXTO       { 220, 220, 220, 255 }
#define COL_TITULO      { 255, 220,  60, 255 }
#define COL_ESPERA_OE   {  40, 120, 200, 255 }
#define COL_ESPERA_EO   { 200, 120,  40, 255 }
#define COL_SEMAFORO_V  {  50, 220,  80, 255 }
#define COL_SEMAFORO_R  { 220,  50,  50, 255 }


#define VENTANA_W   1500
#define VENTANA_H   900
#define SLOT_H       54
#define SLOT_W       54
#define SLOT_GAP      4
#define BRIDGE_Y    400 
#define COLA_MAX_VIS 12   


static pthread_t     hilo_gui;
static volatile int  gui_corriendo = 0;
static Config        gui_cfg;
static int           gui_modo;


static void set_color(SDL_Renderer *r, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

static void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, SDL_Color c) {
    set_color(r, c);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

static void draw_rect(SDL_Renderer *r, int x, int y, int w, int h, SDL_Color c) {
    set_color(r, c);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderDrawRect(r, &rect);
}


static void draw_text(SDL_Renderer *r, TTF_Font *f, const char *txt,
                      int x, int y, SDL_Color c, int centrado) {
    if (!txt || !*txt) return;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(f, txt, c);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    if (!tex) { SDL_FreeSurface(surf); return; }
    int w = surf->w, h = surf->h;
    SDL_FreeSurface(surf);
    SDL_Rect dst = { centrado ? x - w/2 : x, y - h/2, w, h };
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
}


static SDL_Color color_slot(int contenido) {
    switch (contenido) {
        case ESPACIO_CARRO_OE: { SDL_Color c = COL_CARRO_OE; return c; }
        case ESPACIO_AMBU_OE:  { SDL_Color c = COL_AMBU_OE;  return c; }
        case ESPACIO_CARRO_EO: { SDL_Color c = COL_CARRO_EO; return c; }
        case ESPACIO_AMBU_EO:  { SDL_Color c = COL_AMBU_EO;  return c; }
        default:               { SDL_Color c = COL_LIBRE;    return c; }
    }
}

static const char *label_slot(int contenido) {
    switch (contenido) {
        case ESPACIO_CARRO_OE: return "C>";
        case ESPACIO_AMBU_OE:  return "A>";
        case ESPACIO_CARRO_EO: return "<C";
        case ESPACIO_AMBU_EO:  return "<A";
        default:               return "";
    }
}


static void *hilo_gui_fn(void *arg) {
    (void)arg;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "[GUI] SDL_Init error: %s\n", SDL_GetError());
        return NULL;
    }
    if (TTF_Init() != 0) {
        fprintf(stderr, "[GUI] TTF_Init error: %s\n", TTF_GetError());
        SDL_Quit();
        return NULL;
    }

    SDL_Window *win = SDL_CreateWindow(
        "Bridge Simulation",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        VENTANA_W, VENTANA_H,
        SDL_WINDOW_SHOWN
    );
    if (!win) {
        fprintf(stderr, "[GUI] Window error: %s\n", SDL_GetError());
        TTF_Quit(); SDL_Quit();
        return NULL;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        fprintf(stderr, "[GUI] Renderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(win); TTF_Quit(); SDL_Quit();
        return NULL;
    }

   
    const char *fuentes[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeMono.ttf",
        "/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf",
        NULL
    };
    TTF_Font *font_big  = NULL;
    TTF_Font *font_med  = NULL;
    TTF_Font *font_small = NULL;
    for (int i = 0; fuentes[i]; i++) {
        font_big   = TTF_OpenFont(fuentes[i], 20);
        font_med   = TTF_OpenFont(fuentes[i], 15);
        font_small = TTF_OpenFont(fuentes[i], 12);
        if (font_big && font_med && font_small) break;
        if (font_big)   { TTF_CloseFont(font_big);   font_big   = NULL; }
        if (font_med)   { TTF_CloseFont(font_med);   font_med   = NULL; }
        if (font_small) { TTF_CloseFont(font_small); font_small = NULL; }
    }
    if (!font_big) {
        fprintf(stderr, "[GUI] No TTF font was found. Install fonts-dejavu.\n");
        SDL_DestroyRenderer(ren); SDL_DestroyWindow(win);
        TTF_Quit(); SDL_Quit();
        return NULL;
    }

    int n = gui_cfg.num_slots;


    int bridge_total_w = n * SLOT_W + (n - 1) * SLOT_GAP;
    int bridge_x0      = (VENTANA_W - bridge_total_w) / 2;

    SDL_Color bg        = COL_BG;
    SDL_Color col_txt   = COL_TEXTO;
    SDL_Color col_tit   = COL_TITULO;
    SDL_Color col_oe    = COL_ESPERA_OE;
    SDL_Color col_eo    = COL_ESPERA_EO;
    SDL_Color col_verde = COL_SEMAFORO_V;
    SDL_Color col_rojo  = COL_SEMAFORO_R;


    const char *nombres_modo[] = { "", "Mode 1: Carnage (FIFO)",
                                       "Mode 2: Traffic lights",
                                       "Mode 3: Traffic Officer" };

    while (gui_corriendo) {


        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                gui_corriendo = 0;
                break; 
            }
            if (ev.type == SDL_KEYDOWN &&
                ev.key.keysym.sym == SDLK_ESCAPE) {
                gui_corriendo = 0;
                break;
            }
        }
        if (!gui_corriendo) break;

 
        pthread_mutex_lock(&puente_global.mutex_estado);
        int en_puente   = puente_global.carros_en_puente;
        int esp_oe      = puente_global.carros_esperando_oe;
        int esp_eo      = puente_global.carros_esperando_eo;
        long cruzados   = puente_global.total_cruzados;
        int dir_actual  = puente_global.direccion_actual;

  
        int contenido[256];
        for (int i = 0; i < n && i < 256; i++)
            contenido[i] = puente_global.contenido_espacio[i];
        pthread_mutex_unlock(&puente_global.mutex_estado);

        
        set_color(ren, bg);
        SDL_RenderClear(ren);

    
        draw_text(ren, font_big,
                  gui_modo >= 1 && gui_modo <= 3 ? nombres_modo[gui_modo] : "Bridge Simulation",
                  VENTANA_W / 2, 28, col_tit, 1);

 
        draw_text(ren, font_med, "West", bridge_x0 - 80, BRIDGE_Y + SLOT_H/2, col_oe, 1);
        draw_text(ren, font_med, "East",  bridge_x0 + bridge_total_w + 80,
                  BRIDGE_Y + SLOT_H/2, col_eo, 1);

       
        SDL_Color col_puente = COL_PUENTE;
        fill_rect(ren, bridge_x0 - 8, BRIDGE_Y - 8,
                  bridge_total_w + 16, SLOT_H + 16, col_puente);

     
        for (int i = 0; i < n; i++) {
            int sx = bridge_x0 + i * (SLOT_W + SLOT_GAP);
            SDL_Color sc = color_slot(contenido[i]);
            fill_rect(ren, sx, BRIDGE_Y, SLOT_W, SLOT_H, sc);
            draw_rect(ren, sx, BRIDGE_Y, SLOT_W, SLOT_H, col_txt);

          
            char num[8];
            snprintf(num, sizeof(num), "%d", i);
            draw_text(ren, font_small, num,
                      sx + SLOT_W/2, BRIDGE_Y + 10, col_txt, 1);

            
            const char *lbl = label_slot(contenido[i]);
            if (*lbl)
                draw_text(ren, font_med, lbl,
                          sx + SLOT_W/2, BRIDGE_Y + SLOT_H/2 + 6, col_txt, 1);
        }

        
        {
            char flecha[32];
            snprintf(flecha, sizeof(flecha), "Dir: %s",
                     dir_actual == DIR_OE ? "W --> E" : "E --> W");
            draw_text(ren, font_med, flecha,
                      VENTANA_W / 2, BRIDGE_Y + SLOT_H + 28, col_tit, 1);
        }

     
        {
            int dibujar = esp_oe > COLA_MAX_VIS ? COLA_MAX_VIS : esp_oe;
            int cx = bridge_x0 - 30;
            int cy = BRIDGE_Y;
            int sq = 22, gap = 4;
            for (int i = 0; i < dibujar; i++) {
                fill_rect(ren, cx - (i+1)*(sq+gap), cy + (SLOT_H - sq)/2,
                          sq, sq, col_oe);
                draw_rect(ren, cx - (i+1)*(sq+gap), cy + (SLOT_H - sq)/2,
                          sq, sq, col_txt);
            }
            if (esp_oe > COLA_MAX_VIS) {
                char mas[16];
                snprintf(mas, sizeof(mas), "+%d", esp_oe - COLA_MAX_VIS);
                draw_text(ren, font_small, mas,
                          cx - (COLA_MAX_VIS+1)*(sq+gap) - 10,
                          cy + SLOT_H/2, col_oe, 1);
            }
        }

        
        {
            int dibujar = esp_eo > COLA_MAX_VIS ? COLA_MAX_VIS : esp_eo;
            int cx = bridge_x0 + bridge_total_w + 30;
            int cy = BRIDGE_Y;
            int sq = 22, gap = 4;
            for (int i = 0; i < dibujar; i++) {
                fill_rect(ren, cx + (i+1)*(sq+gap), cy + (SLOT_H - sq)/2,
                          sq, sq, col_eo);
                draw_rect(ren, cx + (i+1)*(sq+gap), cy + (SLOT_H - sq)/2,
                          sq, sq, col_txt);
            }
            if (esp_eo > COLA_MAX_VIS) {
                char mas[16];
                snprintf(mas, sizeof(mas), "+%d", esp_eo - COLA_MAX_VIS);
                draw_text(ren, font_small, mas,
                          cx + (COLA_MAX_VIS+1)*(sq+gap) + 10,
                          cy + SLOT_H/2, col_eo, 1);
            }
        }

       
        int panel_y = BRIDGE_Y + SLOT_H + 70;

       
        fill_rect(ren, 20, panel_y, VENTANA_W - 40, 110,
                  (SDL_Color){45, 45, 60, 255});
        draw_rect(ren, 20, panel_y, VENTANA_W - 40, 110, col_txt);

        char buf[128];

        snprintf(buf, sizeof(buf), "On the bridge: %d / %d slots",
                 en_puente, n);
        draw_text(ren, font_med, buf, 50, panel_y + 22, col_txt, 0);

        snprintf(buf, sizeof(buf), "Waiting W->E: %d", esp_oe);
        draw_text(ren, font_med, buf, 50, panel_y + 48, col_oe, 0);

        snprintf(buf, sizeof(buf), "Waiting E->W: %d", esp_eo);
        draw_text(ren, font_med, buf, 50, panel_y + 72, col_eo, 0);

        snprintf(buf, sizeof(buf), "Crusaders : %ld", cruzados);
        draw_text(ren, font_med, buf, 450, panel_y + 22, col_txt, 0);

        snprintf(buf, sizeof(buf), "Bridge`s Slots: %d", n);
        draw_text(ren, font_med, buf, 450, panel_y + 48, col_txt, 0);

        snprintf(buf, sizeof(buf), "Simul.: %.0f s | Ambulances: %.0f%%",
                 gui_cfg.duracion_simulacion,
                 gui_cfg.porcentaje_ambulancias * 100.0);
        draw_text(ren, font_med, buf, 450, panel_y + 72, col_txt, 0);

        
        if (gui_modo == 2) {
        
            SDL_Color c_oe = (dir_actual == DIR_OE) ? col_verde : col_rojo;
            SDL_Color c_eo = (dir_actual == DIR_EO) ? col_verde : col_rojo;
            fill_rect(ren, bridge_x0 - 20, BRIDGE_Y - 55, 18, 18, c_oe);
            draw_rect(ren, bridge_x0 - 20, BRIDGE_Y - 55, 18, 18, col_txt);
            draw_text(ren, font_small, "W->E",
                      bridge_x0 - 11, BRIDGE_Y - 68, c_oe, 1);

            fill_rect(ren, bridge_x0 + bridge_total_w + 4,
                      BRIDGE_Y - 55, 18, 18, c_eo);
            draw_rect(ren, bridge_x0 + bridge_total_w + 4,
                      BRIDGE_Y - 55, 18, 18, col_txt);
            draw_text(ren, font_small, "E->W",
                      bridge_x0 + bridge_total_w + 13, BRIDGE_Y - 68, c_eo, 1);
        }

  
        int ley_y = 68;
        int ley_x = 30;
        int sq = 16;
        struct { SDL_Color c; const char *lbl; } leyenda[] = {
            { COL_CARRO_OE, "Car W->E"  },
            { COL_AMBU_OE,  "Ambu W->E"  },
            { COL_CARRO_EO, "Car E->W" },
            { COL_AMBU_EO,  "Ambu E->W"  },
            { COL_LIBRE,    "Free"       },
        };
        for (int i = 0; i < 5; i++) {
            fill_rect(ren, ley_x + i*160, ley_y, sq, sq, leyenda[i].c);
            draw_rect(ren, ley_x + i*160, ley_y, sq, sq, col_txt);
            draw_text(ren, font_small, leyenda[i].lbl,
                      ley_x + i*160 + sq + 5, ley_y + sq/2, col_txt, 0);
        }

        
        draw_text(ren, font_small, "ESC: close the window",
                  VENTANA_W - 20, VENTANA_H - 14, col_txt,
                   0);

        SDL_RenderPresent(ren);
        SDL_Delay(50);   
    }

    TTF_CloseFont(font_big);
    TTF_CloseFont(font_med);
    TTF_CloseFont(font_small);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    return NULL;
}


int gui_iniciar(const Config *cfg, int modo) {
    gui_cfg     = *cfg;
    gui_modo    = modo;
    gui_corriendo = 1;

    if (pthread_create(&hilo_gui, NULL, hilo_gui_fn, NULL) != 0) {
        gui_corriendo = 0;
        return -1;
    }
    return 0;
}

void gui_detener(void) {
    gui_corriendo = 0;
    pthread_join(hilo_gui, NULL);
}
