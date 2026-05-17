/**
 * @file program.cpp
 * @brief Programme principal de la cabine d'ascenseur - Aéroport de Mérignac
 * 
 * Gère la machine à états de l'ascenseur sur 6 étages.
 * Etats : OPENED, CLOSING, MOVING, OPENING, STOPPED, FORCE_OPEN, FORCE_CLOSE, OVERLOAD
 */

#include "floor.h"
#include "cabin.h"

// Durées en millisecondes
#define TIME_OPENED       6000  // Temps d'attente portes ouvertes (sans présence)
#define TIME_OPENED_PRES 12000  // Temps d'attente augmenté si passager détecté
#define TIME_DOORS        1700  // Durée d'ouverture/fermeture des portes
#define TIME_FLOOR_SHORT  7150  // Durée de déplacement court entre étages
#define TIME_FLOOR_LONG   7700  // Durée de déplacement long entre étages

// Seuils des capteurs analogiques (0-1023)
#define PRESENCE_THRESHOLD 500  // En dessous = passager détecté dans la cabine
#define WEIGHT_THRESHOLD   512  // Au dessus = surcharge (équivalent ~250kg)

// Nombre d'étages du bâtiment
#define FLOOR_NUM 6

/**
 * @brief Configuration des 6 étages de l'aéroport
 * Chaque étage contient : titre, touche clavier, valeur afficheur,
 * étage par défaut, LEDs haut/bas, boutons haut/bas, état boutons
 */
floor_info building[FLOOR_NUM] = {
  //                                --led--   --btn--
  // title          key disp  def   up  down  up   down  pressed
  { "Parking   " , '*', -1, false ,  9,   0,  84,   -1,   0 },
  { "Hall      " , '0',  0, true  , 11,  10, 101,   93,   0 },
  { "Departures" , '1',  1, false , 13,  12, 118,  110,   0 },
  { "Arrivals  " , '2',  2, false , 15,  14, 133,  126,   0 },
  { "Terminal 1" , '3',  3, false , 17,  16, 149,  141,   0 },
  { "Terminal 2" , '4',  4, false ,  0,  18,  -1,  156,   0 }
};

/**
 * @brief Etats possibles de la machine à états de l'ascenseur
 */
enum states {
  STATE_OPENED,      // Portes ouvertes, en attente
  STATE_MOVING,      // Cabine en déplacement
  STATE_OPENING,     // Portes en cours d'ouverture
  STATE_CLOSING,     // Portes en cours de fermeture
  STATE_STOPPED,     // Arrêt d'urgence (bouton STOP)
  STATE_FORCE_OPEN,  // Forçage ouverture portes (bouton <>)
  STATE_FORCE_CLOSE, // Forçage fermeture portes (bouton ><)
  STATE_OVERLOAD     // Surcharge de poids détectée
};

// Variables globales de la machine à états
states state = STATE_OPENED; // État courant
timer_ms timer;              // Timer non-bloquant
int target = -1;             // Étage cible (-1 = aucun)
bool stopped = false;        // Flag arrêt d'urgence

unsigned long movetime();

/**
 * @brief Initialisation : démarre la communication série,
 * initialise les étages et la cabine
 */
void setup() {
  Serial.begin(9600);
  cabin_init(
    floor_init(building, FLOOR_NUM)
  );
}

/**
 * @brief Boucle principale : lit les capteurs/boutons,
 * gère la machine à états et met à jour l'affichage
 */
