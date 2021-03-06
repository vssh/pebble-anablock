var UNDEFINED = 0;
var CLEAR_DAY = 1;
var CLEAR_NIGHT = 2;
var WINDY = 3;
var COLD = 4;
var PARTLY_CLOUDY_DAY = 5;
var PARTLY_CLOUDY_NIGHT = 6;
var HAZE = 7;
var CLOUD = 8;
var RAIN = 9;
var SNOW = 10;
var HAIL = 11;
var CLOUDY = 12;
var STORM = 13;
var NA = 14;
var GPS = 15;
var BLANK = 16;

var imageId = {
  0 : STORM, //tornado
  1 : STORM, //tropical storm
  2 : STORM, //hurricane
  3 : STORM, //severe thunderstorms
  4 : STORM, //thunderstorms
  5 : HAIL, //mixed rain and snow
  6 : HAIL, //mixed rain and sleet
  7 : HAIL, //mixed snow and sleet
  8 : HAIL, //freezing drizzle
  9 : RAIN, //drizzle
  10 : HAIL, //freezing rain
  11 : RAIN, //showers
  12 : RAIN, //showers
  13 : SNOW, //snow flurries
  14 : SNOW, //light snow showers
  15 : SNOW, //blowing snow
  16 : SNOW, //snow
  17 : HAIL, //hail
  18 : HAIL, //sleet
  19 : HAZE, //dust
  20 : HAZE, //foggy
  21 : HAZE, //haze
  22 : HAZE, //smoky
  23 : WINDY, //blustery
  24 : WINDY, //windy
  25 : COLD, //cold
  26 : CLOUDY, //cloudy
  27 : CLOUDY, //mostly cloudy (night)
  28 : CLOUDY, //mostly cloudy (day)
  29 : PARTLY_CLOUDY_NIGHT, //partly cloudy (night)
  30 : PARTLY_CLOUDY_DAY, //partly cloudy (day)
  31 : CLEAR_NIGHT, //clear (night)
  32 : CLEAR_DAY, //sunny
  33 : CLEAR_NIGHT, //fair (night)
  34 : CLEAR_DAY, //fair (day)
  35 : HAIL, //mixed rain and hail
  36 : CLEAR_DAY, //hot
  37 : STORM, //isolated thunderstorms
  38 : STORM, //scattered thunderstorms
  39 : STORM, //scattered thunderstorms
  40 : STORM, //scattered showers
  41 : SNOW, //heavy snow
  42 : SNOW, //scattered snow showers
  43 : SNOW, //heavy snow
  44 : CLOUD, //partly cloudy
  45 : STORM, //thundershowers
  46 : SNOW, //snow showers
  47 : STORM, //isolated thundershowers
  3200 : NA, //not available
};

var locationOptions = { "timeout": 15000, "maximumAge": 60000 };

var options = JSON.parse(localStorage.getItem('options'));
//console.log('read options: ' + JSON.stringify(options));

var optionsBackup = { "use_gps" : "true",
                      "location" : "",
                      "units" : "celsius",
                      //"updatetime" : "60",
                      "shake" : "true",
                      //"analog" : "false",                                 
                      "strip" : "true"};

if (options === null || Object.keys(options).length < Object.keys(optionsBackup).length) options = optionsBackup;
//console.log("options length="+Object.keys(options).length);

var weather = JSON.parse(localStorage.getItem('weather'));
if(weather === null) weather = {icon: NA, temperature: ""};

function getWeatherFromLatLong(latitude, longitude) {
  var response;
  var woeid = -1;
  var query = encodeURI("select woeid from geo.placefinder where text=\""+latitude+","+longitude + "\" and gflags=\"R\"");
  var url = "http://query.yahooapis.com/v1/public/yql?q=" + query + "&format=json";
  var req = new XMLHttpRequest();
  req.open('GET', url, true);
  req.onload = function(e) {
    if (req.readyState == 4) {
      if (req.status == 200) {
        response = JSON.parse(req.responseText);
        if (response) {
          woeid = response.query.results.Result.woeid;
          getWeatherFromWoeid(woeid);
        }
      } else {
        console.log("Error");
      }
    }
  };
  req.send(null);
}

