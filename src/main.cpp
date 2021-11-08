
/**
 * @brief DEBUG MODE
 * For DEBUG, uncomment this line 
 * 
 */
//#define DEBUG_MODE


/**
 * @brief USER's LIGHT SCHEDULE
 * Define the light patterns you want to use
 * 
 * LIGHT_HOURS: How many hours of light
 * DARK_HOURS: How many hours of dark
 * 
 */
#define LIGHT_HOURS 18  // Hours with lights on
#define DARK_HOURS  6   // Hours with lights off
// If the relay must start activated, uncomment this line
#define START_RELAY_ON


/**
 * @brief SerialRelay library
 * 
 * 
 * @notes:
 * - https://github.com/RoboCore/SerialRelay
 * 
 */
#include <SerialRelay.h>

// Which Arduino pin is connected to relay board's Data and Clock
#define RELAY_DATA 7
#define RELAY_CLK 8
// Number of Relay board modules connected one in each other
const int NumModules = 1;    // maximum of 10
// Create SerialRelay object 
// (data, clock, number of modules)
SerialRelay relays(RELAY_DATA, RELAY_CLK, NumModules);

#ifdef START_RELAY_ON
  volatile bool toggle_relay = true;
#else
  volatile bool toggle_relay = false;
#endif


/**
 * @brief TimerInterrupt library
 * 
 * 
 * @notes:
 * - Now we can use these new 16 ISR-based timers, while consuming 
 * only 1 hardware Timer. Their independently-selected, maximum interval 
 * is practically unlimited (limited only by unsigned long milliseconds)
 * The accuracy is nearly perfect compared to software timers. The most 
 * important feature is they're ISR-based timers. Therefore, their executions 
 * are not blocked by bad-behaving functions / tasks.
 * This important feature is absolutely necessary for mission-critical tasks.
 * 
 * - Using ATmega328 used in UNO => 16MHz CPU clock
 * 
 */
// This #define must be placed at the beginning before #include "TimerInterrupt.h"
// _TIMERINTERRUPT_LOGLEVEL_ from 0 to 4
// Don't define _TIMERINTERRUPT_LOGLEVEL_ > 0. 
// Only for special ISR debugging only. Can hang the system.
#define _TIMERINTERRUPT_LOGLEVEL_     0
// Timer0 is used for micros(), millis(), delay(), etc and can't be used
// Select Timer 1-2 for UNO, 1-5 for MEGA, 1,3,4 for 16u4/32u4
// Timer 2 is 8-bit timer, only for higher frequency
// Timer 4 of 16u4 and 32u4 is 8/10-bit timer, only for higher frequency
#define USE_TIMER_1     true

#include <TimerInterrupt.h>

#define TIMER_TRIGGER_MS    3600000 // How long before trigger ISR
#define TIMER1_DURATION_MS  0       // Timer1 runs forever


/**
 * @brief Interrupt Service Routine (ISR) for Timer1
 * 
 * 
 * @notes:
 * - To share data between interrupt code and the rest of your program, 
 * variables usually need to be "volatile" types. 
 * Volatile tells the compiler to avoid optimizations that assume variable 
 * can not spontaneously change. Because your function may change variables 
 * while your program is using them, the compiler needs this hint. 
 * But volatile alone is often not enough.
 * When accessing shared variables, usually interrupts must be disabled. 
 * Even with volatile, if the interrupt changes a multi-byte variable 
 * between a sequence of instructions, it can be read incorrectly.
 * If your data is multiple variables, such as an array and a count, usually 
 * interrupts need to be disabled or the entire sequence of your code which 
 * accesses the data.
 *  
 */
