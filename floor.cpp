/**
 * @file floor.cpp
 * @brief Module de gestion des étages - Aéroport de Mérignac
 *
 * Gère les boutons d'appel, les LEDs NeoPixel, les afficheurs 7 segments,
 * le LCD de la cabine et la détection des boutons spéciaux (STOP, <>, ><).
 */

#include <LiquidCrystal_I2C.h>
#include <Adafruit_NeoPixel.h>
#include <Keypad.h>
#include "timer.h"

#include "floor.h"

// Masques binaires pour l'état des boutons de chaque étage
#define BTN_UP    0b0001  // Bouton monter appuyé
#define BTN_DOWN  0b0010  // Bouton descendre appuyé
#define BTN_PANEL 0b0100  // Bouton cabine appuyé

// Macro pour tester si un flag est actif dans un masque
#define IS_PRESSED(val, flag) (((val)&(flag))==(flag))

// Configuration du pavé numérique (resistor ladder)
static byte rows[] = PIN_KEYMAP_ROWS;
static byte cols[] = PIN_KEYMAP_COLS;

// Objets matériels
static Keypad _keys(const_cast<char*>(KEYMAP), rows, cols, sizeof(rows)/sizeof(byte), sizeof(cols)/sizeof(byte));
static Adafruit_NeoPixel _leds(MAX_LEDS, PIN_LEDS, NEO_GRB + NEO_KHZ800);
static LiquidCrystal_I2C _lcd(I2C_LCD , MAX_LCD_COLS, 2);

// Pointeur vers le tableau des étages et sa taille
static floor_info *_floors;
static size_t _floors_size;

// Flags des boutons spéciaux (lus dans floor_readbtns, consommés dans program.cpp)
static bool _stop_pressed  = false; // Bouton STOP (touche D)
static bool _open_pressed  = false; // Bouton ouvrir portes (touche B)
static bool _close_pressed = false; // Bouton fermer portes (touche C)

// Direction actuelle de la cabine pour les animations 7 segments
static int _direction = 0; // -1 = descend, 0 = arrêt, 1 = monte

// Déclarations des fonctions internes
static bool ladder_pressed(int expected, int actual);
static void neopixel_switch(int led, bool on);

/**
 * @brief Initialise tous les composants matériels des étages
 * - Démarre chaque afficheur 7 segments via I2C
 * - Initialise le LCD, les LEDs NeoPixel
 * - Retourne l'index de l'étage de départ (marqué def=true)
 */
int floor_init(floor_info floors[], size_t floor_num) {
  int start = 0;

  _floors = floors;
  _floors_size = floor_num;
  for(int i=0; i<_floors_size; i++) {
    // Chaque afficheur a une adresse I2C unique : 0x70 + i
    _floors[i].upper7seg.begin(I2C_7SEG_BASE + i);
    if(_floors[i].def) {
      start = i; // Mémorise l'étage de départ
    }
  }
  _lcd.init();
  _lcd.backlight();
  _leds.begin();
  floor_feedback(start);
  return start;
}

/**
 * @brief Met à jour la direction de la cabine pour les animations
 * @param dir 1=monte, -1=descend, 0=arrêt
 */
void floor_set_direction(int dir) {
  _direction = dir;
}

/**
 * @brief Met à jour tous les affichages à intervalle régulier
 * - LCD : nom de l'étage courant + statut
 * - LEDs NeoPixel : feedback des boutons appuyés
 * - Afficheurs 7 segments : numéro de l'étage + animation direction
 * 
 * Utilise un timer pour ne pas rafraîchir trop souvent (FLOOR_FEEDBACK_PERIOD)
 * Efface le LCD uniquement si l'étage ou le statut change (évite le scintillement)
 */
