/*
 * temperature.h
 *
 *  Created on: 6 juin 2025
 *      Author: fleury
 */

#ifndef TEMPERATURE_H_
#define TEMPERATURE_H_

#include <stdint.h>
#include "sl_status.h"

/**
 * @brief Lit la température du capteur et la convertit au format BLE.
 *
 * @param[out] buffer Tableau de 2 octets (little endian) contenant la température
 * @return sl_status_t Code de retour (SL_STATUS_OK si tout s’est bien passé)
 */
sl_status_t read_temperature_ble_format(int16_t *temp_ble_value);


#endif /* TEMPERATURE_H_ */
