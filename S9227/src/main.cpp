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
#include <Arduino.h>

namespace Config
{
  static constexpr uint16_t IntegrationClocks {530};
  static constexpr uint16_t IntegrationLowPhase {300}; //20
  static constexpr uint16_t IntegrationZero {0};
  static constexpr uint16_t PixelCount {512};
};

struct PinDef
{
  PinDef(int8_t pin, int8_t portOffset=0)
	: Pin(pin)
	, Mask(0x1 << (pin-portOffset))
	{}
	int8_t Pin;
	int8_t Mask;
};

namespace Pins
{
	PinDef VideoSignal {A0, 16};  
	PinDef EndOfScan {2};
	PinDef StartTrigger {3};
	PinDef Clock {4};
};

struct VideoBuffer
{
	uint8_t Buffer[Config::PixelCount];
	uint16_t BufferIndex{0};
} gVideoBuffer;

void setup() 
{
	pinMode(Pins::VideoSignal.Pin,	INPUT);
	pinMode(Pins::EndOfScan.Pin,	INPUT);
	pinMode(Pins::StartTrigger.Pin,	OUTPUT);
	pinMode(Pins::Clock.Pin,		OUTPUT);

	Serial.begin(9600);
  	Serial.println("init");

  	delay(1000);
}

bool gClockEnable = false;
bool gVideoRead = false;
uint16_t gVideoReadClock = UINT16_MAX;
uint16_t gClocks = 0;

void StartPulseCycle()
{
	//reset state
	gClocks = 0;
	gVideoRead = false;
	gClockEnable = false;
	gVideoBuffer.BufferIndex = 0;
	gVideoReadClock = Config::IntegrationLowPhase + 14;


	//timer logic begins from falling edge, so ensure when we start the clock
	//pin is high.
	PORTD |= Pins::Clock.Mask;
	delayMicroseconds(1);

	cli();

	//Timer ~
	// n.b. manual manipulation of timer1 will largely criple pins 9 and 10.
	// configure timer.
	TCCR1A = 0;           //Compare Output Mode - Off
	TCCR1B &= ~(_BV(CS10)|_BV(CS11)|_BV(CS12));
	TCCR1B |= _BV(WGM20)|_BV(CS10);   //Compare Match Mode - On, no prescalar.

	//zero the timer.
	TCNT1 = 0;

	//enable the timer.
	//OCR1A = 161; //trigger at 161 clocks 62ns * 161 ~= 10us.
	OCR1A = 70; //trigger at 161 clocks 62ns * 161 ~= 10us.
	TIMSK1 |= _BV(OCIE1A);

	sei();

	while(true)
	{
		if(gVideoRead)
		{
			cli();
			gVideoRead = false;

			//configure adc prescalar, low resolution sample.
			ADCSRA &= ~(_BV(ADPS0)|_BV(ADPS2)|_BV(ADPS2));
			ADCSRA |= _BV(ADPS0);

			//read adc
			gVideoBuffer.Buffer[gVideoBuffer.BufferIndex++] = analogRead(Pins::VideoSignal.Pin);
			gVideoRead = false;

			if(gVideoBuffer.BufferIndex >= Config::PixelCount)
			{
				PORTD |= Pins::StartTrigger.Mask;
				PORTD |= Pins::Clock.Mask;
				TIMSK1 &= ~_BV(OCIE1A);

				return;
			}

			sei();
		}
		TIMSK1 |= _BV(OCIE1A);
	}

	//disable timer.
	TIMSK1 &= ~_BV(OCIE1A);
	PORTD &= ~Pins::Clock.Mask;
}

ISR (TIMER1_COMPA_vect)
{
	//integration period
	if(gClocks == Config::IntegrationZero)
	{
		PORTD |= Pins::StartTrigger.Mask;
	}
	else if(gClocks == Config::IntegrationLowPhase)
	{
		PORTD &= ~Pins::StartTrigger.Mask;
	}

	//clock signal
	if(gClockEnable)
	{
		PORTD |= Pins::Clock.Mask;
		OCR1A = 70;
	}
	else
	{
		gClocks++;
		PORTD &= ~Pins::Clock.Mask;
		OCR1A = 50;

		//permit pixel read.
		gVideoRead = gClocks >= gVideoReadClock;
	}

	gClockEnable = !gClockEnable; 
	TCNT1 = 0;
	TIMSK1 &= ~_BV(OCIE1A);
}

void loop() 
{
  //PORTD |= B00000100;
  //PORTD |= Pins::Clock.Mask;
  //digitalWrite(1, HIGH);
  //digitalWrite(2, HIGH);
  	//return; 

	StartPulseCycle();
	delayMicroseconds(1000);

	delay(1000);

	for(uint16_t pixel = 0; pixel < Config::PixelCount; ++pixel )
	{
		Serial.print(gVideoBuffer.Buffer[pixel]);
		Serial.print(", ");
	}
	Serial.println("END");
  	while(true)
  	{
		delayMicroseconds(1000);
	}
}