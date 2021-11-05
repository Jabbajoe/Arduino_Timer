
/**
 * @brief SerialRelay library
 * 
 * 
 * @notes:
 * - https://github.com/RoboCore/SerialRelay
 * 
 */
#include <SerialRelay.h>
#define START_RELAY_ON

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
 * is practically unlimited (limited only by unsigned long miliseconds)
 * The accuracy is nearly perfect compared to software timers. The most 
 * important feature is they're ISR-based timers. Therefore, their executions 
 * are not blocked by bad-behaving functions / tasks.
 * This important feature is absolutely necessary for mission-critical tasks.
 * 
 * - Using ATmega328 used in UNO => 16MHz CPU clock
 * 
 */
// These define's must be placed at the beginning before #include "TimerInterrupt.h"
// _TIMERINTERRUPT_LOGLEVEL_ from 0 to 4
// Don't define _TIMERINTERRUPT_LOGLEVEL_ > 0. 
// Only for special ISR debugging only. Can hang the system.
#define TIMER_INTERRUPT_DEBUG         0
#define _TIMERINTERRUPT_LOGLEVEL_     0
// Timer0 is used for micros(), millis(), delay(), etc and can't be used
// Select Timer 1-2 for UNO, 1-5 for MEGA, 1,3,4 for 16u4/32u4
// Timer 2 is 8-bit timer, only for higher frequency
// Timer 4 of 16u4 and 32u4 is 8/10-bit timer, only for higher frequency
#define USE_TIMER_1     true

#include <TimerInterrupt.h>

#define LIGHT_HOURS         14400000  // How long before trigger ISR
#define DARK_HOURS          23400000  // How long before trigger ISR
#define TIMER1_DURATION_MS  0         // Timer1 runs forever


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
#ifdef START_RELAY_ON
  static bool toggle_relay_1 = false;
#else
  static bool toggle_relay_1 = true;
#endif

  toggle_relay = toggle_relay_1;

#if (TIMER_INTERRUPT_DEBUG > 1)
  Serial.print("ITimer1 called, millis() = "); Serial.println(millis());
#endif

  byte _data[0];
  _data[0] = 0;
  
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
      // latch timming
      delayMicroseconds(SERIAL_RELAY_DELAY_LATCH);
    else
      // shift timming
      delayMicroseconds(SERIAL_RELAY_DELAY_CLOCK_HIGH);
    digitalWrite(8, LOW);
    // it is acceptable to have 5µs delay after the last bit has been sent
    delayMicroseconds(SERIAL_RELAY_DELAY_CLOCK_LOW);
    
    mask_reset >>= 1; // update mask
  }
  // Reset to maintain LOW level when not in use
  digitalWrite(7, LOW);
  // Toggle flag in order to toggle relay in the next Trigger_relay call
  toggle_relay_1 = !toggle_relay_1;
}


void setup()
{
  Serial.begin(115200);
  while (!Serial);
  Serial.print(F("\nStarting ESTUFA on "));
  Serial.println(BOARD_TYPE);
  Serial.print(F("CPU Frequency = ")); 
  Serial.print(F_CPU / 1000000); 
  Serial.println(F(" MHz"));

  // Initialize the TimerInterrupt object
  ITimer1.init();
#ifdef START_RELAY_ON
  if (ITimer1.attachInterruptInterval(
    LIGHT_HOURS,
    Trigger_relay,
    TIMER1_DURATION_MS
  ))
#else
  if (ITimer1.attachInterruptInterval(
    DARK_HOURS,
    Trigger_relay,
    TIMER1_DURATION_MS
  ))
#endif
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
  // Persist the last toggle_relay status
  static bool toggle_relay_last = toggle_relay;

  // If there is a status change
  if (toggle_relay_last != toggle_relay) {
    // Persist the new toggle_relay status
    toggle_relay_last = toggle_relay;
    // If the new status is TRUE (i.e. Relay coil energized)
    if (toggle_relay)
      // Set interrupt to trigger after LIGHT_HOURS miliseconds
      ITimer1.setInterval(LIGHT_HOURS, Trigger_relay);
    // If the new status is FALSE (i.e. Relay coil de-energized)
    else
      // Set interrupt to trigger after DARK_HOURS miliseconds
      ITimer1.setInterval(DARK_HOURS, Trigger_relay);
  }

}