void loop() {
  const char* status = nullptr;

  // Lecture des boutons d'étage et du pavé numérique
  floor_readbtns();

  // Lecture des capteurs analogiques
  int presence = analogRead(PIN_PRESENCE); // Capteur photorésistance
  int pressure = analogRead(PIN_PRESSURE); // Capteur de force (poids)
  bool someone_present = presence < PRESENCE_THRESHOLD; // Passager détecté
  bool overload = pressure > WEIGHT_THRESHOLD;           // Surcharge détectée

  // Gestion surcharge : portes restent ouvertes tant que le poids dépasse le seuil
  if(overload && state != STATE_STOPPED) {
    cabin_door(CABIN_DOOR_OPEN);
    state = STATE_OVERLOAD;
  }
  if(state == STATE_OVERLOAD && !overload) {
    cabin_door(CABIN_DOOR_STOP);
    state = STATE_OPENED;
  }
  if(state == STATE_OVERLOAD) {
    floor_set_direction(0);
    floor_feedback(cabin_current_floor(), "(SURCHARGE!!)  ");
    return;
  }

  // Gestion bouton STOP : bascule entre arrêt et reprise
  if(floor_stop_pressed()) {
    stopped = !stopped;
    if(stopped) {
      cabin_stop();
      floor_set_direction(0);
      state = STATE_STOPPED;
    } else {
      state = STATE_OPENED;
    }
  }
  if(state == STATE_STOPPED) {
    floor_feedback(cabin_current_floor(), "(STOP)         ");
    return;
  }

  // Gestion boutons forçage portes (uniquement en état OPENED)
  if(state == STATE_OPENED) {
    if(floor_open_pressed()) {
      cabin_door(CABIN_DOOR_OPEN);
      state = STATE_FORCE_OPEN;
    }
    else if(floor_close_pressed()) {
      cabin_door(CABIN_DOOR_CLOSE);
      state = STATE_FORCE_CLOSE;
    }
  }

  // Temporisation adaptée selon présence d'un passager
  unsigned long time_opened = someone_present ? TIME_OPENED_PRES : TIME_OPENED;

  // Machine à états principale
  switch(state) {
    case STATE_OPENED:
      floor_set_direction(0);
      target = floor_requested(cabin_current_floor()); // Cherche prochain étage
      status = someone_present ? "(someone here) " : "(waiting...)   ";
      if(timer_elapsed(timer, time_opened) && target>=0) {
        cabin_door(CABIN_DOOR_CLOSE);
        state = STATE_CLOSING;
      }
      break;

    case STATE_FORCE_OPEN:
      floor_set_direction(0);
      status = "(door open)    ";
      if(floor_close_pressed() || floor_stop_pressed()) {
        cabin_door(CABIN_DOOR_STOP);
        state = STATE_OPENED;
      }
      break;

    case STATE_FORCE_CLOSE:
      floor_set_direction(0);
      status = "(door close)   ";
      if(floor_open_pressed() || floor_stop_pressed()) {
        cabin_door(CABIN_DOOR_STOP);
        state = STATE_OPENED;
      }
      break;

    case STATE_CLOSING:
      floor_set_direction(0);
      cabin_door(CABIN_DOOR_CLOSE);
      status = "(closing doors)";
      if(timer_elapsed(timer, TIME_DOORS)) {
        cabin_door(CABIN_DOOR_STOP);
        state = STATE_MOVING;
      }
      break;

    case STATE_MOVING:
      // Direction : 1 = monte, -1 = descend (pour animation 7 segments)
      floor_set_direction(target > cabin_current_floor() ? 1 : -1);
      status = "(moving)       ";
      if(cabin_move(timer, target, movetime()) == target) {
        cabin_stop();
        floor_set_direction(0);
        state = STATE_OPENING;
      }
      break;

    case STATE_OPENING:
      floor_set_direction(0);
      status = "(opening doors)";
      cabin_door(CABIN_DOOR_OPEN);
      if(timer_elapsed(timer, TIME_DOORS)) {
        cabin_door(CABIN_DOOR_STOP);
        state = STATE_OPENED;  
      }
      break;

    default:
      break;
  }

  // Mise à jour LCD et LEDs
  floor_feedback(cabin_current_floor(), status);
}

/**
 * @brief Calcule la durée de déplacement entre deux étages
 * La durée varie selon la parité de l'étage courant et la direction,
 * pour simuler des distances variables entre niveaux
 * @return Durée en millisecondes
 */
unsigned long movetime() {
  auto cur = cabin_current_floor();
  auto odd = (cur % 2) != 0;

  if(target < cur && odd || target > cur && !odd) {
    return TIME_FLOOR_SHORT;
  }
  else {
    return TIME_FLOOR_LONG;
  }
}