void Trigger_relay()
{
  static uint8_t hours = 0;
  hours++;

#ifdef START_RELAY_ON
  static bool toggle_relay_1 = true;
#else
  static bool toggle_relay_1 = false;
#endif

  if (
    (toggle_relay_1 == true && hours == LIGHT_HOURS) || 
    (toggle_relay_1 == false && hours == DARK_HOURS)
    )
    {
      // Reset couting hours
      hours = 0;
      // Update toggle_relay
      toggle_relay_1 = !toggle_relay_1;

      #ifdef DEBUG_MODE
        digitalWrite(LED_BUILTIN, toggle_relay_1);
      #endif

      /**
       * @brief Arduino-Relay communication
       * Serial communication via I2C protocol
       * 
       */
      byte _data[0];
      _data[0] = 0;
      // Define the command to be sent to relay (Turn on or Turn off)
      if(toggle_relay_1){
        byte mask = (1 << (0));
        _data[0] |= mask;
      } else {
        byte mask = ~(1 << (0));
        _data[0] &= mask;
      }
      // send I2C pulse
      byte mask_reset = 0x80; // reset
      for(int i=1 ; i <= 8 ; i++){
        // set Data line
        if(_data[0] & mask_reset)
          digitalWrite(7, HIGH);
        else
          digitalWrite(7, LOW);
        // delay between Data and Clock signals
        delayMicroseconds(SERIAL_RELAY_DELAY_DATA);
        // set Clock line
        digitalWrite(8, HIGH); // rising edge
        if(i == 8)
          // latch timing
          delayMicroseconds(SERIAL_RELAY_DELAY_LATCH);
        else
          // shift timing
          delayMicroseconds(SERIAL_RELAY_DELAY_CLOCK_HIGH);
        digitalWrite(8, LOW);
        // it is acceptable to have 5µs delay after the last bit has been sent
        delayMicroseconds(SERIAL_RELAY_DELAY_CLOCK_LOW);
        
        mask_reset >>= 1; // update mask
      }
      // Reset to maintain LOW level when not in use
      digitalWrite(7, LOW);
    }
}


void setup()
{
  /**
   * @note
   * - "Opening the serial port (starting serial monitor) auto resets 
   * the Arduino. 
   * You can temporally disable the auto reset by putting a 10uf capacitor from 
   * reset to ground or, on some Arduinos, there is a jumper on the PCB that 
   * you can cut. You will need to re-enable the auto reset or manually reset 
   * the board to upload new code, though"
   * [source: 
   * https://forum.arduino.cc/t/millis-resets-every-time-i-open-the-serial-monitor/518635]
   * 
   * "The Arduino uses the RTS (Request To Send) (and I think 
   * DTR (Data Terminal Ready)) signals to auto-reset. 
   * If you get a serial terminal that allows you to change the flow control 
   * settings you can change this functionality."
   * [source: 
   * https://arduino.stackexchange.com/questions/439/why-does-starting-the-serial-monitor-restart-the-sketch]
   * 
   */
  Serial.begin(115200);
  while (!Serial);
  Serial.println("#WARNING: ARDUINO HAS BEEN RESET");
  Serial.print(F("\nStarting ESTUFA on "));
  Serial.println(BOARD_TYPE);
  Serial.print(F("CPU Frequency = ")); 
  Serial.print(F_CPU / 1000000); 
  Serial.println(F(" MHz"));

// DEBUG ONLY
#ifdef DEBUG_MODE
  // Config Arduino LED
  pinMode(LED_BUILTIN, OUTPUT);
  #ifdef START_RELAY_ON
    digitalWrite(LED_BUILTIN, HIGH);
  #else
    digitalWrite(LED_BUILTIN, LOW);
  #endif
#endif

  // Initialize the TimerInterrupt object
  ITimer1.init();
  if (ITimer1.attachInterruptInterval(
    TIMER_TRIGGER_MS,
    Trigger_relay,
    TIMER1_DURATION_MS
  ))
  {
    Serial.print(F("Starting  ITimer1 OK, millis() = ")); 
    Serial.println(millis());
  }
  else
    Serial.println(F("Can't set ITimer1"));

  // Initialize all relays OFF
  for(int i=1 ; i <= NumModules ; i++){
    for(int j=1 ; j <= 4 ; j++){
      relays.SetRelay(j, SERIAL_RELAY_OFF, i);
      delay(1000);
    }
  }

#ifdef START_RELAY_ON
  relays.SetRelay(1, SERIAL_RELAY_ON, 1); // Start with relay on
#endif

}

void loop()
{
}