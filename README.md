# Tone Player/Recorder System
An embedded system that allows you to play different tones using the keypad and record a sequence of tones that can be played back later.
[Video Demo](https://youtu.be/ymebTnf5Pa0)

## Description
The system has three modes: play, record, and playback. The user can switch between the modes at any time, and the current mode is indicated by three LED's.

### PLAY Mode
When a keypad button 1 through 8 is pressed, a note is played (each button is associated with a different pitch). Press the 'A' button to switch to this mode.

### RECORD Mode
When a keypad button 1 through 8 is pressed, a note is played, and information about it is stored on the external EEPROM (so that it can be played back later). Press the 'B' button to switch to this mode.

### PLAYBACK Mode
When the 'D' button is pressed, the recorded sequence is played back. Press the 'C' button to switch to this mode.

## Hardware Components
- [Texas Instruments Tiva C Series TM4C123G LaunchPad](https://www.ti.com/tool/EK-TM4C123GXL)
- [Microchip 24LC32A EEPROM](https://www.microchip.com/en-us/product/24lc32a)
- [Parallax 4x4 Matrix Membrane Keypad](https://www.parallax.com/product/4x4-matrix-membrane-keypad/)
- Speaker
- LED's
- Breadboard, jumper wires, and resistors

## Hardware Setup
- Wire PB2 to SCL pin of EEPROM
- Wire PB3 to SDA pin of EEPROM
- Wire PB6 to speaker
- Wire PC4-PC7 to the column pins of keypad
- Wire PD1, PD2, PD3 to three separate LED's
- Wire PE0-PE3 to the row pins of keypad
