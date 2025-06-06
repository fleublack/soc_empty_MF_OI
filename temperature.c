/*
 * temperature.c
 *
 *  Created on: 6 juin 2025
 *      Author: fleury
 */
#include "temperature.h"
#include "sl_sensor_rht.h"
#include "app_log.h"

sl_status_t read_temperature_ble_format(int16_t *temp_ble_value)
{
  sl_status_t sc;
  uint32_t rh_unused;
  int32_t temp_mC;

  // Initialisation du capteur
  sc = sl_sensor_rht_init();
  if (sc != SL_STATUS_OK) {
    app_log_error("Erreur init capteur: 0x%lx\n", (unsigned long)sc);
    return sc;
  }

  // Lecture température
  sc = sl_sensor_rht_get(&rh_unused, &temp_mC);
  if (sc != SL_STATUS_OK) {
    app_log_error("Erreur lecture temperature: 0x%lx\n", (unsigned long)sc);
    sl_sensor_rht_deinit();
    return sc;
  }

  sl_sensor_rht_deinit();

  *temp_ble_value = (int16_t)(temp_mC / 10); // conversion milli°C → centi°C
  return SL_STATUS_OK;
}




