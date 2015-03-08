#pragma once
#include "pebble.h"

static const GPathInfo MINUTE_HAND_POINTS = {
  4, (GPoint []) {
    {-3, 0},
    {3, 0},
    {3, -60},
    {-3, -60}
  }
  };

static const GPathInfo HOUR_HAND_POINTS = {
  4, (GPoint []){
    {-4, 0},
    {4, 0},
    {4, -40},
    {-4, -40}
  }
};

static const char *WEATHER_ICONS[] = {
  "", "\\", "]", "F", "G", "^", "_", "M", "`", "c", "g", "h", "i", "j", "m","l", ""};

enum WeatherKey {
  WEATHER_ICON_KEY = 0x0,         // TUPLE_INT
  WEATHER_TEMPERATURE_KEY = 0x1,  // TUPLE_CSTRING
  SHAKE_KEY = 0x2,
  ANALOG_KEY = 0x3,
  INVERT_STRIP_KEY = 0x4,
  UPDATE_TIME_KEY = 0x5,
  UPDATE_NOW = 0x6,
  LAST_UPDATE_TIME = 0x7
};