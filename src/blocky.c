#include "pebble.h"
#include "blocky.h"

Window *window;

Layer *window_layer = NULL;
Layer *weather_holder = NULL;
Layer *day_holder = NULL;
Layer *bt_holder = NULL;

static Layer *s_hands_layer, *s_digits_layer;
static GPath *s_minute_arrow, *s_hour_arrow;

TextLayer *icon_layer;
TextLayer *text_temp_layer;
TextLayer *text_day_layer;
TextLayer *text_date_layer;
TextLayer *text_hour1_layer;
TextLayer *text_hour2_layer;
TextLayer *text_min1_layer;
TextLayer *text_min2_layer;
TextLayer *text_date1_layer;
TextLayer *text_date2_layer;
TextLayer *text_month_layer;
TextLayer *text_bt_layer;
TextLayer *text_bat_layer;

GFont meteocons;
GFont imagine_15;
GFont imagine_58;

bool analog = false;
bool shake = true;
static int yday = -1;

InverterLayer *inverter_strip = NULL;

static char time_text[] = "00:00";
static char hour1[] = "0";
static char hour2[] = "0";
static char min1[] = "0";
static char min2[] = "0";
static char date1[] = "0";
static char date2[] = "0";
static char date_text[] = "00";
static char day_text[] = "xxx";
static char month_text[] = "xxx";

TextLayer *battery_text_layer;

static AppSync sync;
static uint8_t sync_buffer[64];

void bluetooth_connection_changed(bool connected) {
  static bool _connected = true;

  // This seemed to get called twice on disconnect
  if (!connected && _connected) {
    vibes_short_pulse();
    layer_set_hidden((Layer *)text_bt_layer, false);    
  }
  else if(connected && !_connected) {
    layer_set_hidden((Layer *)text_bt_layer, true); 
  }
  _connected = connected;
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  if(analog == false) {
    layer_mark_dirty(s_digits_layer);
  }
  else {
    layer_mark_dirty(s_hands_layer);
  }  
  // Only update the date when it has changed.
  if (yday != tick_time->tm_yday) {
    layer_mark_dirty((Layer*)day_holder);
    yday = tick_time->tm_yday;
  }
}

// battery
void update_battery_state(BatteryChargeState battery_state) {
  if(battery_state.charge_percent <= 20) {
    static char battery_text[] = "BT20";
    snprintf(battery_text, sizeof(battery_text), "BT%d", battery_state.charge_percent);
    text_layer_set_text(text_bat_layer, battery_text);
    layer_set_hidden((Layer *)text_bat_layer, false);
    vibes_short_pulse();
  }  
  else {
    layer_set_hidden((Layer *)text_bat_layer, true);
  }
}

void switch_analog_digital() {
  if(analog == true) {
    layer_set_hidden((Layer*)text_hour1_layer, true);
    layer_set_hidden((Layer*)text_hour2_layer, true);
    layer_set_hidden((Layer*)text_min1_layer, true);
    layer_set_hidden((Layer*)text_min2_layer, true);
    layer_set_hidden((Layer*)text_date1_layer, true);
    layer_set_hidden((Layer*)text_date2_layer, true);
    layer_set_hidden(s_hands_layer, false);
    layer_mark_dirty(s_hands_layer);
  }
  else {
    layer_set_hidden(s_hands_layer, true);
    layer_set_hidden((Layer*)text_hour1_layer, false);
    layer_set_hidden((Layer*)text_hour2_layer, false);
    layer_set_hidden((Layer*)text_min1_layer, false);
    layer_set_hidden((Layer*)text_min2_layer, false);
    layer_set_hidden((Layer*)text_date1_layer, false);
    layer_set_hidden((Layer*)text_date2_layer, false); 
    layer_mark_dirty(s_digits_layer);    
  }
  layer_mark_dirty((Layer*)day_holder);
}

void wrist_flick_handler(AccelAxisType axis, int32_t direction) {
  if (axis == 1) {
    analog = !analog;
    switch_analog_digital();
  }
}

