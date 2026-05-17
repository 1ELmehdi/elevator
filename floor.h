/**
 * @file floor.h
 * @brief Module de gestion des étages - Aéroport de Mérignac
 * 
 * Déclare les constantes, la structure floor_info et toutes les fonctions
 * du module floor utilisées par program.cpp.
 */
#ifndef _FLOOR_H
#define _FLOOR_H

#include <Adafruit_LEDBackpack.h>

// Période de rafraîchissement des affichages en ms (100ms = 10 fois/seconde)
#define FLOOR_FEEDBACK_PERIOD 100 

// Adresses I2C des composants
#define I2C_LCD       0x20  // Adresse du LCD 16x2 de la cabine
#define I2C_7SEG_BASE 0x70  // Adresse de base des afficheurs 7 segments (0x70 à 0x75)

// Broches Arduino
#define PIN_LEDS      15    // Bande NeoPixel
#define PIN_CALLBTNS  A0    // Resistor ladder (boutons d'appel)
#define PIN_PRESSURE  16    // Capteur de force (poids)
#define PIN_PRESENCE  17    // Photorésistance (présence dans la cabine)

// Configuration du pavé numérique 4x4
#define PIN_KEYMAP_ROWS { 13, 12, 11, 10 }
#define PIN_KEYMAP_COLS {  9,  8,  7,  6 }

// Mappage des touches : 
// Lignes 1-4 → touches, Colonne D : B=ouvrir, C=fermer, D=STOP
#define KEYMAP "123A" "456B" "789C" "*0#D"

// Limites des tableaux
#define MAX_LEDS         19  // Total LEDs NeoPixel (0-8 cabine + 9-18 étages)
#define MAX_LCD_COLS     16  // Largeur du LCD en caractères
#define MAX_FLOOR_TITLE  MAX_LCD_COLS+1  // Longueur max du titre d'étage

/**
 * @struct floor_info
 * @brief Configuration et état d'un étage
 * 
 * Contient toutes les informations nécessaires pour gérer un étage :
 * affichage, boutons, LEDs et état courant.
 */
struct floor_info {
  char title[MAX_FLOOR_TITLE]; // Nom affiché sur le LCD (ex: "Hall      ")
  char key;                    // Touche du pavé numérique pour cet étage
  int displayVal;              // Valeur affichée sur le 7 segments (ex: -1, 0, 1...)
  bool def;                    // true = étage de départ par défaut
  int ledUp, ledDown;          // Numéros des LEDs NeoPixel haut/bas
  int callUp, callDown;        // Valeurs analogiques des boutons d'appel (resistor ladder)
  int pressed;                 // Masque des boutons pressés (BTN_UP | BTN_DOWN | BTN_PANEL)
  Adafruit_7segment upper7seg; // Afficheur 7 segments au-dessus de la porte
};

/**
 * @brief Initialise les composants matériels de tous les étages
 * @param floors Tableau des étages
 * @param floor_num Nombre d'étages
 * @return Index de l'étage de départ (def=true)
 */
int  floor_init        (floor_info floors[], size_t floor_num);

/**
 * @brief Lit tous les boutons d'appel et du pavé numérique
 * Met à jour les flags pressed de chaque étage et les boutons spéciaux
 */
void floor_readbtns    ();

/**
 * @brief Met à jour le LCD, les LEDs et les afficheurs 7 segments
 * @param current Index de l'étage courant
 * @param status Message optionnel pour la 2ème ligne du LCD
 */
void floor_feedback    (int current, const char *status = nullptr);

/**
 * @brief Détermine le prochain étage à desservir
 * @param from Étage courant
 * @return Index du prochain étage demandé, ou -1 si aucun
 */
int  floor_requested   (int from);

/**
 * @brief Vérifie si le bouton STOP a été pressé (touche D)
 * @return true une seule fois après chaque appui
 */
bool floor_stop_pressed();

/**
 * @brief Vérifie si le bouton ouvrir portes a été pressé (touche B = <>)
 * @return true une seule fois après chaque appui
 */
bool floor_open_pressed();

/**
 * @brief Vérifie si le bouton fermer portes a été pressé (touche C = ><)
 * @return true une seule fois après chaque appui
 */
bool floor_close_pressed();

/**
 * @brief Définit la direction de la cabine pour les animations 7 segments
 * @param dir 1 = monte, -1 = descend, 0 = arrêt
 */
void floor_set_direction(int dir);

#endif