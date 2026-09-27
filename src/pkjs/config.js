module.exports = [
  {
    "type": "heading",
    "defaultValue": "CrystalTick Configuration"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Watchface Settings"
      },
      {
        "type": "select",
        "messageKey": "CentralFaceColor",
        "label": "Central Face Color",
        "defaultValue": 0,
        "options": [
          { "label": "White (Default)", "value": 0 },
          { "label": "Cyan", "value": 1 },
          { "label": "Green", "value": 2 },
          { "label": "Amber", "value": 3 }
        ],
        "description": "Select the tint color for the LCD screen area."
      },
      {
        "type": "toggle",
        "messageKey": "DateFormatDDMM",
        "label": "Date Format: DD-MM",
        "defaultValue": false,
        "description": "Enable to show Day first (DD-MM). Disable for Month first (MM-DD)."
      },
      {
        "type": "select",
        "messageKey": "FirstRowComplication",
        "label": "First Row Complication",
        "defaultValue": 0,
        "options": [
          { "label": "Steps", "value": 0 },
          { "label": "Temperature", "value": 1 },
          { "label": "Heart Rate", "value": 2 }
        ],
        "description": "Choose the data to display on the first row."
      },
      {
        "type": "select",
        "messageKey": "TemperatureUnit",
        "label": "Temperature Unit",
        "defaultValue": 0,
        "options": [
          { "label": "Celsius (°C)", "value": 0 },
          { "label": "Fahrenheit (°F)", "value": 1 }
        ],
        "description": "Choose the unit for displaying the temperature."
      },
      {
        "type": "select",
        "messageKey": "SecondsDisplay",
        "label": "Seconds Display",
        "defaultValue": 0,
        "options": [
          { "label": "Hide seconds", "value": 0 },
          { "label": "Show seconds", "value": 1 },
          { "label": "Show 00", "value": 2 },
          { "label": "Power save (show while backlight is on)", "value": 3 }
        ],
        "description": "Select how seconds are displayed. \"Show seconds\" ticks continuously and costs the most battery; \"Power save\" only ticks live while the backlight is on."
      },
      {
        "type": "toggle",
        "messageKey": "FirstDayIsMonday",
        "label": "First Day is Monday",
        "defaultValue": false,
        "description": "Enable to start the week on Monday. Disable to start on Sunday."
      },
      {
        "type": "select",
        "messageKey": "BacklightColor",
        "label": "Backlight Color (Pebble Time 2 Only)",
        "defaultValue": 0,
        "options": [
          { "label": "System Default", "value": 0 },
          { "label": "White", "value": 1 },
          { "label": "Blue", "value": 2 },
          { "label": "Green", "value": 3 },
          { "label": "Amber", "value": 4 },
          { "label": "Red", "value": 5 }
        ],
        "description": "Select the hardware backlight color (only supported on Pebble Time 2)."
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Screen Care"
      },
      {
        "type": "text",
        "defaultValue": "CrystalTick includes two features to reduce LCD burn-in from long-held static elements like the bezel and colon."
      },
      {
        "type": "toggle",
        "messageKey": "NightInvertEnabled",
        "label": "Night Auto-Invert",
        "defaultValue": true,
        "description": "Automatically swap the foreground/background colors overnight, both for a dim negative look and to periodically flip which pixels are lit."
      },
      {
        "type": "slider",
        "messageKey": "NightStartHour",
        "label": "Night Starts At",
        "defaultValue": 22,
        "min": 0,
        "max": 23,
        "step": 1,
        "description": "Hour of day (0-23) night mode begins."
      },
      {
        "type": "slider",
        "messageKey": "NightEndHour",
        "label": "Night Ends At",
        "defaultValue": 6,
        "min": 0,
        "max": 23,
        "step": 1,
        "description": "Hour of day (0-23) night mode ends."
      },
      {
        "type": "toggle",
        "messageKey": "AntiBurnInShift",
        "label": "Anti Burn-in Shift",
        "defaultValue": true,
        "description": "Nudge the whole display by a pixel or two each day so no single pixel stays lit or unlit for weeks."
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];