static void hands_update_proc(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  // minute/hour hand
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorBlack);  
  gpath_rotate_to(s_hour_arrow, (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6));
  gpath_draw_filled(ctx, s_hour_arrow);
  gpath_draw_outline(ctx, s_hour_arrow);
  gpath_rotate_to(s_minute_arrow, TRIG_MAX_ANGLE * t->tm_min / 60);
  gpath_draw_filled(ctx, s_minute_arrow);
  gpath_draw_outline(ctx, s_minute_arrow);
}

static void digits_update_proc(Layer *layer, GContext *ctx) {  
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  
  char *time_format;
  if (clock_is_24h_style()) {
    time_format = "%R";
    } else {
      time_format = "%I:%M";
    }
  
  strftime(time_text, sizeof(time_text), time_format, t);

  strncpy(hour1, time_text, 1);
  strncpy(hour2, time_text+1, 1);
  strncpy(min1, time_text+3, 1);
  strncpy(min2, time_text+4, 1);    

  text_layer_set_text(text_hour1_layer, hour1);
  text_layer_set_text(text_hour2_layer, hour2);
  text_layer_set_text(text_min1_layer, min1);
  text_layer_set_text(text_min2_layer, min2);
}

static void day_update_proc(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  
  if(analog == false){
    strftime(date_text, sizeof(date_text), "%d", t);
    strncpy(date1, date_text, 1);
    strncpy(date2, date_text+1, 1);
    text_layer_set_text(text_date1_layer, date1);
    text_layer_set_text(text_date2_layer, date2);
    strftime(month_text, sizeof(month_text), "%b", t);
    text_layer_set_text(text_month_layer, month_text);
  }
  else {
    strftime(date_text, sizeof(date_text), "%d", t);
    text_layer_set_text(text_month_layer, date_text);
  }
  
  strftime(day_text, sizeof(day_text), "%a", t);     
  text_layer_set_text(text_day_layer, day_text);
}

static void sync_tuple_changed_callback(const uint32_t key,
                                        const Tuple* new_tuple,
                                        const Tuple* old_tuple,
                                        void* context) {
  bool old_shake = shake;

  // App Sync keeps new_tuple in sync_buffer, so we may use it directly
  switch (key) {
    case WEATHER_ICON_KEY:
      text_layer_set_text(icon_layer, WEATHER_ICONS[new_tuple->value->uint8]);
      break;

    case WEATHER_TEMPERATURE_KEY:
      text_layer_set_text(text_temp_layer, new_tuple->value->cstring);
      break;
    
    case SHAKE_KEY:      
      shake = strcmp(new_tuple->value->cstring, "true") == 0 ? true : false;
      if(shake != old_shake) {
        shake == true ? accel_tap_service_subscribe(wrist_flick_handler) : accel_tap_service_unsubscribe();
      }
      break;
    
    case ANALOG_KEY:
      analog = strcmp(new_tuple->value->cstring, "true") == 0 ? true : false;
      switch_analog_digital();
      break;      
  }
}

