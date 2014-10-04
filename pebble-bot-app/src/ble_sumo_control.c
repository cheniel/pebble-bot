#include "ble_sumo_control.h"

typedef struct {
  BTDevice device;
  Window *window;

  TextLayer *info_text_layer;
  char info_text_buffer[64];

  BLECharacteristic scratch1_characteristic;
  BLECharacteristic scratch2_characteristic;
} SumoControlCtx;

static SumoControlCtx s_ctx;

static void ready(void);

//------------------------------------------------------------------------------
// BLE callbacks:

static void service_change_handler(BTDevice device,
                                   const BLEService services[],
                                   uint8_t num_services,
                                   BTErrno status) {
  // Invalidate any old references:
  s_ctx.scratch1_characteristic = BLE_CHARACTERISTIC_INVALID;
  s_ctx.scratch2_characteristic = BLE_CHARACTERISTIC_INVALID;

  // Iterate through the found services:
  for (uint8_t i = 0; i < num_services; ++i) {
    Uuid service_uuid = ble_service_get_uuid(services[i]);

    // Compare with the Bean "Scratch Service" UUID:
    const Uuid bean_scratch_service_uuid =
    UuidMake(0xa4, 0x95, 0xff, 0x20, 0xc5, 0xb1, 0x4b, 0x44,
             0xb5, 0x12, 0x13, 0x70, 0xf0, 0x2d, 0x74, 0xde);
    if (!uuid_equal(&service_uuid, &bean_scratch_service_uuid)) {
      // Not the Bean's "Scratch Service"
      continue;
    }

    char uuid_buffer[UUID_STRING_BUFFER_LENGTH];
    uuid_to_string(&service_uuid, uuid_buffer);
    const BTDeviceAddress address = bt_device_get_address(device);
    APP_LOG(APP_LOG_LEVEL_INFO,
            "Discovered Bean Scratch service %s (0x%08x) on " BT_DEVICE_ADDRESS_FMT,
            uuid_buffer,
            services[i],
            BT_DEVICE_ADDRESS_XPLODE(address));


    // Iterate over the characteristics within the "Scratch Service":
    BLECharacteristic characteristics[8];
    uint8_t num_characteristics =
    ble_service_get_characteristics(services[i], characteristics, 8);
    if (num_characteristics > 8) {
      num_characteristics = 8;
    }
    for (uint8_t c = 0; c < num_characteristics; ++c) {
      Uuid characteristic_uuid = ble_characteristic_get_uuid(characteristics[c]);

      // The characteristic UUIDs we're looking for:
      const Uuid bean_scratch_char1_uuid =
            UuidMake(0xa4, 0x95, 0xff, 0x21, 0xc5, 0xb1, 0x4b, 0x44,
                     0xb5, 0x12, 0x13, 0x70, 0xf0, 0x2d, 0x74, 0xde);
      const Uuid bean_scratch_char2_uuid =
            UuidMake(0xa4, 0x95, 0xff, 0x22, 0xc5, 0xb1, 0x4b, 0x44,
                     0xb5, 0x12, 0x13, 0x70, 0xf0, 0x2d, 0x74, 0xde);

      uint8_t scratch_num = 0; // Just for logging purposes
      if (uuid_equal(&characteristic_uuid, &bean_scratch_char1_uuid)) {
        // Found Bean's "Scratch1"
        s_ctx.scratch1_characteristic = characteristics[c];
        scratch_num = 1;
      } else if (uuid_equal(&characteristic_uuid, &bean_scratch_char2_uuid)) {
        // Found Bean's "Scratch2"
        s_ctx.scratch2_characteristic = characteristics[c];
        scratch_num = 2;
      } else {
        continue;
      }

      uuid_to_string(&characteristic_uuid, uuid_buffer);
      APP_LOG(APP_LOG_LEVEL_INFO, "-- Found Scratch%u: %s (0x%08x)",
              scratch_num, uuid_buffer, characteristics[c]);

      // Check if Scratch1 and Scratch2 are found:
      if (s_ctx.scratch1_characteristic != BLE_CHARACTERISTIC_INVALID &&
          s_ctx.scratch2_characteristic != BLE_CHARACTERISTIC_INVALID) {
        ready();
      }
    }
  }
}

static void connection_handler(BTDevice device, BTErrno connection_status) {
  const BTDeviceAddress address = bt_device_get_address(device);

  const bool connected = (connection_status == BTErrnoConnected);

  snprintf(s_ctx.info_text_buffer, sizeof(s_ctx.info_text_buffer),
           "%s " BT_DEVICE_ADDRESS_FMT " (status=%d)",
          connected ? "Connected.\nDiscovering services..." : "Disconnected",
          BT_DEVICE_ADDRESS_XPLODE(address), connection_status);
  text_layer_set_text(s_ctx.info_text_layer, s_ctx.info_text_buffer);

  if (connected) {
    ble_client_discover_services_and_characteristics(device);
  }
}

//------------------------------------------------------------------------------
// BLE Helpers:

void ble_sumo_control_set_device(BTDevice device) {
  s_ctx.device = device;
}

