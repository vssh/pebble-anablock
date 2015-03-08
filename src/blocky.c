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
GFont bat;
GFont imagine_15;
GFont imagine_58;

bool analog = false;
bool shake = true;
bool strip = true;
int update_time = 60;
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
static int weather_icon = 0;
static char weather_temp[] = "";
static int weather_timestamp = 0;

TextLayer *battery_text_layer;

static AppSync sync;
static uint8_t sync_buffer[96];

char* boolToString(bool b) {
  return b == true ? "true" : "false";
}

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

// battery
void update_battery_state(BatteryChargeState battery_state) {
  if(battery_state.charge_percent <= 20) {
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
  }
  else {
    time_format = "%I:%M";
  }
  
  char time_text_old[10] = "";
  strcpy(time_text_old, time_text);
  strftime(time_text, sizeof(time_text), time_format, t);
    
  if(strncmp(time_text, time_text_old, 2)!=0) {
    strncpy(hour1, time_text, 1);
    strncpy(hour2, time_text+1, 1);
    text_layer_set_text(text_hour1_layer, hour1);
    text_layer_set_text(text_hour2_layer, hour2);
  }      
  strncpy(min1, time_text+3, 1);
  strncpy(min2, time_text+4, 1);
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

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  if(analog == false) {
    digits_update_proc(NULL, NULL);
    //layer_mark_dirty(s_digits_layer);
  }
  else {    
    layer_mark_dirty(s_hands_layer);
  }  
  // Only update the date when it has changed.
  if (yday != tick_time->tm_yday) {
    layer_mark_dirty((Layer*)day_holder);
    yday = tick_time->tm_yday;
  }
  
  int time_now = tick_time->tm_mday*24*60 + tick_time->tm_hour*60 + tick_time->tm_min;
  /*APP_LOG(0, "time_now: %d", time_now);
  APP_LOG(0, "weather_timestamp: %d", weather_timestamp);
  APP_LOG(0, "update_time: %d", update_time);
  APP_LOG(0, "weather_icon: %d", weather_icon);
  APP_LOG(0, "weather_temp: %s", weather_temp);*/
  if(time_now - weather_timestamp > update_time || time_now < weather_timestamp || weather_icon > 12){
  //if(time_now - weather_timestamp > 30 || time_now < weather_timestamp || weather_icon > 12){    
    Tuplet new_values[] = {
      TupletCString(UPDATE_NOW, boolToString(false))
    };
    app_sync_set(&sync, new_values, ARRAY_LENGTH(new_values));
  }
  /*else {
    text_layer_set_text(icon_layer, WEATHER_ICONS[weather_icon]);
    text_layer_set_text(text_temp_layer, weather_temp);
  }*/
}

static void sync_tuple_changed_callback(const uint32_t key,
                                        const Tuple* new_tuple,
                                        const Tuple* old_tuple,
                                        void* context) {
  bool old_shake = shake;
  bool old_strip = strip;
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  // App Sync keeps new_tuple in sync_buffer, so we may use it directly
  switch (key) {
    case WEATHER_ICON_KEY:
      //APP_LOG(0, "IN WEATHER ICON KEY");
      if(weather_icon != new_tuple->value->uint8) {
        weather_icon = new_tuple->value->uint8;
        text_layer_set_text(icon_layer, WEATHER_ICONS[weather_icon]);
      }      
      break;

    case WEATHER_TEMPERATURE_KEY:
      //APP_LOG(0, "IN WEATHER TEMPERATURE KEY:%s", weather_temp);
      if(strcmp(weather_temp, new_tuple->value->cstring) != 0 && strcmp(new_tuple->value->cstring, "") != 0) {
        strcpy(weather_temp, new_tuple->value->cstring);
        text_layer_set_text(text_temp_layer, weather_temp);
      }      
      break;
    
    case SHAKE_KEY:
      //APP_LOG(0, "IN SHAKE KEY");
      shake = strcmp(new_tuple->value->cstring, "true") == 0 ? true : false;
      if(shake != old_shake) {
        shake == true ? accel_tap_service_subscribe(wrist_flick_handler) : accel_tap_service_unsubscribe();
      }
      break;
    
    /*case ANALOG_KEY:
      APP_LOG(0, "IN ANALOG KEY");
      analog = strcmp(new_tuple->value->cstring, "true") == 0 ? true : false;
      switch_analog_digital();
      break;*/
    
    case INVERT_STRIP_KEY:
      //APP_LOG(0, "IN INVERT STRIP KEY");
      strip = strcmp(new_tuple->value->cstring, "true") == 0 ? true : false;
      if (strip != old_strip) {
        if (strip == true){
          inverter_strip = inverter_layer_create(GRect(96, 0, 48, 168));
          layer_add_child(window_layer, inverter_layer_get_layer(inverter_strip));
        }
        else {
          layer_remove_from_parent((Layer*)inverter_strip);
          inverter_layer_destroy(inverter_strip);
        }
      }      
      break;
      
    /*case UPDATE_TIME_KEY:
      APP_LOG(0, "IN UPDATE TIME KEY:%d",atoi(new_tuple->value->cstring));
      update_time = atoi(new_tuple->value->cstring);
      break;*/
    
    case UPDATE_NOW:
      //APP_LOG(0, "IN UPDATE NOW: %s", new_tuple->value->cstring);
      if(strcmp(new_tuple->value->cstring, "true") == 0) {
        weather_timestamp = t->tm_mday*24*60 + t->tm_hour*60 + t->tm_min;
      }
      break;
  }
}

