/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "app.h"
#include "app_log.h"
#include "temperature.h"
#include "gatt_db.h"
#include "sl_simple_led_instances.h"
#include "sl_simple_led.h"



#define TEMPERATURE_TIMER_SIGNAL (1 << 0)

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

static void timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data);
static sl_sleeptimer_timer_handle_t timer_handle;
static uint32_t timer_step = 0;

static uint8_t active_connection = 0xff;


/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
  app_log_info("%s\n", __FUNCTION__);
  sl_simple_led_init_instances();
  app_log_info("Application demarree\n");
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);
      // Start advertising and enable connections.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      app_log_info("%s: connection_opened\n", __FUNCTION__);
      active_connection = evt->data.evt_connection_opened.connection;

      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      app_log_info("%s: connection_closed\n", __FUNCTION__);

      active_connection = 0xff;

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);
      // Restart advertising after client has disconnected.
      sc = sl_bt_legacy_advertiser_start(
          advertising_set_handle,
          sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

      // This event indicates
    case sl_bt_evt_gatt_server_user_read_request_id:
      app_log_info("%s: read request received\n", __FUNCTION__);

       if (evt->data.evt_gatt_server_user_read_request.characteristic == gattdb_temperature) {
         app_log_info("Lecture de la temperature demandee.\n");

         // Lire et convertir la température
         int16_t temp_ble;
         sl_status_t sc = read_temperature_ble_format(&temp_ble);

         if (sc == SL_STATUS_OK)
           {
           // Réponse BLE : envoyer la température
           sc = sl_bt_gatt_server_send_user_read_response(
             evt->data.evt_gatt_server_user_read_request.connection, // ID connexion
             gattdb_temperature,                                     // ID caractéristique
             0,                                                      // Aucun code d'erreur ATT
             sizeof(temp_ble),                                       // Taille des données : 2 octets
             (uint8_t *)&temp_ble,                                   // Valeur formatée
             NULL                                                    // Longueur réellement envoyée (non utilisée)
           );

           app_log_info("Temperature envoyee (BLE): %d (0.01°C format)\n", temp_ble);
         }
         else {
           app_log_error("Erreur lecture temperature: 0x%lx\n", (unsigned long)sc);
         }
       }
       break;

    case sl_bt_evt_gatt_server_characteristic_status_id:
        app_log_info("Changement de configuration CCCD reCcu.\n");

        sl_bt_evt_gatt_server_characteristic_status_t *status =
          &evt->data.evt_gatt_server_characteristic_status;

        if (status->characteristic == gattdb_temperature &&
            status->status_flags == sl_bt_gatt_server_client_config)
          {

            app_log_info("Modification détectee pour temperature.\n");
            app_log_info("Valeur de client_config_flags : 0x%04X\n", status->client_config_flags);

            if (status->client_config_flags == sl_bt_gatt_server_notification)
              {
              app_log_info("Notification activee pour temperature.\n");

              sl_status_t sc = sl_sleeptimer_start_periodic_timer_ms(
                      &timer_handle,
                      1000,             // 1 seconde
                      timer_callback,
                      NULL,
                      0,
                      0
                    );
                    app_log_status_f(sc, "Timer demarre\n");

            }
            else if (status->client_config_flags == sl_bt_gatt_server_disable)
              {
              app_log_info("Notification desactivee pour temperature.\n");

              // Arrêter le timer
                   sl_status_t sc = sl_sleeptimer_stop_timer(&timer_handle);
                   app_log_status_f(sc, "Timer arretee\n");

                   timer_step = 0;
            }
            else
              {
              app_log_info("Autre configuration CCCD inconnue : 0x%04X\n", status->client_config_flags);
            }
        }
        break;

    case sl_bt_evt_system_external_signal_id:
      if (evt->data.evt_system_external_signal.extsignals & TEMPERATURE_TIMER_SIGNAL) {
        int16_t temp_ble;
        sl_status_t sc = read_temperature_ble_format((int16_t *)&temp_ble);
        if (sc == SL_STATUS_OK && active_connection != 0xff) {
          sc = sl_bt_gatt_server_send_notification(
            active_connection,
            gattdb_temperature,
            sizeof(temp_ble),
            (uint8_t *)&temp_ble
          );
          app_log_info("Notification envoyee: %d (0.01°C format)\n", temp_ble);
        }
        else
          {
          app_log_error("Erreur envoi notification: 0x%lx\n", (unsigned long)sc);
        }
      }
      break;


    case sl_bt_evt_gatt_server_user_write_request_id:
      if (evt->data.evt_gatt_server_user_write_request.characteristic == gattdb_aio_digital_in) {
          const uint8_t value = evt->data.evt_gatt_server_user_write_request.value.data[0];

          app_log_info("Commande LED recue : %d\n", value);

          for (uint8_t i = 0; i < SL_SIMPLE_LED_COUNT; i++) {
            if (value) {
                sl_simple_led_turn_on((sl_led_t *)sl_simple_led_array[i]);
            } else {
                sl_simple_led_turn_off((sl_led_t *)sl_simple_led_array[i]);

            }
          }

          sl_bt_gatt_server_send_user_write_response(
              evt->data.evt_gatt_server_user_write_request.connection,
              evt->data.evt_gatt_server_user_write_request.characteristic,
              0
          );
        }
        break;




    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}

static void timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  sl_bt_external_signal(TEMPERATURE_TIMER_SIGNAL);
}

