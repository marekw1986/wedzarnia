#ifndef TYPES_H
#define TYPES_H

typedef enum {INIT, SELECT_PROGRAM, CONFIG_MENU, WEDZENIE_MAIN_MENU, WEDZENIE_SUB_MENU_STAGES_CNT,
WEDZENIE_SUB_MENU_CONFIG_STAGE, WEDZENIE_SUB_MENU_CONFIG_STAGE_SEL_END_TRIG, WARZENIE_MAIN_MENU,
MAIN_MENU, SUB_MENU_SENSOR_SELECT, WEDZENIE_PERFORM, WEDZENIE_PERFORM_WAIT_NEXT, WARZENIE_PERFORM, TEST} state_t;

enum stage_end_trig_t {STAGE_END_TRIG_TIME, STAGE_END_TRIG_TEMP, STAGE_END_TRIG_NONE};

struct stage_t {
  stage_end_trig_t end_trigger;
  unsigned char czujnik;
  unsigned char czas_trwania;             //w minutach
  unsigned char interwal_wiatraka;         //w minutach ?
  unsigned char praca_wiatraka;            //w minutach ?
  unsigned char interwal_pompy;            //w minutach ?
  unsigned char praca_pompy;               //w minutach ?
  unsigned char koncowa_temperatura;
};

struct conf_t {
  unsigned char liczba_etapow;
  stage_t stage[6];
  unsigned char warz_temp_wody;
  unsigned char warz_temp_miesa;
};

#endif
