# PWM to PPM Encoder with Arduino Nano

A lightweight Arduino Nano sketch that acts as a real-time bridge between the FlySky FS-CT6B transmitter (via FS-R6B receiver) and a Pixhawk 6C flight controller running PX4. Since PX4 does not support direct PWM input, this converter reads 6 individual PWM channels and encodes them into a single PPM stream.

---

## How It Works

The FS-R6B receiver outputs 6 separate PWM signals. The Nano reads each channel using hardware interrupts and pin change interrupts, assembles them into a standard 50Hz PPM frame using the Timer1 hardware ISR, and outputs it directly into the Pixhawk RC input pin. All timing-critical operations run inside hardware ISRs with no serial overhead, ensuring clean and stable PPM output.

---

## Wiring

| FS-R6B | Arduino Nano | Function |
|--------|-------------|----------|
| CH1    | D2          | Yaw      |
| CH2    | D3          | CH2      |
| CH3    | D4          | CH3      |
| CH4    | D5          | CH4      |
| CH5    | D6          | CH5      |
| CH6    | D7          | CH6      |
| GND    | GND         | Common Ground |
| 5V     | 5V          | Power    |

| Arduino Nano | Pixhawk 6C  |
|-------------|-------------|
| D10         | RC IN (PPM) |
| GND         | GND         |

> **Important:** GND must be shared between the receiver, Nano, and Pixhawk. Floating ground will corrupt the signal.

---

## PPM Signal Spec

| Parameter       | Value         |
|----------------|---------------|
| Frame length    | 20ms (50Hz)   |
| Sync pulse      | 300µs HIGH    |
| Channel range   | 1000–2000µs   |
| Center value    | 1500µs        |
| End frame gap   | ≥ 2700µs      |

---

## Failsafe

If the transmitter is switched off or signal is lost for more than **100ms**, the Nano automatically sends failsafe PPM values with throttle set to 900µs (below PX4 minimum threshold), triggering PX4's RC loss failsafe. This prevents the drone from holding its last known state when the controller disconnects.

---

## Interrupt Strategy

| Pin   | Channel | Interrupt Type          |
|-------|---------|------------------------|
| D2    | CH1     | Hardware INT0           |
| D3    | CH2     | Hardware INT1           |
| D4    | CH3     | Pin Change (PCINT20)    |
| D5    | CH4     | Pin Change (PCINT21)    |
| D6    | CH5     | Pin Change (PCINT22)    |
| D7    | CH6     | Pin Change (PCINT23)    |
| D10   | PPM OUT | Timer1 CTC ISR (50Hz)  |

---

## Requirements

- Arduino Nano (ATmega328P, 16MHz)
- FlySky FS-CT6B Transmitter
- FlySky FS-R6B Receiver
- Pixhawk 6C running PX4
- Arduino IDE

---

## PX4 Setup

After uploading the sketch, go to **QGroundControl**.


Then go to **Radio Setup** and calibrate all channels.

---

## Inspiration

PPM generation logic referenced from a working ESP32 + Bluepad32 implementation used to control the same Pixhawk 6C via a Bluetooth gamepad, adapted for Arduino Nano hardware timers.

---

## License

MIT
