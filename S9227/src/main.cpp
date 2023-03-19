#include <Arduino.h>

struct PinDef
{
  PinDef(int pin)
    :Pin(pin)
  {
    if( pin < 8)
    {
      Mask = 0x1 << pin;
    }
    else if( pin < 14)
    {
      Mask = 0x1 << (pin-8);
    }
      else if( pin < A7)
    {
      Mask = 0x1 << (pin-16);
    }
  }

  int8_t Pin;
  int8_t Mask;
};

struct Pins_
{
  PinDef VideoSignal {A0};  
  PinDef EndOfScan {1};
  PinDef StartTrigger {2};
  PinDef Clock {3};
} Pins;

struct Config
{
  static constexpr int ClockInterval {0};
  static constexpr int IntegrationClocks {530};
  static constexpr int IntegrationZero {0};
  static constexpr int IntegrationLowPhase {0};
  struct PulseCycle
  {
    static constexpr int IntegreationEnd {0};

    static constexpr int StartPulseCycleInterval {0};
    static constexpr int HighPeriodTriggerClock {0};
    static constexpr int LowPeriodTriggerClock {0};
    static constexpr int VideoReadPhase {0};
  };
};

void setup() 
{
  pinMode(Pins.VideoSignal.Pin, INPUT);
  pinMode(Pins.EndOfScan.Pin, INPUT);
  pinMode(Pins.StartTrigger.Pin, OUTPUT);
  pinMode(Pins.Clock.Pin, OUTPUT);
}

void Spin(int)
{

}

void ExecuteStartPulseCycle()
{
  //int Clocks = 0;
  //bool EndOfScan = false;
  //bool StartPulseCycle = true;
  //bool HighPeriodTriggered = false;
  //bool LowPeriodTriggered = false;
  //bool VideoPeriodTriggered = false;
//
  //while(!EndOfScan)
  //{
  //  digitalWrite(Pins.Clock, LOW);
//
  //  if (StartPulseCycle)
  //  {
  //    //Begin Intrgration
  //    if (Clocks == Config::PulseCycle::HighPeriodTriggerClock)
  //    {
  //      digitalWrite(Pins::StartTrigger, HIGH);
  //    }
  //    if (Clocks == Config::PulseCycle::LowPeriodTriggerClock)
  //    {
  //      digitalWrite(Pins::StartTrigger, LOW);
  //      break;
  //    }
  //  }
//
  //  if (Clocks > Config::PulseCycle::VideoReadPhase)
  //  {
  //     EndOfScan = digitalRead(Pins::EndOfScan);
  //  }
  //};
}

bool gClockEnable = false;
bool gVideoRead = false;
int clocks = 0;
enum class ClockEdge {LowEdge, Low, HighEdge, High}ClockState;
void StartPulseCycle()
{
  PORTD |= Pins.Clock.Mask;
  delayMicroseconds(1);

  //set timer.
  //TIFR0 |= _BV(OCF0A);
  TIMSK1 |= _BV(OCIE0A);

  //Integration Time
  bool readComplete = false;
  while(readComplete == false && clocks < Config::IntegrationClocks)
  {
    if(gVideoRead)
    {
      //adc read
    }
  }
}

ISR (TIMER1_OVF_vect)
{
  if(clocks == Config::IntegrationZero)
  {
    PORTD |= Pins.StartTrigger.Mask;
  }
  else if(clocks == Config::IntegrationLowPhase)
  {
    PORTD &= ~Pins.StartTrigger.Mask;
  }
  if(gClockEnable)
  {
    PORTD |= Pins.Clock.Mask;
  }
  else
  {
    clocks++;
    PORTD &= ~Pins.Clock.Mask;
  }
    
  gClockEnable = !gClockEnable; 
}

void loop() 
{
  // Timing Diagram
  // https://www.hamamatsu.com/content/dam/hamamatsu-photonics/sites/documents/99_SALES_LIBRARY/ssd/s9227_series_kmpd1122e.pdf

  /***Clock***
   * Clock is measured falling edge to falling edge.
   * min 'Clock_freq' 50khz
   * MAX 'Clock_freq' 5MHz
   * Clock duty is 50%
  */

  /*** Imaging / Integration Cycle***
   * min 'Start Pulse Cycle' period: 530/clock_freq. 
   * MAX 'Start Pulse Cycle' period: 1100ms.
   * min 'Start Pulse high'  period: 8/clock_freq.
   * MAX 'Start Pulse high'  period: 1000ms.
   * min 'Start Pulse high'  period: 15/clock_freq.
   * MAX 'Start Pulse high'  period: 100ms.

   Start Pulse Cycle 
   * Start Pulse Cycle is the length of the imaging cycle. 
   * Duration of which = StartTrigger high period + StartTrigger low period
   * "Integration" is period of actual imaging. And is most but not all of the cycle.
   * Pressumably the other time is initialisation, and shifting data from the hold register into the output shift register.

   Integration
   * Integration begins sometime after StartTrigger goes high.
   * Integration time = StartTrigger high period + 6 clocks.

   Note:
   * The datasheet specifies; "The integration time equals the high period of ST pulse plus 6 CLK cycles."
   * But also specifies; "The integration time can be changed by changing the high-to-low ratio of ST pulses."
   * I think given the fixed start pulse cycle period, the second is just an obscure way of saying the first.
  */

  /*** Output ***
   
   Video Signal
   * Video output starts 14 clocks + 100ns after the trigger condition.
   * The trigger condition is the falling edge of Start Trigger following the high period.
   * There are 512 pixels. One pixel is read per clock.
   * The datasheet says there are two video delays per each pixel. It does not specify what these are for.
   * Video Delay 1 (ns), min 32, typical 40, max 48.
   * Video Delay 2 (ns), min 40, typical 50, max 60.
   * They appear consecutive on the timing diagram. 
   *  Delay 1 is enitrely a voltage high.
   *  Delay 2 is the falling duration to ground reference voltage.
   * Some kind of padding?
   * Signal boltage goes high (vdd referenced) after the final pixel.
   
   End Of Scan
   * Signal to confirm all bits read.
   * Occurs 39ns after the falling edge of the last clock of the previos pixel.
   * High for a single clock.
  */


}