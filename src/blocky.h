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
  "1", "2", "F", "G", "3", "4", "M", "5", "8", "#", "$", "%", "6", ")",};

enum WeatherKey {
  WEATHER_ICON_KEY = 0x0,         // TUPLE_INT
  WEATHER_TEMPERATURE_KEY = 0x1,  // TUPLE_CSTRING
  INVERT_COLOR_KEY = 0x2,  // TUPLE_CSTRING
  ANALOG_KEY = 0x3
};