static void connect(void) {
  BTErrno e = ble_central_connect(s_ctx.device,
                                  true /* auto_reconnect */,
                                  false /* is_pairing_required */);
  if (e) {
    snprintf(s_ctx.info_text_buffer, sizeof(s_ctx.info_text_buffer), "Error Connecting: %d", e);
    text_layer_set_text(s_ctx.info_text_layer, s_ctx.info_text_buffer);
  } else {
    text_layer_set_text(s_ctx.info_text_layer, "Connecting...");
  }
}

static void disconnect(void) {
  BTErrno e = ble_central_cancel_connect(s_ctx.device);
  if (e) {
    snprintf(s_ctx.info_text_buffer, sizeof(s_ctx.info_text_buffer), "Error Disconnecting: %d", e);
    text_layer_set_text(s_ctx.info_text_layer, s_ctx.info_text_buffer);
  } else {
    text_layer_set_text(s_ctx.info_text_layer, "Disconnecting...");
  }
}

static void ready(void) {
  const BTDeviceAddress address = bt_device_get_address(s_ctx.device);
  snprintf(s_ctx.info_text_buffer, sizeof(s_ctx.info_text_buffer),
           "Connected to: " BT_DEVICE_ADDRESS_FMT " \n\n\nReady to Rumble!",
           BT_DEVICE_ADDRESS_XPLODE(address));
  text_layer_set_text(s_ctx.info_text_layer, s_ctx.info_text_buffer);
}

//------------------------------------------------------------------------------
// Window callbacks:

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);

  s_ctx.info_text_layer = text_layer_create(bounds);
  layer_add_child(window_layer, text_layer_get_layer(s_ctx.info_text_layer));

  // Set up handlers:
  ble_central_set_connection_handler(connection_handler);
  ble_client_set_service_change_handler(service_change_handler);
}

static void window_unload(Window *window) {
  text_layer_destroy(s_ctx.info_text_layer);
  s_ctx.info_text_layer = NULL;

  window_destroy(window);
  s_ctx.window = NULL;
}

static void window_disappear(Window *window) {
  disconnect();
}

static void window_appear(Window *window) {
  connect();
}

//------------------------------------------------------------------------------
// Button -> Characteristic write glue code

static const uint16_t SERVO_BACK = 180;
static const uint16_t SERVO_STILL = 90;
static const uint16_t SERVO_FWD = 0;

static void handle_up_button_up(ClickRecognizerRef recognizer, void *context) {
  ble_client_write_without_response(s_ctx.scratch1_characteristic,
                                    (const uint8_t *) &SERVO_STILL, sizeof(SERVO_STILL));
  printf("UP=0");
}

static void handle_up_button_down(ClickRecognizerRef recognizer, void *context) {
  ble_client_write_without_response(s_ctx.scratch1_characteristic,
                                    (const uint8_t *) &SERVO_BACK, sizeof(SERVO_BACK));
  printf("UP=1");
}

static void handle_down_button_up(ClickRecognizerRef recognizer, void *context) {
  ble_client_write_without_response(s_ctx.scratch2_characteristic,
                                    (const uint8_t *) &SERVO_STILL, sizeof(SERVO_STILL));
  printf("DOWN=0");
}

static void handle_down_button_down(ClickRecognizerRef recognizer, void *context) {
  ble_client_write_without_response(s_ctx.scratch2_characteristic,
                                    (const uint8_t *) &SERVO_FWD, sizeof(SERVO_FWD));
  printf("DOWN=1");
}

static void handle_select_button_up(ClickRecognizerRef recognizer, void *context) {
  ble_client_write_without_response(s_ctx.scratch1_characteristic,
                                    (const uint8_t *) &SERVO_STILL, sizeof(SERVO_STILL));
  ble_client_write_without_response(s_ctx.scratch2_characteristic,
                                    (const uint8_t *) &SERVO_STILL, sizeof(SERVO_STILL));
  printf("SELECT=0");
}

static void handle_select_button_down(ClickRecognizerRef recognizer, void *context) {
  ble_client_write_without_response(s_ctx.scratch1_characteristic,
                                    (const uint8_t *) &SERVO_FWD, sizeof(SERVO_FWD));
  ble_client_write_without_response(s_ctx.scratch2_characteristic,
                                    (const uint8_t *) &SERVO_BACK, sizeof(SERVO_BACK));
  printf("SELECT=1");
}

static void click_config_provider(void *data) {
  window_raw_click_subscribe(BUTTON_ID_UP, handle_up_button_down, handle_up_button_up, NULL);
  window_raw_click_subscribe(BUTTON_ID_DOWN, handle_down_button_down, handle_down_button_up, NULL);
  window_raw_click_subscribe(BUTTON_ID_SELECT, handle_select_button_down, handle_select_button_up, NULL);

}

//------------------------------------------------------------------------------

Window * ble_sumo_control_window_create(void) {
  Window *window = window_create();
  window_set_user_data(window, &s_ctx);

  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
    .appear = window_appear,
    .disappear = window_disappear,
  });

  window_set_click_config_provider(window, click_config_provider);

  s_ctx.window = window;
  return window;
}