void handle_init(void) {
  window = window_create();
  window_stack_push(window, true /* Animated */);
  window_set_background_color(window, GColorBlack);

  window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  analog = persist_exists(ANALOG_KEY) ? persist_read_bool(ANALOG_KEY) : false;
  shake = persist_exists(SHAKE_KEY) ? persist_read_bool(SHAKE_KEY) : true;

  // Setup weather bar
  weather_holder = layer_create(GRect(96, 0, 48, 56));
  layer_add_child(window_layer, weather_holder);
  
  meteocons = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_METEOCONS_30));
  imagine_15 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_IMAGINE_15));
  imagine_58 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_IMAGINE_58));
  
  icon_layer = text_layer_create(GRect(0, 0, 48, 30));
  text_layer_set_text_color(icon_layer, GColorWhite);
  text_layer_set_background_color(icon_layer, GColorClear);
  text_layer_set_font(icon_layer, meteocons);
  text_layer_set_text_alignment(icon_layer, GTextAlignmentCenter);
  layer_add_child(weather_holder, text_layer_get_layer(icon_layer));

  text_temp_layer = text_layer_create(GRect(1, 30, 46, 18));
  text_layer_set_text_color(text_temp_layer, GColorWhite);
  text_layer_set_background_color(text_temp_layer, GColorClear);
  text_layer_set_font(text_temp_layer, imagine_15);
  text_layer_set_text_alignment(text_temp_layer, GTextAlignmentRight);
  layer_add_child(weather_holder, text_layer_get_layer(text_temp_layer));

  // Initialize day, month
  day_holder = layer_create(GRect(96, 112, 48, 56));
  layer_set_update_proc(day_holder, day_update_proc);
  layer_add_child(window_layer, day_holder);
  
  text_month_layer = text_layer_create(GRect(1, 20, 46, 23));
  text_layer_set_text_color(text_month_layer, GColorWhite);
  text_layer_set_background_color(text_month_layer, GColorClear);
  text_layer_set_font(text_month_layer, imagine_15);
  text_layer_set_text_alignment(text_month_layer, GTextAlignmentRight);
  layer_add_child(day_holder, text_layer_get_layer(text_month_layer));
  
  text_day_layer = text_layer_create(GRect(1, 38, 46, 23));
  text_layer_set_text_color(text_day_layer, GColorWhite);
  text_layer_set_background_color(text_day_layer, GColorClear);
  text_layer_set_font(text_day_layer, imagine_15);
  layer_add_child(day_holder, text_layer_get_layer(text_day_layer));   
  
  // Initialize time
  s_digits_layer = layer_create(bounds);
  layer_set_update_proc(s_digits_layer, digits_update_proc);
  layer_add_child(window_layer, s_digits_layer);
  
  text_hour1_layer = text_layer_create(GRect(4, -12, 48, 66));
  text_layer_set_text_color(text_hour1_layer, GColorWhite);
  text_layer_set_background_color(text_hour1_layer, GColorClear);
  text_layer_set_text_alignment(text_hour1_layer, GTextAlignmentCenter);
  text_layer_set_font(text_hour1_layer, imagine_58);
  layer_add_child(s_digits_layer, text_layer_get_layer(text_hour1_layer));
  
  text_hour2_layer = text_layer_create(GRect(52, -12, 48, 66));
  text_layer_set_text_color(text_hour2_layer, GColorWhite);
  text_layer_set_background_color(text_hour2_layer, GColorClear);
  text_layer_set_text_alignment(text_hour2_layer, GTextAlignmentCenter);
  text_layer_set_font(text_hour2_layer, imagine_58);
  layer_add_child(s_digits_layer, text_layer_get_layer(text_hour2_layer));
  
  text_min1_layer = text_layer_create(GRect(52, 44, 48, 66));
  text_layer_set_text_color(text_min1_layer, GColorWhite);
  text_layer_set_background_color(text_min1_layer, GColorClear);
  text_layer_set_text_alignment(text_min1_layer, GTextAlignmentCenter);
  text_layer_set_font(text_min1_layer, imagine_58);
  layer_add_child(s_digits_layer, text_layer_get_layer(text_min1_layer));
  
  text_min2_layer = text_layer_create(GRect(100, 44, 48, 66));
  text_layer_set_text_color(text_min2_layer, GColorWhite);
  text_layer_set_background_color(text_min2_layer, GColorClear);
  text_layer_set_text_alignment(text_min2_layer, GTextAlignmentCenter);
  text_layer_set_font(text_min2_layer, imagine_58);
  layer_add_child(s_digits_layer, text_layer_get_layer(text_min2_layer));
  
  text_date1_layer = text_layer_create(GRect(4, 100, 48, 66));
  text_layer_set_text_color(text_date1_layer, GColorWhite);
  text_layer_set_background_color(text_date1_layer, GColorClear);
  text_layer_set_text_alignment(text_date1_layer, GTextAlignmentCenter);
  text_layer_set_font(text_date1_layer, imagine_58);
  layer_add_child(s_digits_layer, text_layer_get_layer(text_date1_layer));
  
  text_date2_layer = text_layer_create(GRect(52, 100, 48, 66));
  text_layer_set_text_color(text_date2_layer, GColorWhite);
  text_layer_set_background_color(text_date2_layer, GColorClear);
  text_layer_set_text_alignment(text_date2_layer, GTextAlignmentCenter);
  text_layer_set_font(text_date2_layer, imagine_58);
  layer_add_child(s_digits_layer, text_layer_get_layer(text_date2_layer)); 
  
  // Initialize BT and battery indicators
  bt_holder = layer_create(GRect(0, 56, 48, 56));
  layer_add_child(window_layer, bt_holder);
  
  text_bt_layer = text_layer_create(GRect(1, 3, 46, 23));
  text_layer_set_text_color(text_bt_layer, GColorWhite);
  text_layer_set_background_color(text_bt_layer, GColorClear);
  text_layer_set_font(text_bt_layer, imagine_15);
  text_layer_set_text(text_bt_layer, "BT X");
  layer_set_hidden((Layer *)text_bt_layer, true);
  layer_add_child(bt_holder, text_layer_get_layer(text_bt_layer));
  
  text_bat_layer = text_layer_create(GRect(1, 30, 46, 23));
  text_layer_set_text_color(text_bat_layer, GColorWhite);
  text_layer_set_background_color(text_bat_layer, GColorClear);
  text_layer_set_font(text_bat_layer, imagine_15);
  layer_set_hidden((Layer *)text_bat_layer, true);
  layer_add_child(bt_holder, text_layer_get_layer(text_bat_layer)); 
  
  // Initialize analog
  s_hands_layer = layer_create(bounds);
  layer_set_update_proc(s_hands_layer, hands_update_proc);
  layer_add_child(window_layer, s_hands_layer);
  
  switch_analog_digital();
  
   // init hand paths
  s_hour_arrow = gpath_create(&HOUR_HAND_POINTS);
  s_minute_arrow = gpath_create(&MINUTE_HAND_POINTS);  
  
  GPoint center = grect_center_point(&bounds);
  gpath_move_to(s_minute_arrow, center);
  gpath_move_to(s_hour_arrow, center);

  
  // Inverter strip
  inverter_strip = inverter_layer_create(GRect(96, 0, 48, 168));
  layer_add_child(window_layer, inverter_layer_get_layer(inverter_strip));

  // Setup messaging
  const int inbound_size = 64;
  const int outbound_size = 64;
  app_message_open(inbound_size, outbound_size);
  
  Tuplet initial_values[] = {
    TupletInteger(WEATHER_ICON_KEY, 15),
    TupletCString(WEATHER_TEMPERATURE_KEY, "")
  };

  app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values,
                ARRAY_LENGTH(initial_values), sync_tuple_changed_callback,
                NULL, NULL);
  

  // Subscribe to notifications
  bluetooth_connection_service_subscribe(bluetooth_connection_changed);
  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
  battery_state_service_subscribe(update_battery_state);
  if(shake == true){
    accel_tap_service_subscribe(wrist_flick_handler);
  }  

  // Update the battery on launch
  update_battery_state(battery_state_service_peek());

  // TODO: Update display here to avoid blank display on launch?
}

