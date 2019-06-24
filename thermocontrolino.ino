
#include <OneWire.h>
#include <DallasTemperature.h>

#include <TimerOne.h>
#include <TM74HC595Display.h>

// INTERNAL FAN RELAY CONFIG
#define PERIODIC_PIN      8
#define PERIODIC_ON       LOW
#define PERIODIC_OFF      HIGH
#define PERIODIC_INTERVAL 60
#define PERIODIC_ACTIVITY 15
#define PERIODIC_TEMP_HI  45.0
#define PERIODIC_TEMP_LO  35.0
#define PERIODIC_ENABLED  true
unsigned int periodic_state;
unsigned int periodic_timer;

// COOLER RELAY CONFIG
#define COOLER_PIN      8
#define COOLER_ON       LOW
#define COOLER_OFF      HIGH
#define COOLER_TEMP_HI  37.8
#define COOLER_TEMP_LO  37.7
#define COOLER_ENABLED  true
unsigned int cooler_state;

// HEATER RELAY CONFIG
#define HEATER_PIN      9
#define HEATER_ON       LOW
#define HEATER_OFF      HIGH
#define HEATER_TEMP_HI  39.0
#define HEATER_TEMP_LO  38.6
#define HEATER_ENABLED  true
unsigned int heater_state;

// 4-DIGIT LED DISPLAY CONFIG
#define LED_SCLK        7
#define LED_RCLK        6
#define LED_DIO         5
TM74HC595Display Display(LED_SCLK, LED_RCLK, LED_DIO);

// TEMPERATURE SENSOR CONFIG
#define TEMPERATURE_PIN 3
OneWire wire(TEMPERATURE_PIN);
DallasTemperature Temperature(&wire);
double temp_prev;

/*
   Initialization
*/
void timerIsr() {

  Display.timerIsr();
}

void setup(void) {

  // Init LED display
  Timer1.initialize(1500); // set a timer of length 1500
  Timer1.attachInterrupt(timerIsr); // attach the service routine here
  Display.clear();

  // Init serial port for logging
  Serial.begin(115200);
 
  // Init temp sensor
  temp_prev = 85.0;
  DallasTemperature();

  // Init cooler's relay pin
  if (COOLER_ENABLED) {
    cooler_state = COOLER_OFF;
    pinMode(COOLER_PIN, OUTPUT);
    digitalWrite(COOLER_PIN, cooler_state);
  }

  // Init heater's relay pin
  if (HEATER_ENABLED) {
    heater_state = HEATER_OFF;
    pinMode(HEATER_PIN, OUTPUT);
    digitalWrite(HEATER_PIN, heater_state);
  }

  // Init periodic fan relay pin
  if (PERIODIC_ENABLED) {
    periodic_timer = 1;
    periodic_state = PERIODIC_OFF;
    pinMode(PERIODIC_PIN, OUTPUT);
    digitalWrite(PERIODIC_PIN, periodic_state);
  }

}

/*
   Main function, get and show the temperature
*/
void loop(void) {

  // Send the command to get temperatures
  Temperature.requestTemperatures();

  // Get temperature from the sensor
  double temp = (int)(Temperature.getTempCByIndex(0) * 100) / 100.0;
  if (temp > 84.9 && temp < 85.1) {

    // Skip 85°C value on sensor init
    Serial.print(".");
  }
  else {

    if (temp_prev > 84.9 && temp_prev < 85.1) {

      // Skip temp_prev = 85°C
      Serial.print("_");
      temp_prev = temp - 0.02;
    }
    else {

      double temp_delta = temp - temp_prev;
      if (temp_delta >= 0.01 || temp_delta <= -0.01) {

        if (COOLER_ENABLED) {
          if (temp >= COOLER_TEMP_HI) cooler_state = COOLER_ON;
          if (temp < COOLER_TEMP_LO) cooler_state = COOLER_OFF;
        }

        if (HEATER_ENABLED) {
          if (temp >= HEATER_TEMP_HI) heater_state = HEATER_OFF;
          if (temp <  HEATER_TEMP_LO) heater_state = HEATER_ON;
        }

        Serial.println();
        Serial.print(temp);

        temp_prev = temp;
      }

      // Periodic fan activation for inner air mixing to help temperature spread 
      if (PERIODIC_ENABLED) {
        periodic_state = 
          PERIODIC_ACTIVITY > periodic_timer % (PERIODIC_ACTIVITY + PERIODIC_INTERVAL) // interval timings
          && PERIODIC_TEMP_LO <= temp && temp < PERIODIC_TEMP_HI // check defined limits
            ? PERIODIC_ON           
            : PERIODIC_OFF;
      }

      // Current state of operations
      bool H = HEATER_ENABLED && HEATER_ON == heater_state;
      bool C = COOLER_ENABLED && COOLER_ON == cooler_state;
      bool P = PERIODIC_ENABLED && PERIODIC_ON == periodic_state;

      // Execute heater operation 
      if (HEATER_ENABLED) digitalWrite(HEATER_PIN, heater_state);

      // Execute fans(s) operations (cooling and mixing)
      if (PERIODIC_ENABLED && COOLER_ENABLED &&
          PERIODIC_PIN == COOLER_PIN) {
        
        // If a signle fan is shared for periodic inner air mixing and for cooling by external air,
        // whatever operation is active, it will be used for output for that shared pin (signals OR'ed)
        digitalWrite(COOLER_PIN, !!P && !C ? periodic_state : cooler_state);
      }
      else {
        
        // Different fans, separate signals processing
        if (COOLER_ENABLED) digitalWrite(COOLER_PIN, cooler_state);
        if (PERIODIC_ENABLED) digitalWrite(PERIODIC_PIN, periodic_state);
      }
      
      // Print symbols of current states to the log
      Serial.print(" ");
      Serial.print(C ? "C" : "_");
      Serial.print(H ? "H" : "_");
      Serial.print(P ? "P" : "_");

      // Show current temperature on the LED display
      if (C || H || P) {

        // Prefix with symbols of active operations
        int m = periodic_timer % ((C ? 1 : 0) + (H ? 1 : 0) + (P ? 1 : 0));
        
        Display.set(
          C ? (H ? (P ? (!m ? 0xC6 : (m == 1 ? 0x89 : 0x8C)) : (!m ? 0xC6 : 0x89))
                 : (P ? (!m ? 0xC6 : 0x8C) : 0xC6))
            : (H ? (P ? (!m ? 0x89 : 0x8C) : 0x89)
                 : (P ? 0x89 : 0xFF)), 3);

        // Make the decimal separator point blink to show it works not fails
        if (periodic_timer % 2 != 0) Display.digit4((int)(temp * 10)); // hide '.'
        else Display.dispFloat((double)((int)(temp * 10) / 10.0), 1); // show '.'
      }
      else {
        
        // Temperature as float with two digits after . <- make it blinking slowly
        if (periodic_timer % 2 != 0) Display.digit4((int)(temp * 100)); // hide '.'
        else Display.dispFloat(temp, 2);  // show '.'
      }
      
    }
  }

  ++periodic_timer;
  delay(1000);
}