void floor_feedback(int current, const char* status) {
  static timer_ms refresh = 0L;
  static int last_floor = -1;
  static const char* last_status = nullptr;
  
  // Rafraîchissement limité dans le temps
  if(!timer_elapsed(refresh, FLOOR_FEEDBACK_PERIOD)) {
    return;
  }

  // Efface le LCD uniquement si l'étage ou le statut a changé
  if(current != last_floor || status != last_status) {
    _lcd.clear();
    last_floor = current;
    last_status = status;
  }

  // Affiche le nom de l'étage sur la première ligne du LCD
  _lcd.setCursor(0, 0);
  _lcd.print(_floors[current].title);

  // Affiche le statut sur la deuxième ligne si disponible
  if(status) {
    _lcd.setCursor(0, 1);
    _lcd.print(status);
  }

  // Mise à jour de chaque étage
  for(int i=0; i<_floors_size; i++) {
    floor_info& info = _floors[i];
    auto btns = info.pressed;

    // LEDs NeoPixel : allumées si le bouton correspondant est actif
    neopixel_switch(info.ledUp  , IS_PRESSED(btns, BTN_UP));
    neopixel_switch(info.ledDown, IS_PRESSED(btns, BTN_DOWN));
    neopixel_switch(i           , IS_PRESSED(btns, BTN_PANEL));

    // Animation 7 segments selon la direction de la cabine
    if(_direction == 1) {
      // Montée : affiche le numéro + segment haut allumé (flèche montante)
      info.upper7seg.printNumber(_floors[current].displayVal, 10);
      info.upper7seg.writeDigitRaw(4, 0x01);
    } else if(_direction == -1) {
      // Descente : affiche le numéro + segment bas allumé (flèche descendante)
      info.upper7seg.printNumber(_floors[current].displayVal, 10);
      info.upper7seg.writeDigitRaw(4, 0x08);
    } else {
      // Arrêt : affiche uniquement le numéro de l'étage
      info.upper7seg.printNumber(_floors[current].displayVal, 10);
    }
    info.upper7seg.writeDisplay();
  }
  _leds.show();
}

/**
 * @brief Lit tous les boutons d'appel et du pavé numérique
 * - Resistor ladder : un seul pin analogique pour tous les boutons d'étage
 * - Pavé numérique : touches D=STOP, B=ouvrir, C=fermer
 * - Met à jour les flags pressed de chaque étage
 */
void floor_readbtns() {
  int btn = analogRead(PIN_CALLBTNS); // Lecture de la resistor ladder
  char key = _keys.getKey();          // Lecture du pavé numérique
  
  for(int i=0; i < _floors_size; i++) {
    // Vérifie si une touche du pavé correspond à cet étage
    if(_floors[i].key == key) {
      _floors[i].pressed |= BTN_PANEL;
    }
    // Boutons spéciaux du panneau
    if(key == 'D') _stop_pressed  = true; // Touche D = STOP
    if(key == 'B') _open_pressed  = true; // Touche B = <>
    if(key == 'C') _close_pressed = true; // Touche C = >

    // Détection des boutons d'étage via la resistor ladder
    if(ladder_pressed(_floors[i].callDown, btn)) {
      _floors[i].pressed |= BTN_DOWN;
    }
    else if(ladder_pressed(_floors[i].callUp, btn)) {
      _floors[i].pressed |= BTN_UP;
    }
  }
}

/**
 * @brief Détermine le prochain étage à desservir
 * Parcourt les étages depuis l'étage courant dans l'ordre circulaire.
 * Efface les demandes de l'étage courant à l'arrivée.
 * @param from Étage courant
 * @return Index du prochain étage demandé, ou -1 si aucun
 */
int floor_requested(int from) {
  _floors[from].pressed = 0; // Efface les demandes de l'étage courant
  for(int i = 0; i < _floors_size; i++) {
    int index = (from + i) % _floors_size; // Parcours circulaire
    if(_floors[index].pressed) {
      return index;
    }
  }
  return -1; // Aucune demande en attente
}

// Fonctions de lecture des boutons spéciaux
// Chaque fonction retourne true une seule fois puis remet le flag à false

bool floor_stop_pressed() {
  bool result = _stop_pressed;
  _stop_pressed = false;
  return result;
}

bool floor_open_pressed() {
  bool result = _open_pressed;
  _open_pressed = false;
  return result;
}

bool floor_close_pressed() {
  bool result = _close_pressed;
  _close_pressed = false;
  return result;
}

/**
 * @brief Détecte si une valeur analogique correspond à un bouton de la resistor ladder
 * Chaque bouton a une résistance différente → valeur analogique unique
 * On accepte une tolérance de ±5 pour compenser le bruit
 * @param expected Valeur analogique attendue pour ce bouton
 * @param actual Valeur lue sur le pin analogique
 * @return true si le bouton est pressé
 */
bool ladder_pressed(int expected, int actual) {
  return expected != -1 
      && expected-5 < actual && actual < expected+5;
}

/**
 * @brief Allume ou éteint une LED NeoPixel
 * Allumée = orange (255, 64, 0), Éteinte = noir (0, 0, 0)
 * @param led Numéro de la LED
 * @param on true = allumée, false = éteinte
 */
void neopixel_switch(int led, bool on) {
  uint32_t color;
  if(on) {
    color = Adafruit_NeoPixel::Color(255, 64, 0); // Orange
  }
  else {
    color = Adafruit_NeoPixel::Color(0, 0, 0);   // Éteint
  }
  _leds.setPixelColor(led, color);
}