void handle_deinit(void) {
  bluetooth_connection_service_unsubscribe();
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();  
  if(shake == true){
    accel_tap_service_unsubscribe();
  }  
  
  persist_write_bool(ANALOG_KEY, analog);
  persist_write_bool(SHAKE_KEY, shake);
  
  fonts_unload_custom_font(meteocons);
  fonts_unload_custom_font(imagine_15);
  fonts_unload_custom_font(imagine_58);
  
  gpath_destroy(s_minute_arrow);
  gpath_destroy(s_hour_arrow);
  
  /*text_layer_destroy(icon_layer);
  text_layer_destroy(text_temp_layer);
  text_layer_destroy(text_hour1_layer);
  text_layer_destroy(text_hour2_layer);
  text_layer_destroy(text_min1_layer);
  text_layer_destroy(text_min2_layer);
  text_layer_destroy(text_date1_layer);
  text_layer_destroy(text_date2_layer);
  text_layer_destroy(text_day_layer);
  text_layer_destroy(text_month_layer);
  text_layer_destroy(text_bt_layer);
  text_layer_destroy(text_bat_layer);
  
  layer_destroy(weather_holder);
  layer_destroy(day_holder);
  layer_destroy(bt_holder);
  layer_destroy(window_layer);
  layer_destroy(s_hands_layer)*/
  
  window_destroy(window);
}

int main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}
