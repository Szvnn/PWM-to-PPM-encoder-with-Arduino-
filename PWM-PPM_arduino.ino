*_Latest_*

/*
 * PWM to PPM Converter - FIXED (Referenced from working ESP32 code)
 * FS-CT6B --> Arduino Nano --> Pixhawk 6C
 *
 * WIRING:
 *   FS-CT6B CH1 --> D2
 *   FS-CT6B CH2 --> D3
 *   FS-CT6B CH3 --> D4
 *   FS-CT6B CH4 --> D5
 *   FS-CT6B CH5 --> D6
 *   FS-CT6B CH6 --> D7
 *   PPM OUT     --> D10 --> Pixhawk RC IN
 *   Common GND  --> Receiver + Nano + Pixhawk
 */

#define NUM_CH          6
#define PPM_OUT         10
#define FRAME_US        20000UL   // UL to prevent overflow
#define SYNC_US         300
#define CH_DEFAULT      1500
#define SIGNAL_TIMEOUT  100000UL

const uint8_t pwmPin[NUM_CH] = {2, 3, 4, 5, 6, 7};

volatile uint16_t pwmVal[NUM_CH];
volatile uint32_t riseTime[NUM_CH];
volatile uint32_t lastPulseTime[NUM_CH];

// PPM state
volatile uint16_t ppmVals[NUM_CH];   // mirrored exactly like ESP32 code
volatile uint8_t  ppmCh     = NUM_CH; // start at end like ESP32 (triggers frame copy first)
volatile bool     ppmHigh   = false;  // tracks HIGH/LOW phase like ESP32
volatile bool     txActive  = false;

// ── ISR for CH1 (D2) ─────────────────────────────────────────────────────────

void isr0() {
  uint32_t now = micros();
  if (PIND & (1 << 2)) {
    riseTime[0] = now;
  } else {
    uint32_t w = now - riseTime[0];
    if (w > 800 && w < 2200) {
      pwmVal[0]        = w;
      lastPulseTime[0] = now;
    }
  }
}

// ── ISR for CH2 (D3) ─────────────────────────────────────────────────────────

void isr1() {
  uint32_t now = micros();
  if (PIND & (1 << 3)) {
    riseTime[1] = now;
  } else {
    uint32_t w = now - riseTime[1];
    if (w > 800 && w < 2200) {
      pwmVal[1]        = w;
      lastPulseTime[1] = now;
    }
  }
}

// ── PCINT for CH3-CH6 (D4-D7) ────────────────────────────────────────────────

ISR(PCINT2_vect) {
  uint32_t now = micros();
  static uint8_t lastPIND = 0;
  uint8_t curr    = PIND;
  uint8_t changed = curr ^ lastPIND;
  lastPIND        = curr;

  for (uint8_t i = 2; i < NUM_CH; i++) {
    uint8_t bit = i + 2;              // D4=bit4 ... D7=bit7
    if (changed & (1 << bit)) {
      if (curr & (1 << bit)) {
        riseTime[i] = now;
      } else {
        uint32_t w = now - riseTime[i];
        if (w > 800 && w < 2200) {
          pwmVal[i]        = w;
          lastPulseTime[i] = now;
        }
      }
    }
  }
}

// ── Timer1 ISR: PPM generator (logic mirrored from ESP32 ppmISR) ──────────────
//
//  ESP32 logic:
//    if (!ppmHigh) → go HIGH, set timer to SYNC_US
//    if ( ppmHigh) → go LOW,  set timer to (channelVal - SYNC_US) or end gap
//
//  Same logic here. Timer1 prescaler=8 → 1 tick = 0.5µs → multiply µs by 2

ISR(TIMER1_COMPA_vect) {
  TCNT1 = 0;                           // reset counter (like timerWrite(0) in ESP32)

  if (!ppmHigh) {
    // ── Go HIGH (sync pulse start) ────────────────────────────────────────────
    digitalWrite(PPM_OUT, HIGH);
    ppmHigh = true;
    OCR1A   = (uint16_t)(SYNC_US * 2); // stay HIGH for SYNC_US

  } else {
    // ── Go LOW (gap after sync) ───────────────────────────────────────────────
    digitalWrite(PPM_OUT, LOW);
    ppmHigh = false;

    if (ppmCh < NUM_CH) {
      // Channel gap = channelVal - SYNC_US  (exactly like ESP32)
      uint16_t gapUs = ppmVals[ppmCh] - SYNC_US;
      OCR1A = (uint16_t)(gapUs * 2);
      ppmCh++;

    } else {
      // End of frame: calculate remaining time (like ESP32)
      uint32_t used = SYNC_US;         // opening sync
      for (uint8_t i = 0; i < NUM_CH; i++)
        used += ppmVals[i];            // each channel = SYNC + gap

      uint32_t endGap = FRAME_US - used;
      if (endGap < 2700) endGap = 2700; // PX4 needs >2700us to detect frame end

      OCR1A = (uint16_t)(endGap * 2);

      // Copy fresh PWM values for next frame
      if (txActive) {
        for (uint8_t i = 0; i < NUM_CH; i++)
          ppmVals[i] = constrain(pwmVal[i], 1000, 2000);
      } else {
        // Failsafe
        ppmVals[0] = 1500;
        ppmVals[1] = 1500;
        ppmVals[2] = 900;   // throttle zero → PX4 failsafe
        ppmVals[3] = 1500;
        ppmVals[4] = 1500;
        ppmVals[5] = 1500;
      }

      ppmCh = 0;            // reset channel counter
    }
  }
}

// ── TX alive check ────────────────────────────────────────────────────────────

bool checkTxActive() {
  uint32_t now = micros();
  for (uint8_t i = 0; i < NUM_CH; i++)
    if ((now - lastPulseTime[i]) > SIGNAL_TIMEOUT) return false;
  return true;
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);

  for (uint8_t i = 0; i < NUM_CH; i++) {
    pwmVal[i]        = CH_DEFAULT;
    ppmVals[i]       = CH_DEFAULT;
    lastPulseTime[i] = micros();
    pinMode(pwmPin[i], INPUT);
  }

  attachInterrupt(digitalPinToInterrupt(2), isr0, CHANGE);
  attachInterrupt(digitalPinToInterrupt(3), isr1, CHANGE);

  PCICR  |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT20) | (1 << PCINT21) | (1 << PCINT22) | (1 << PCINT23);

  pinMode(PPM_OUT, OUTPUT);
  digitalWrite(PPM_OUT, LOW);

  // Timer1: CTC mode, prescaler 8 → 0.5µs per tick
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;
  TCCR1B |= (1 << WGM12);   // CTC mode
  TCCR1B |= (1 << CS11);    // prescaler 8
  OCR1A   = SYNC_US * 2;    // first interrupt after SYNC_US
  TIMSK1 |= (1 << OCIE1A);
  interrupts();
}

// ── Loop ─────────────────────────────────────────────────────────────────────

void loop() {
  txActive = checkTxActive();
  // Temprorary Serial monitor
  // static uint32_t t = 0;
  // if (millis() - t > 500) {
  //   t = millis();
  //   Serial.print(txActive ? "TX:ON  " : "TX:OFF  ");
  //   for (uint8_t i = 0; i < NUM_CH; i++) {
  //     Serial.print("CH"); Serial.print(i + 1);
  //     Serial.print(":"); Serial.print(pwmVal[i]);
  //     Serial.print("  ");
  //   }
  //   Serial.println();
  // }
}