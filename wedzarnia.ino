#include <U8g2lib.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <stdio.h>
#include "types.h"
#include "heater.h"
#include "pt1000.h"

#define LEFT_BUTTON_PIN     5
#define RIGHT_BUTTON_PIN    6
#define CENTRAL_BUTTON_PIN  7
#define BEEP_PIN            4
#define WIATRAK_PIN          17
#define POMPA_PIN          18

#define TEMP0_PIN           6
#define TEMP1_PIN           7

#define BT_TX_PIN           2
#define BT_RX_PIN           3

#define LCD_RST             8
#define LCD_CS              9
#define LCD_A0              10
#define LCD_DATA_IN         11
#define LCD_CLK             13


conf_t cfg;
conf_t eeprom_cfg EEMEM;

unsigned char i;
unsigned long timer=0;
unsigned long wiatrak_timer = 0;
unsigned long pompa_timer = 0;
unsigned char current_stage = 0;
unsigned int sekundy = 0;
int sekundy_do_konca = 0;
state_t state = INIT;

U8G2_ST7565_EA_DOGM128_1_4W_SW_SPI glcd(U8G2_R0, LCD_CLK, LCD_DATA_IN, LCD_CS, LCD_A0, LCD_RST);
pt1000 termometr[2] = {pt1000(TEMP0_PIN), pt1000(TEMP1_PIN)};


void termostat (int czujnik, int wartosc_docelowa) {
  if (czujnik < wartosc_docelowa) {
    Heater.increasePower(1);
  }
  else if (czujnik > wartosc_docelowa) {
    Heater.decreasePower(1);
  }
}

void handle_motor_cycle (unsigned long *cycleStart, unsigned char pin, unsigned long interwal, unsigned long praca) {
  if (!(*cycleStart)) return;
  if ( (unsigned long)(millis()-*cycleStart) >= praca) {
    digitalWrite(pin, 0);
  }
  if ( (unsigned long)(millis()-*cycleStart) >= interwal) {
    digitalWrite(pin, 1);
    *cycleStart = millis();
  }
}

void restart_motor_cycle (unsigned long *cycleStart, unsigned char pin, unsigned char interwal) {
  if (interwal == 0 ) {
    digitalWrite(pin, 0);
    *cycleStart = 0;
  }
  else {
    digitalWrite(pin, 1);
    *cycleStart = millis();
  }
}

char * time_str (unsigned int s) {
  static char buff[12];
  unsigned char h, m;

  h = s/3600;
  s = s%3600;
  m = s/60;
  s = s%60;
  sprintf_P(buff, PSTR("%02i:%02i:%02i"), h, m, s);
  return buff;
}

void stop_all (void) {
  sekundy = 0;
  sekundy_do_konca = 0;
  pompa_timer = 0;
  wiatrak_timer = 0;
  digitalWrite(WIATRAK_PIN, 0);
  digitalWrite(POMPA_PIN, 0);
  Heater.stop();
}

void load_default_settings (void) {
  unsigned char i;

  cfg.liczba_etapow = 4;
  for (i=0; i<6; i++) {
    cfg.stage[i].end_trigger = STAGE_END_TRIG_TIME;
    cfg.stage[i].czujnik = 0;
    cfg.stage[i].czas_trwania = 1;
    cfg.stage[i].interwal_wiatraka = 2;
    cfg.stage[i].praca_wiatraka = 1;
    cfg.stage[i].interwal_pompy = 2;
    cfg.stage[i].praca_pompy = 1;
    cfg.stage[i].koncowa_temperatura = 50;
    
  }
  cfg.warz_temp_wody = 60;
  cfg.warz_temp_miesa = 50;
}


void check_and_load_defaults( void ) {
  uint8_t i, len = sizeof( cfg );
  uint8_t * ram_wsk = (uint8_t*)&cfg;

  eeprom_read_block(&cfg, &eeprom_cfg, sizeof(cfg));
  for(i=0; i<len; i++) {
    if( 0xff == *ram_wsk++ ) continue;
    break;
  }

  if( i == len ) {
    load_default_settings();
    eeprom_write_block(&cfg, &eeprom_cfg, sizeof(cfg));
  }

}