void handle_init(void) {
  
  analog = persist_exists(ANALOG_KEY) ? persist_read_bool(ANALOG_KEY) : false;
  shake = persist_exists(SHAKE_KEY) ? persist_read_bool(SHAKE_KEY) : true;
  strip = persist_exists(INVERT_STRIP_KEY) ? persist_read_bool(INVERT_STRIP_KEY) : true;
  /*weather_icon = persist_exists(WEATHER_ICON_KEY) ? persist_read_int(WEATHER_ICON_KEY) : 15;
  if(persist_exists(WEATHER_TEMPERATURE_KEY)) {
    persist_read_string(WEATHER_TEMPERATURE_KEY, weather_temp, sizeof(weather_temp));
  }*/
  //APP_LOG(0, "update time after persist get: %d", update_time);
  weather_timestamp = persist_exists(LAST_UPDATE_TIME) ? persist_read_int(LAST_UPDATE_TIME) : 0;
  //update_time = persist_exists(UPDATE_TIME_KEY) ? persist_read_int(UPDATE_TIME_KEY) : 60;
  
  window = window_create();
  window_stack_push(window, true /* Animated */);
  window_set_background_color(window, GColorBlack);

  window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);  
  
  // Setup weather bar
  weather_holder = layer_create(GRect(96, 0, 48, 56));
  layer_add_child(window_layer, weather_holder);
  
  meteocons = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_METEOBATT_30));
  bat = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_METEOBATT_15));
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
  layer_add_child(window_layer, s_digits_layer);
  layer_set_update_proc(s_digits_layer, digits_update_proc);
  
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
  
  text_bt_layer = text_layer_create(GRect(1, 5, 46, 21));
  text_layer_set_text_color(text_bt_layer, GColorWhite);
  text_layer_set_background_color(text_bt_layer, GColorClear);
  text_layer_set_font(text_bt_layer, bat);
  text_layer_set_text(text_bt_layer, "s");
  layer_set_hidden((Layer *)text_bt_layer, true);
  layer_add_child(bt_holder, text_layer_get_layer(text_bt_layer));
  
  text_bat_layer = text_layer_create(GRect(1, 32, 46, 21));
  text_layer_set_text_color(text_bat_layer, GColorWhite);
  text_layer_set_background_color(text_bat_layer, GColorClear);
  text_layer_set_font(text_bat_layer, bat);
  text_layer_set_text(text_bat_layer, "r");
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
  if (strip == true){
    inverter_strip = inverter_layer_create(GRect(96, 0, 48, 168));
    layer_add_child(window_layer, inverter_layer_get_layer(inverter_strip));
  }
  
  // Setup messaging
  const int inbound_size = 96;
  const int outbound_size = 96;
  /*char *upd_time = "";
  snprintf(upd_time, 4, "%d", update_time);
  APP_LOG(0, "update time to string:%s", upd_time);*/
  app_message_open(inbound_size, outbound_size);
  
  Tuplet initial_values[] = {
    TupletInteger(WEATHER_ICON_KEY, weather_icon),
    TupletCString(WEATHER_TEMPERATURE_KEY, ""),
    TupletCString(SHAKE_KEY, boolToString(shake)),
    //TupletCString(ANALOG_KEY, boolToString(analog)),
    TupletCString(INVERT_STRIP_KEY, boolToString(strip)),
    TupletCString(UPDATE_NOW, "false")
    //TupletCString(UPDATE_TIME_KEY, upd_time)
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
  
  app_sync_deinit(&sync);
  
  persist_write_bool(ANALOG_KEY, analog);
  persist_write_bool(SHAKE_KEY, shake);
  persist_write_bool(INVERT_STRIP_KEY, strip);
  //persist_write_int(WEATHER_ICON_KEY, weather_icon);
  //APP_LOG(0, "update time before persist set: %d", update_time);
  //persist_write_string(WEATHER_TEMPERATURE_KEY, weather_temp);
  persist_write_int(LAST_UPDATE_TIME, weather_timestamp);
  //persist_write_int(UPDATE_TIME_KEY, update_time);
  
  fonts_unload_custom_font(meteocons);
  fonts_unload_custom_font(imagine_15);
  fonts_unload_custom_font(imagine_58);
  
  gpath_destroy(s_minute_arrow);
  gpath_destroy(s_hour_arrow);
  
  /*layer_remove_from_parent((Layer*)icon_layer);
  layer_remove_from_parent((Layer*)text_temp_layer);
  layer_remove_from_parent((Layer*)text_hour1_layer);
  layer_remove_from_parent((Layer*)text_hour2_layer);
  layer_remove_from_parent((Layer*)text_min1_layer);
  layer_remove_from_parent((Layer*)text_min2_layer);
  layer_remove_from_parent((Layer*)text_date1_layer);
  layer_remove_from_parent((Layer*)text_date2_layer);
  layer_remove_from_parent((Layer*)text_day_layer);
  layer_remove_from_parent((Layer*)text_month_layer);
  layer_remove_from_parent((Layer*)text_bt_layer);
  layer_remove_from_parent((Layer*)text_bat_layer);
  layer_remove_from_parent((Layer*)weather_holder);
  layer_remove_from_parent((Layer*)day_holder);
  layer_remove_from_parent((Layer*)bt_holder);
  layer_remove_from_parent((Layer*)s_digits_layer);
  layer_remove_from_parent((Layer*)s_hands_layer);
  layer_remove_from_parent((Layer*)window_layer);
  
  text_layer_destroy(icon_layer);
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
  layer_destroy(s_digits_layer);  
  layer_destroy(s_hands_layer);
  layer_destroy(window_layer);*/
  
  window_destroy(window);
}

int main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}
