var Clay = require('@rebble/clay');
var clayConfig = require('./config');

// autoHandleEvents is off so we control exactly when the webview
// opens/closes rather than relying on Clay's defaults.
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

Pebble.addEventListener('showConfiguration', function () {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) {
    return;
  }
  var dict = clay.getSettings(e.response);
  Pebble.sendAppMessage(dict, function () {
    console.log('Settings sent successfully');
  }, function () {
    console.log('Error sending settings');
  });
});

var WEATHER_REQUEST_TIMEOUT_MS = 15000;
var COMPLICATION_TEMPERATURE = 1;
// Clay's own getSettings() already persists the flattened settings here on
// every save - reused instead of tracking our own copy.
var CLAY_SETTINGS_STORAGE_KEY = 'clay-settings';

// Clay's `select` items always send their value as a string (see
// settings.c's tuple_to_uint for the watch-side version of this same
// gotcha), so this coerces before comparing rather than using ===.
function isTemperatureComplicationSelected() {
  var stored = localStorage.getItem(CLAY_SETTINGS_STORAGE_KEY);
  if (!stored) {
    return false;
  }
  try {
    var settings = JSON.parse(stored);
    return Number(settings['FirstRowComplication']) === COMPLICATION_TEMPERATURE;
  } catch (e) {
    return false;
  }
}

// Only reset by a fresh JS launch, not persisted - worst case (a restart
// right after a real change) is one redundant send, not a stuck value.
var lastSentTemperature = null;

function fetchWeather() {
  navigator.geolocation.getCurrentPosition(
    function (pos) {
      // Open-Meteo: no API key needed, always returns Celsius.
      var url = 'https://api.open-meteo.com/v1/forecast?latitude=' + pos.coords.latitude +
          '&longitude=' + pos.coords.longitude + '&current=temperature_2m';
      var req = new XMLHttpRequest();
      req.open('GET', url, true);
      req.timeout = WEATHER_REQUEST_TIMEOUT_MS;
      req.onload = function () {
        if (req.readyState === 4 && req.status === 200) {
          var response = JSON.parse(req.responseText);
          var temp = Math.round(response.current.temperature_2m);
          if (temp === lastSentTemperature) {
            console.log('Temperature unchanged (' + temp + '), skipping send');
            return;
          }
          lastSentTemperature = temp;
          Pebble.sendAppMessage({ 'TEMPERATURE': temp }, function () {
            console.log('Temperature sent successfully: ' + temp);
          }, function () {
            console.log('Error sending temperature');
          });
        }
      };
      req.onerror = function () {
        console.log('Weather request failed');
      };
      req.ontimeout = function () {
        console.log('Weather request timed out');
      };
      req.send(null);
    },
    function () {
      console.log('Error requesting location!');
    },
    { timeout: WEATHER_REQUEST_TIMEOUT_MS, maximumAge: 60000 }
  );
}

var WEATHER_REFRESH_INTERVAL_MS = 30 * 60 * 1000;

// Both the initial fetch and the repeating one check isTemperatureComplicationSelected()
// each time, so switching away/back to Temperature just works without touching the interval.
Pebble.addEventListener('ready', function () {
  console.log('PebbleKit JS ready!');
  if (isTemperatureComplicationSelected()) {
    fetchWeather();
  }
  setInterval(function () {
    if (isTemperatureComplicationSelected()) {
      fetchWeather();
    }
  }, WEATHER_REFRESH_INTERVAL_MS);
});

// The watch asks for a fresh reading right when the user switches the
// complication to Temperature, so it isn't stuck showing a stale value.
Pebble.addEventListener('appmessage', function (e) {
  if (e.payload && e.payload['REQUEST_WEATHER']) {
    fetchWeather();
  }
});