function getWeatherFromLocation(location_name) {
  var response;
  var woeid = -1;

  var query = encodeURI("select woeid from geo.places(1) where text=\"" + location_name + "\"");
  var url = "http://query.yahooapis.com/v1/public/yql?q=" + query + "&format=json";
  var req = new XMLHttpRequest();
  req.open('GET', url, true);
  req.onload = function(e) {
    if (req.readyState == 4) {
      if (req.status == 200) {
        // console.log(req.responseText);
        response = JSON.parse(req.responseText);
        if (response) {
          woeid = response.query.results.place.woeid;
          getWeatherFromWoeid(woeid);
        }
      } else {
        console.log("Error");
      }
    }
  };
  req.send(null);
}

function getWeatherFromWoeid(woeid) {
  //console.log(options.units);
  var celsius = options.units == 'celsius';
  var query = encodeURI("select item.condition from weather.forecast where woeid = " + woeid +
                        " and u = " + (celsius ? "\"c\"" : "\"f\""));
  var url = "http://query.yahooapis.com/v1/public/yql?q=" + query + "&format=json";

  var response;
  var req = new XMLHttpRequest();
  req.open('GET', url, true);
  req.onload = function(e) {
    if (req.readyState == 4) {
      if (req.status == 200) {
        response = JSON.parse(req.responseText);
        if (response) {
          var condition = response.query.results.channel.item.condition;
          var temperature = condition.temp + (celsius ? "C" : "F");
          var icon = imageId[condition.code];
          // console.log("temp " + temperature);
          // console.log("icon " + icon);
          // console.log("condition " + condition.text);
          weather.icon = icon;
          weather.temperature = temperature;
          localStorage.setItem('weather', JSON.stringify(weather));
          Pebble.sendAppMessage({
            "icon" : icon,
            "temperature" : temperature,
            "updatenow" : "true"
          });
        }
      } else {
        console.log("Error");
      }
    }
  };
  req.send(null);
}

function updateWeather() {
  if (options.use_gps == "true") {
    window.navigator.geolocation.getCurrentPosition(locationSuccess,
                                                    locationError,
                                                    locationOptions);
  } else {
    getWeatherFromLocation(options.location);
  }
  //console.log("weather updated");
}

function locationSuccess(pos) {
  var coordinates = pos.coords;
  getWeatherFromLatLong(coordinates.latitude, coordinates.longitude);
}

function locationError(err) {
  console.warn('location error (' + err.code + '): ' + err.message);
  Pebble.sendAppMessage({
    "icon": GPS,
    "temperature": "X",
    "updatenow" : "true"
  });
}

Pebble.addEventListener('showConfiguration', function(e) {
  var uri = 'http://vssh.github.io/pebble-anablock/config.html?' +
    'use_gps=' + encodeURIComponent(options.use_gps) +
    '&location=' + encodeURIComponent(options.location) +
    '&units=' + encodeURIComponent(options.units) +
    //'&update_time=' + encodeURIComponent(options.update_time) +
    '&shake=' + encodeURIComponent(options.shake) +
    //'&analog=' + encodeURIComponent(options.analog) +
    '&strip=' + encodeURIComponent(options.strip);
  //console.log('showing configuration at uri: ' + uri);

  Pebble.openURL(uri);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e.response) {
    options = JSON.parse(decodeURIComponent(e.response));
    localStorage.setItem('options', JSON.stringify(options));
    //console.log('storing options: ' + JSON.stringify(options));
    Pebble.sendAppMessage({
            "shake" : options.shake,
            //"analog" : options.analog,
            "strip" : options.strip
            //"updatetime" : options.update_time
          });
    //console.log("update_time:" + options.update_time);
    updateWeather();
  } else {
    //console.log('no options received');
  }
});

Pebble.addEventListener("ready", function(e) {
  //console.log("connect!" + e.ready);
  /*if (weather.timestamp < Math.floor(Date.now() / 1000) - parseInt(options.update_time)*60/2 || weather.icon === NA || weather.icon === GPS || weather.temperature ==="") {
    updateWeather();    
  }
  else {
    Pebble.sendAppMessage({
            "icon" : weather.icon,
            "temperature" : weather.temperature
          });
  }
  setInterval(function() {
    //console.log("timer fired");
    updateWeather();
  }, parseInt(options.update_time)*60*1000); // 30 minutes
  console.log(e.type);*/
  /*if(weather.icon === NA) {
    updateWeather();
  }
  else {
    Pebble.sendAppMessage({
            "icon" : weather.icon,
            "temperature" : weather.temperature,
            "updatenow" : "false"
          });
    //console.log("temp:"+weather.temperature);
  }*/
});

Pebble.addEventListener("appmessage", function(e) {
  if(e.payload.updatenow === "false") {
    //console.log("msg received");
    updateWeather();
  }
});