/**
 * @file timer.cpp
 * @brief Implémentation des timers non-bloquants
 * 
 * Permet de gérer des temporisations sans utiliser delay(),
 * ce qui est essentiel pour que la boucle loop() reste réactive.
 * Utilisé pour les portes, les déplacements et le rafraîchissement LCD.
 */

#include <wino.h>
#include "timer.h"

/**
 * @brief Réinitialise le timer au temps actuel
 * Enregistre la valeur actuelle de millis() comme point de départ
 * @param timer Référence au timer à réinitialiser
 */
void timer_reset(timer_ms& timer) { 
  timer = millis();
}

/**
 * @brief Vérifie si la durée limite est écoulée
 * 
 * Compare le temps écoulé depuis le dernier reset avec la limite.
 * Si la limite est atteinte, remet le timer à zéro automatiquement.
 * 
 * Exemple d'utilisation :
 *   if(timer_elapsed(timer, 2000)) { // 2 secondes écoulées
 *     // faire quelque chose
 *   }
 * 
 * @param timer Référence au timer à vérifier
 * @param limit Durée en millisecondes à attendre
 * @return true si la durée est écoulée (et reset automatique)
 * @return false si la durée n'est pas encore écoulée
 */
bool timer_elapsed(timer_ms& timer, unsigned long limit) {
  // Calcule le temps écoulé depuis le dernier reset
  if(millis() - timer < limit) {
    return false; // Pas encore écoulé
  }
  timer_reset(timer); // Reset automatique
  return true;        // Durée écoulée
}