void setup() {
  pinMode(BEEP_PIN, OUTPUT);
  digitalWrite(BEEP_PIN, 0);
  pinMode(WIATRAK_PIN, OUTPUT);
  digitalWrite(WIATRAK_PIN, 0);
  pinMode(POMPA_PIN, OUTPUT);
  digitalWrite(POMPA_PIN, 0);
  
  Serial.begin(9600);
  
//  Heater.setPower(50);
//  Heater.start();

  glcd.begin(CENTRAL_BUTTON_PIN, RIGHT_BUTTON_PIN, LEFT_BUTTON_PIN);
  glcd.setContrast(60);
  glcd.setFont(u8g2_font_6x12_tr);

  load_default_settings();
  i=0;
}

void loop() {
  char menu_title[32];
  char menu_content[200];
  unsigned int selection=1;
  unsigned char len, v;

  switch(state) {

  case INIT:
    if ((unsigned long)(millis()-timer) >= 300) {
      timer = millis();
      termometr[0].read(); termometr[1].read();
      sprintf_P(menu_content, PSTR("Inicjalizacja %i/100"), i*5);
      glcd.firstPage();
      do {
        glcd.setCursor(0,20);
        glcd.print(menu_content);
      } while (glcd.nextPage());
      i++;
      if (i>20) {
        i=0;
        state = SELECT_PROGRAM;
      }
    }
  break;

  case SELECT_PROGRAM:
    sprintf_P(menu_title, PSTR("Wybor programu"));
    sprintf_P(menu_content, PSTR("Wedzenie\nWarzenie\nKonfiguracja"));
    selection = glcd.userInterfaceSelectionList(menu_title, 1, menu_content);
    if (selection == 1) {
      state = WEDZENIE_MAIN_MENU;
    }
    else if (selection == 2) {
      state = WARZENIE_MAIN_MENU;
    }
    else if (selection == 3) {
      state = CONFIG_MENU;
    }
  break;

  case CONFIG_MENU:
    sprintf_P(menu_title, PSTR("Konfiguracja"));
    sprintf_P(menu_content, PSTR("..\nMoc min. %i\nMoc max. %i\nZapisz\nWczytaj\nUst. domyslne"), Heater.getMinPower(), Heater.getMaxPower());
    selection = glcd.userInterfaceSelectionList(menu_title, 1, menu_content);
    if (selection == 1) {
      state = SELECT_PROGRAM;
    }
    else if (selection == 2) {
      //ustawianie mocy minimalnej
      sprintf_P(menu_title, PSTR("Moc min. "));
      v = Heater.getMinPower();
      if ( glcd.userInterfaceInputValue("", menu_title, &v, 0, 100, 3, "") ) Heater.setMinPower(v);
    }
    else if (selection == 3) {
      //ustawianie mocy minimalnej
      sprintf_P(menu_title, PSTR("Moc max. "));
      v = Heater.getMaxPower();
      if ( glcd.userInterfaceInputValue("", menu_title, &v, 1, 100, 3, "") ) Heater.setMaxPower(v);
    }
    else if (selection == 4) {
      //zapisywanie konfiguracji do pamięci EEPROM
      selection = glcd.userInterfaceMessage("Zapisac ustawienia?", "", "", " Tak \n Nie ");
      if (selection == 1) eeprom_write_block(&cfg, &eeprom_cfg, sizeof(cfg));
    }
    else if (selection == 5) {
      //wczytywanie konfiguracji z pamięci EEPROM
      selection = glcd.userInterfaceMessage("Wczytac ustawienia?", "", "", " Tak \n Nie ");
      if (selection == 1) eeprom_read_block(&cfg, &eeprom_cfg, sizeof(cfg));
    }
    else if (selection == 6) {
      // Wczytywanie ustawień domyślnych;
      selection = glcd.userInterfaceMessage("Wczytac domyslne ustawienia?", "", "", " Tak \n Nie ");
      if (selection == 1) load_default_settings();
    }
  break;

  case WEDZENIE_MAIN_MENU:
    sprintf_P(menu_title, PSTR("Wedzenie"));
    sprintf_P(menu_content, PSTR("..\nLiczba etapow: %i\n"), cfg.liczba_etapow);
    len = strlen(menu_content);
    for (i=1; i<=cfg.liczba_etapow; i++) {
      len += sprintf_P(menu_content+len, PSTR("Etap %i\n"), i);
      if (i>6) break;
    }
    sprintf_P(menu_content+len, PSTR("START"));
    selection = glcd.userInterfaceSelectionList(menu_title, 1, menu_content);
    if (selection == 1) {
      state = SELECT_PROGRAM;
    }
    else if (selection == 2) {
      state = WEDZENIE_SUB_MENU_STAGES_CNT;
    }
    else if ( (selection>=3) && (selection<=(2+cfg.liczba_etapow)) ) {
	    current_stage = selection-3;
      state = WEDZENIE_SUB_MENU_CONFIG_STAGE;
    }
    else if (selection == (3+cfg.liczba_etapow)) {
      sprintf_P(menu_title, PSTR("Uruchomic wedzenie?"));
      sprintf_P(menu_content, PSTR("TAK\nNIE"));
	    selection = glcd.userInterfaceMessage(menu_title, "", "", menu_content);
      if (selection == 1) {
        state = WEDZENIE_PERFORM;
        current_stage = 0;
        sekundy = 0;
        sekundy_do_konca = cfg.stage[current_stage].czas_trwania*60;
        restart_motor_cycle(&pompa_timer, POMPA_PIN, cfg.stage[current_stage].interwal_pompy);
        restart_motor_cycle(&wiatrak_timer, WIATRAK_PIN, cfg.stage[current_stage].interwal_wiatraka);
        Heater.start(); // DOBRZE?
      }
      else if (selection == 2) {
        state = WEDZENIE_MAIN_MENU;
      }
    }
  break;

  case WEDZENIE_SUB_MENU_STAGES_CNT:
    sprintf_P(menu_title, PSTR("Liczba etapow: "));
    glcd.userInterfaceInputValue("", menu_title, &(cfg.liczba_etapow), 1, 6, 1, "");
    state = WEDZENIE_MAIN_MENU;
  break;

  case WEDZENIE_SUB_MENU_CONFIG_STAGE:
    sprintf_P(menu_title, PSTR("Etap %i"), current_stage+1);
    sprintf_P(menu_content, PSTR("..\nInterw. wiatr.: %i\nPraca. wiatr.: %i\nInterw. pompy: %i\nPraca. pompy: %i\nKoniec: "), cfg.stage[current_stage].interwal_wiatraka, cfg.stage[current_stage].praca_wiatraka, cfg.stage[current_stage].interwal_pompy, cfg.stage[current_stage].praca_pompy);
    switch(cfg.stage[current_stage].end_trigger) {
      case STAGE_END_TRIG_TIME:
      sprintf_P(menu_content+strlen(menu_content), PSTR("czas\n"));
      break;

      case STAGE_END_TRIG_TEMP:
      sprintf_P(menu_content+strlen(menu_content), PSTR("temperatura\n"));
      break;

      case STAGE_END_TRIG_NONE:
      sprintf_P(menu_content+strlen(menu_content), PSTR("brak\n"));
      break;
    }
    sprintf_P(menu_content+strlen(menu_content), PSTR("Termp. %i st. C\nCzujnik: %i\nCzas: %i min."), cfg.stage[current_stage].koncowa_temperatura, cfg.stage[current_stage].czujnik, cfg.stage[current_stage].czas_trwania);
    selection = glcd.userInterfaceSelectionList(menu_title, 1, menu_content);
    if (selection == 1) {
      state = WEDZENIE_MAIN_MENU;
    }
    else if (selection == 2) {
      sprintf_P(menu_title, PSTR("Int. wiatr. "));
      glcd.userInterfaceInputValue("", menu_title, &(cfg.stage[current_stage].interwal_wiatraka), 0, 255, 3, " min.");      
    }
    else if (selection == 3) {
      sprintf_P(menu_title, PSTR("Praca wiatr. "));
      glcd.userInterfaceInputValue("", menu_title, &(cfg.stage[current_stage].praca_wiatraka), 0, 255, 3, " min.");
      if (cfg.stage[current_stage].praca_wiatraka > cfg.stage[current_stage].interwal_wiatraka) {
        cfg.stage[current_stage].praca_wiatraka = cfg.stage[current_stage].interwal_wiatraka;
      }
    }
    else if (selection == 4) {
      sprintf_P(menu_title, PSTR("Int. pompy "));
      glcd.userInterfaceInputValue("", menu_title, &(cfg.stage[current_stage].interwal_pompy), 0, 255, 3, " min.");   
    }
    else if (selection == 5) {
      sprintf_P(menu_title, PSTR("Praca pompy "));
      glcd.userInterfaceInputValue("", menu_title, &(cfg.stage[current_stage].praca_pompy), 0, 255, 3, " min.");
      if (cfg.stage[current_stage].praca_pompy > cfg.stage[current_stage].interwal_pompy) {
        cfg.stage[current_stage].praca_pompy = cfg.stage[current_stage].interwal_pompy;
      }        
    }    
    else if (selection == 6) {
      state = WEDZENIE_SUB_MENU_CONFIG_STAGE_SEL_END_TRIG;
    }
    else if (selection == 7) {
      sprintf_P(menu_title, PSTR("Konc. temp. "));
      glcd.userInterfaceInputValue("", menu_title, &(cfg.stage[current_stage].koncowa_temperatura), 0, 100, 3, " st C.");
    }
    else if (selection == 8) {
        sprintf_P(menu_title, PSTR("Czujnik temp. "));
        glcd.userInterfaceInputValue("", menu_title, &(cfg.stage[current_stage].czujnik), 0, 1, 1, "");      
    }
    else if (selection == 9) {
        sprintf_P(menu_title, PSTR("Czas "));
        glcd.userInterfaceInputValue("", menu_title, &(cfg.stage[current_stage].czas_trwania), 0, 255, 3, " min.");      
    }
  break;

  case WEDZENIE_SUB_MENU_CONFIG_STAGE_SEL_END_TRIG:
    sprintf_P(menu_title, PSTR("Koniec etapu %i"), current_stage);
    sprintf_P(menu_content, PSTR("Czas\nTemperatura\nBrak"));
    selection = glcd.userInterfaceSelectionList(menu_title, 1, menu_content);
    if (selection == 1) {
      cfg.stage[current_stage].end_trigger = STAGE_END_TRIG_TIME;
    }
    else if (selection == 2 ) {
      cfg.stage[current_stage].end_trigger = STAGE_END_TRIG_TEMP;
    }
    else if (selection == 3) {
      cfg.stage[current_stage].end_trigger = STAGE_END_TRIG_NONE;
    }
    state = WEDZENIE_SUB_MENU_CONFIG_STAGE;
  break;

  case WARZENIE_MAIN_MENU:
    sprintf_P(menu_title, PSTR("Warzenie"));
    sprintf_P(menu_content, PSTR("..\nT. wody [1]: %i st. C\nT. miesa [2]: %i st. C\nStart"), cfg.warz_temp_wody, cfg.warz_temp_miesa);
    selection = glcd.userInterfaceSelectionList(menu_title, 1, menu_content);
    if (selection == 1) {
      state = SELECT_PROGRAM;
    }
    else if (selection == 2) {
      // ustawiamy temperature wody
      sprintf_P(menu_title, PSTR("Temp. wody "));
      glcd.userInterfaceInputValue("", menu_title, &(cfg.warz_temp_wody), 0, 100, 3, "st. C");
    }
    else if (selection == 3) {
      // ustawiamy temperature miesa
      sprintf_P(menu_title, PSTR("Temp. miesa "));
      glcd.userInterfaceInputValue("", menu_title, &(cfg.warz_temp_miesa), 0, 100, 3, "st. C");
    }
    else if (selection == 4) {
      // uruchamiamy proces
      state = WARZENIE_PERFORM;
    }
  break;

  case WEDZENIE_PERFORM ... WEDZENIE_PERFORM_WAIT_NEXT:
    if ((unsigned long)(millis()-timer) >= 1000) {
      sekundy++;
      if ( (state == WEDZENIE_PERFORM) && (cfg.stage[current_stage].end_trigger == STAGE_END_TRIG_TIME) && (sekundy_do_konca > 0) ) sekundy_do_konca--;
      if ( (cfg.stage[current_stage].end_trigger == STAGE_END_TRIG_TIME) && (sekundy >= (cfg.stage[current_stage].czas_trwania * 60)) ) {
        current_stage++;
        sekundy = 0;
        sekundy_do_konca = cfg.stage[current_stage].czas_trwania*60;
        digitalWrite(BEEP_PIN, 1);
        delay(300);
        restart_motor_cycle(&pompa_timer, POMPA_PIN, cfg.stage[current_stage].interwal_pompy);
        restart_motor_cycle(&wiatrak_timer, WIATRAK_PIN, cfg.stage[current_stage].interwal_wiatraka);
        if (current_stage >= cfg.liczba_etapow) {
            stop_all();
            current_stage = 0;
            state = SELECT_PROGRAM;
            delay(1200);
        }
        digitalWrite(BEEP_PIN, 0);
      }

      handle_motor_cycle(&pompa_timer, POMPA_PIN, cfg.stage[current_stage].interwal_pompy*60000, cfg.stage[current_stage].praca_pompy*60000);
      handle_motor_cycle(&wiatrak_timer, WIATRAK_PIN, cfg.stage[current_stage].interwal_wiatraka*60000, cfg.stage[current_stage].praca_wiatraka*60000);
      
      termometr[0].read();
      termometr[1].read();

      termostat((int)termometr[cfg.stage[current_stage].czujnik].degC(), cfg.stage[current_stage].koncowa_temperatura);

      //temoeratura koncowa osiagnieta, przejscie do odliczania na koniec etapu i odliczanie
      if ( cfg.stage[current_stage].end_trigger == STAGE_END_TRIG_TEMP) {
        //temperatura jeszcze nie osiagnieta, czekamy na taka sytuacje
        if (state == WEDZENIE_PERFORM) {
          if ((int)termometr[cfg.stage[current_stage].czujnik].degC() >= cfg.stage[current_stage].koncowa_temperatura) {
            state = WEDZENIE_PERFORM_WAIT_NEXT;
             cfg.stage[current_stage].czas_trwania * 60;
            //TODO?
          }
        }
        else if (state == WEDZENIE_PERFORM_WAIT_NEXT) {
          sekundy_do_konca--;
          if (sekundy_do_konca <= 0) {
            state = WEDZENIE_PERFORM;
            current_stage++;
             cfg.stage[current_stage].czas_trwania*60;
            digitalWrite(BEEP_PIN, 1);
            delay(300);
            restart_motor_cycle(&pompa_timer, POMPA_PIN, cfg.stage[current_stage].interwal_pompy);
            restart_motor_cycle(&wiatrak_timer, WIATRAK_PIN, cfg.stage[current_stage].interwal_wiatraka);            
            if (current_stage >= cfg.liczba_etapow) {
              state = SELECT_PROGRAM;
              stop_all();
              current_stage = 0;
              sekundy = 0;
              delay(1200);
            }
            digitalWrite(BEEP_PIN, 0);
          }
        }
      }

//      if ( (cfg.stage[current_stage].end_trigger == STAGE_END_TRIG_TEMP) && ((int)termometr[cfg.stage[current_stage].czujnik].degC() >= cfg.stage[current_stage].koncowa_temperatura)) {
//        current_stage++;
//        sekundy = 0;
//        if (current_stage >= cfg.liczba_etapow) {
//          state = SELECT_PROGRAM;
//          //TODO;
//        }
//      }
      

      sprintf_P(menu_title, PSTR("Etap %i/%i, %s"), current_stage+1, cfg.liczba_etapow, time_str(sekundy));
      sprintf_P(menu_content, PSTR("T0:%i °C  T1:%i °C"), (int)termometr[0].degC(), (int)termometr[1].degC());
      sprintf_P(menu_content+40, PSTR("P: %i/%i  W: %i/%i"), cfg.stage[current_stage].praca_pompy, cfg.stage[current_stage].interwal_pompy, cfg.stage[current_stage].praca_wiatraka, cfg.stage[current_stage].interwal_wiatraka);
      if (cfg.stage[current_stage].end_trigger == STAGE_END_TRIG_TIME) {
        sprintf_P(menu_content+80, PSTR("K: po %i min."), cfg.stage[current_stage].czas_trwania);
        sprintf_P(menu_content+120, PSTR("DK: %s"), time_str(sekundy_do_konca));
      }
      else if (cfg.stage[current_stage].end_trigger == STAGE_END_TRIG_TEMP) {
        sprintf_P(menu_content+80, PSTR("K: T%i > %i °C + %i min."), cfg.stage[current_stage].czujnik, cfg.stage[current_stage].koncowa_temperatura, cfg.stage[current_stage].czas_trwania);
        sprintf_P(menu_content+120, PSTR("DK: %s"), (state == WEDZENIE_PERFORM) ? "Oczekiwanie" : time_str(sekundy_do_konca)); 
      }
      else if (cfg.stage[current_stage].end_trigger == STAGE_END_TRIG_NONE) {
        sprintf_P(menu_content+80, PSTR("K: recznie"));
        sprintf_P(menu_content+120, PSTR("DK: N/A"));
      }
      sprintf_P(menu_content+160, PSTR("<<           STOP           >>"));
      glcd.firstPage();
      do {
        glcd.setCursor(0,7);
        glcd.print(menu_title);
        glcd.setCursor(0,17);
        glcd.print(menu_content);
        glcd.setCursor(0,27);
        glcd.print(menu_content+40);
        glcd.setCursor(0,37);
        glcd.print(menu_content+80);        
        glcd.setCursor(0,47);
        glcd.print(menu_content+120);
        glcd.setCursor(0, 64);
        glcd.print(menu_content+160);
      } while (glcd.nextPage());

      timer = millis();
    }

    switch (glcd.getMenuEvent()) {

      case U8X8_MSG_GPIO_MENU_SELECT:
        sprintf_P(menu_title, PSTR("Przerwac?"));
        sprintf_P(menu_content, PSTR("TAK\nNIE"));
        selection = glcd.userInterfaceMessage(menu_title, "", "", menu_content);
        if (selection == 1) {
          state = SELECT_PROGRAM;
          Heater.stop();
          digitalWrite(WIATRAK_PIN, LOW);
          digitalWrite(POMPA_PIN, LOW);
          stop_all();
        }
      break;

      case U8X8_MSG_GPIO_MENU_PREV:
        sprintf_P(menu_title, PSTR("Poprzedni etap?"));
        sprintf_P(menu_content, PSTR("Tak\nNIE"));
        selection = glcd.userInterfaceMessage(menu_title, "", "", menu_content);
        if (selection == 1) {
          if (current_stage > 0) {
            current_stage--;
          }
          else {
            current_stage = cfg.liczba_etapow;
          }
          // TODO przejscie
        }
      break;

      case U8X8_MSG_GPIO_MENU_NEXT:
        sprintf_P(menu_title, PSTR("Nastepny etap?"));
        sprintf_P(menu_content, PSTR("Tak\nNIE"));
        selection = glcd.userInterfaceMessage(menu_title, "", "", menu_content);
        if (selection == 1) {
          current_stage++;
          if(current_stage >= cfg.liczba_etapow) current_stage=0;
          // TODO przejscie
        }
      break;

      default:

      break;
    }
  break;

/*  case WEDZENIE_PERFORM_WAIT_NEXT:
    if ((unsigned long)(millis()-timer) >= 1000) {
      timer = millis();

      sprintf_P(menu_title, PSTR("Oczekiwanie")); //TODO
      glcd.firstPage();
      do {
        
      } while (glcd.nextPage());
    }
  break; */

  case WARZENIE_PERFORM:
    termometr[0].read();
    termometr[1].read();
    termostat((int)termometr[0].degC(), cfg.warz_temp_wody);

    if ((int)termometr[1].degC() >= cfg.warz_temp_miesa) {
      //koncowa temperatura miesa zostala osiagnieta, zamykamy proces
      stop_all();
      state = SELECT_PROGRAM;
      digitalWrite(BEEP_PIN, 1);
      delay(1500);
      digitalWrite(BEEP_PIN, 0);
    }

    if ((unsigned long)(millis()-timer) >= 1000) {
      sprintf_P(menu_title, PSTR("Warzenie"));
      sprintf_P(menu_content, PSTR("T. wody: %i/%i °C"), (int)termometr[0].degC(), cfg.warz_temp_wody);
      sprintf_P(menu_content+40, PSTR("T. miesa:  %i/%i °C"), (int)termometr[1].degC(), cfg.warz_temp_miesa);
      sprintf_P(menu_content+160, PSTR("             STOP             "));
      glcd.firstPage();
      do {
        glcd.setCursor(0,7);
        glcd.print(menu_title);
        glcd.setCursor(0,17);
        glcd.print(menu_content);
        glcd.setCursor(0,27);
        glcd.print(menu_content+40);
        glcd.setCursor(0, 64);
        glcd.print(menu_content+160);
      } while (glcd.nextPage());
      timer = millis();
    }
    
    switch (glcd.getMenuEvent()) {
      case U8X8_MSG_GPIO_MENU_SELECT:
        sprintf_P(menu_title, PSTR("Przerwac?"));
        sprintf_P(menu_content, PSTR("TAK\nNIE"));
        selection = glcd.userInterfaceMessage(menu_title, "", "", menu_content);
        if (selection == 1) {
          state = SELECT_PROGRAM;
          stop_all();
        }
      break;
    }  
  break;
  
	
    
  default:

  break;
  }

}
