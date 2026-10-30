# Digitech Dynamo – Smart Washing Machine

Arduino smart washing machine controller project.

## Hardware and libraries shown in the sketch
- Arduino Mega 2560 (the sketch uses digital pins 22–31)
- 128×64 SSD1306 I2C OLED, address `0x3C`
- Four push buttons: ENTER, BACK, UP, DOWN
- Lid switch
- Buzzer
- Relay outputs: WATER, WASH, DRAIN, SPIN
- Arduino libraries: Adafruit GFX Library and Adafruit SSD1306

## Pin mapping
| Function | Arduino pin |
|---|---:|
| ENTER button | 22 |
| BACK button | 23 |
| UP button | 24 |
| DOWN button | 25 |
| Lid switch | 26 |
| Buzzer | 27 |
| Water relay | 28 |
| Wash relay | 29 |
| Drain relay | 30 |
| Spin relay | 31 |
| OLED SDA | Mega SDA (pin 20) |
| OLED SCL | Mega SCL (pin 21) |

## Important safety note
This sketch controls relay outputs intended for a washing-machine prototype. Do not connect mains voltage or a real washing-machine motor/pump until the circuit, relay/contactor ratings, isolation, grounding, lid interlock, and emergency-stop design have been checked by a qualified person. Test first with low-voltage indicator loads.

## Upload
Open `Smart_Washing_Machine.ino` in Arduino IDE. Select **Arduino Mega or Mega 2560**, choose the correct COM port, install the Adafruit GFX and Adafruit SSD1306 libraries, then compile and upload.

This repository contains the sketch as supplied. It has not been hardware-tested or independently verified.
