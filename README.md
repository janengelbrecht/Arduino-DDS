# Arduino-DDS
AD9850-Based 30MHz Signal Generator with SCPI Remote Control
1. System Introduction

The AD9850 Professional DDS Signal Generator is a precision frequency synthesis instrument designed for laboratory, 
educational, and hobbyist applications requiring stable and accurate sine wave signals from DC to 30 MHz. 
This project represents a complete, standalone signal generation solution that combines the flexibility of 
Direct Digital Synthesis (DDS) technology with a professional-grade user interface and industry-standard remote control capabilities.

At its core, the system utilizes the Analog Devices AD9850 DDS module—a complete digitally programmable frequency 
synthesizer capable of generating spectrally pure sine waves with 32-bit frequency resolution. 
The instrument is built around the ubiquitous Arduino UNO platform, making it accessible for modification and 
customization while delivering performance that rivals commercial benchtop generators in its frequency range.

What distinguishes this project from typical DDS implementations is its dual-mode operation: it functions as a 
fully self-contained bench instrument with intuitive local controls while simultaneously offering a complete SCPI remote 
control interface. This SCPI compliance—rare in DIY projects—allows the generator to be integrated into automated test systems, 
controlled from MATLAB, Python, or LabVIEW, and operated exactly like professional test equipment.

The system includes comprehensive memory management with automatic frequency storage, IF offset capability for receiver applications, 
and a versatile tuning step selection from 10 Hz to 1 MHz. The RF output stage features a variable attenuator and 
grounded-collector amplifier, providing a usable 50-ohm compatible signal at a BNC connector.
2. Hardware Architecture
2.1 System Overview

The hardware architecture follows a modular design philosophy with five major subsystems:

    Processing Core: Arduino UNO with ATmega328P microcontroller

    Frequency Synthesis Engine: AD9850 DDS Module

    User Interface: 16x2 LCD display, rotary encoder, push buttons

    RF Output Stage: Variable attenuator and BF198 amplifier

    Remote Interface: USB-to-serial for SCPI command processing

The processing core orchestrates all activities: reading the rotary encoder, updating the LCD, sending 
frequency words to the DDS, monitoring buttons, and processing incoming SCPI commands from the serial port. 
Local and remote control operate concurrently.
2.2 Processing Core - Arduino UNO

The Arduino UNO provides:

    8-bit AVR processor running at 16 MHz

    32 KB Flash memory for program storage

    2 KB SRAM for runtime variables

    1 KB EEPROM for non-volatile frequency storage

    14 digital I/O pins and 6 analog inputs

Pin Allocation:

DDS Control (4 pins):

    Pin 8 (W_CLK) - Word Load Clock for serial data transfer

    Pin 9 (FQ_UD) - Frequency Update to latch new frequency

    Pin 10 (RESET) - DDS initialization

    Pin 11 (DATA) - Serial data output, LSB first

LCD Interface (6 pins, 4-bit mode):

    Pin 12 (RS) - Register Select for command or data

    Pin 13 (E) - Enable strobe

    Pin 7 (D4) - Data bit 4

    Pin 6 (D5) - Data bit 5

    Pin 5 (D6) - Data bit 6

    Pin 4 (D7) - Data bit 7

User Input (4 pins):

    Pin 2 - Rotary encoder channel A (interrupt-capable)

    Pin 3 - Rotary encoder channel B (interrupt-capable)

    Pin 4 - Encoder push button (shared with LCD D7)

    Pin A0 - Increment selection button

    Pin A5 - IF offset toggle button

The encoder uses pin-change interrupts on pins 2 and 3, ensuring no rotation steps are missed even during 
LCD updates or SCPI command parsing. This provides real-time responsiveness essential for smooth tuning.
2.3 Frequency Synthesis - AD9850 DDS Module

The AD9850 implements Direct Digital Synthesis technology with these specifications:

    125 MHz internal reference clock

    32-bit frequency tuning word providing resolution of approximately 0.0291 Hz

    5-bit phase modulation capability (11.25 degree increments)

    10-bit built-in DAC with complementary current outputs

    Serial or parallel programming interface

DDS Operating Principle:

The AD9850 generates frequencies by constructing a sine wave numerically. Inside the chip, 
a 32-bit phase accumulator increments at each clock cycle by the value of the frequency tuning word. 
When the accumulator reaches its maximum value of 2 to the 32nd power and overflows, 
it has completed one full sine wave cycle.

The output frequency is determined by this formula:

    Output frequency equals (tuning word value times reference clock frequency) divided by 2 to the 32nd power

To calculate the tuning word for a desired frequency:

    Tuning word equals (desired frequency times 2 to the 32nd power) divided by reference clock frequency

For example, to generate 10 MHz with a 125 MHz clock:

    Multiply 10,000,000 by 4,294,967,296

    Divide the result by 125,000,000

    This yields approximately 2,748,779 as the 32-bit tuning word

The phase accumulator output addresses a sine lookup table in ROM, converting phase to digital amplitude values. 
These feed the 10-bit DAC, producing the analog sine wave output. 
This all-digital process ensures exceptional frequency stability determined entirely by the reference clock.

Programming Sequence:

    Pulse RESET to initialize internal state

    Pulse W_CLK to prepare for serial mode

    Pulse FQ_UD to enable serial loading

    Send 40 bits LSB first (32 frequency bits plus 8 control bits)

    Pulse FQ_UD to update output frequency

The control byte format includes:

    Bits 0-4: Phase modulation (set to 0)

    Bit 5: Reserved (must be 0)

    Bit 6: Power-down control (0 for normal operation)

    Bit 7: Reserved (must be 0)

2.4 User Interface Components
2.4.1 LCD Display

A 16x2 character LCD with HD44780 controller operates in 4-bit mode to save pins. Display format:

Line 0 (Frequency): Shows current frequency with thousand separators:

    Format: "X.XXX.XXX MHz" for frequencies below 10 MHz

    Format: "XX.XXX.XXX MHz" for frequencies 10 MHz and above

    Decimal points placed every three digits for readability

Line 1 (Status): Shows tuning increment and IF offset indicator:

    Tuning step display centered (e.g., "10 Hz", "1 kHz", "2.5 kHz")

    Small dot in bottom-right corner when IF offset active

2.4.2 Rotary Encoder

A quadrature encoder with integrated push button provides tuning control:

    Rotation: Increases or decreases frequency by the selected step

    Clockwise rotation: Increases frequency

    Counter-clockwise rotation: Decreases frequency

    Push button: Toggles IF offset mode on/off

The encoder generates two square waves 90 degrees out of phase. Direction is determined by which channel 
changes first—channel A leading indicates clockwise rotation, channel B leading indicates counter-clockwise.
2.4.3 Control Buttons

Two momentary push buttons with internal pull-ups provide additional control:

Increment Button (Pin A0):

    Cycles through tuning steps in this circular sequence:
    10 Hz → 50 Hz → 100 Hz → 500 Hz → 1 kHz → 2.5 kHz → 5 kHz → 10 kHz → 100 kHz → 1 MHz → back to 10 Hz

    Each press advances to the next step

IF Offset Button (Pin A5):

    Momentary action: enables IF offset while pressed, disables on release

    Works in conjunction with encoder button toggling

2.5 RF Output Stage

The RF output stage conditions the DDS signal for practical use:
2.5.1 Variable Attenuator

A potentiometer connected directly to the AD9850 output provides:

    Continuous amplitude adjustment from maximum down to near zero

    Simple impedance matching between DDS output and amplifier input

    DC blocking inherently through the potentiometer configuration

Configuration:

    Top terminal: AD9850 output pin

    Bottom terminal: Ground

    Wiper: Output to amplifier stage

2.5.2 Amplifier Stage

A grounded-collector (common collector) amplifier based on the BF198 NPN RF transistor provides:

    Current gain to drive 50-ohm loads

    Impedance transformation from high-impedance DDS output to low-impedance system

    Broadband operation from DC to beyond 30 MHz

Circuit features:

    AC coupling through capacitor from attenuator wiper to transistor base

    Appropriate biasing resistors for stable DC operating point from +5V supply

    Output coupling capacitor to prevent DC on BNC connector

    BNC female chassis connector for standard test leads

The BF198 transistor is specifically chosen for RF applications up to VHF frequencies, 
providing good gain and linearity across the entire 30 MHz operating range.

3. Software Architecture
3.1 Firmware Overview

The firmware is written in C++ using the Arduino framework, organized into modular functional blocks:

    Hardware Abstraction Layer (HAL): Low-level drivers for AD9850 and LCD

    User Interface (UI): Display management and input processing

    Memory Management: EEPROM read/write with validation

    SCPI Parser: Command processing for remote control

    Main Control Loop: System coordination and timing

Total code size is approximately 30 KB, utilizing about 60% of available Flash memory.
3.2 Main Program Flow

Initialization Sequence (setup function):

    Configure input pins with internal pull-ups

    Initialize LCD display

    Configure pin-change interrupts for encoder

    Initialize AD9850 hardware

    Load frequency from EEPROM (or use 7.050 MHz default)

    Set initial increment display to 10 Hz

    Initialize SCPI subsystem and serial communication

Main Loop (continuous):

    Check encoder rotation and update frequency by current step

    Apply frequency limits (0 Hz to 30 MHz)

    Update LCD and AD9850 if frequency changed

    Process increment button for step cycling

    Process IF offset button for momentary offset

    Process encoder button for offset toggling

    Handle delayed EEPROM write (2 seconds after last change)

    Process incoming SCPI commands from serial

3.3 Frequency Control

The frequency calculation uses 64-bit arithmetic through double-precision floating point to prevent overflow:

    Tuning word = (frequency × 4,294,967,296) ÷ reference clock

    Reference clock defaults to 125,000,000 Hz

    Result is a 32-bit integer sent to AD9850

When IF offset is active:

    Output frequency = displayed frequency - intermediate frequency

    Default intermediate frequency is 0 Hz

    Visual indicator appears on LCD when active

3.4 Memory Management

EEPROM storage uses a robust format to prevent reading invalid data:

Memory Map:

    Address 0: Magic byte (0xAA) - validates data is from this application

    Address 1: Version byte (0x01) - allows format changes in future

    Addresses 2-5: 32-bit frequency value (MSB first)

Validation Process:

    On startup, reads magic byte and version

    If both match expected values, reconstructs frequency

    If invalid, initializes with default 7.050 MHz

Write Strategy:

    Records timestamp of last frequency change

    Writes to EEPROM only after 2 seconds of stability

    Prevents excessive writes from rapid tuning

    Extends EEPROM lifespan (rated for ~100,000 writes)

3.5 Interrupt Handling

The rotary encoder uses pin-change interrupts for instant response:

    PCINT2_vect services pins D2 and D3

    Calls encoder library service routine on each pin change

    Updates internal encoder position counter

    Main loop reads accumulated changes and applies to frequency

This interrupt-driven approach ensures no encoder steps are missed, even during LCD updates or SCPI command parsing.
4. SCPI Remote Control Implementation
4.1 SCPI Standard Overview

SCPI (Standard Commands for Programmable Instruments) is an IEEE standard that defines a common command 
set for test and measurement equipment. It provides:

    Standardized command structure across different instruments

    Hierarchical command trees for intuitive organization

    Consistent parameter formats including units

    Common queries for identification and status

The implemented subset makes this generator compatible with commercial test software and automation systems.
4.2 Communication Interface

Physical Layer:

    Hardware serial port (UART) via USB connection

    Baud rate: 115200

    Data bits: 8

    Parity: None

    Stop bits: 1

    Flow control: None

Message Format:

    Commands terminated by LF (line feed) or CR+LF

    Case-insensitive parsing

    Support for both short and long command forms

    Units accepted: Hz, kHz, MHz, GHz

4.3 Implemented Command Subsystems
4.3.1 IEEE 488.2 Common Commands

These fundamental commands provide basic instrument control:
Command	Function	Response/Behavior
*IDN?	Identification query	Returns "AD7C,AD9850 Generator,4.1,SCPI-1.0"
*RST	Reset to default state	Sets 7.050 MHz, 10 Hz step, output on
*OPC	Operation complete	Sets operation complete bit
*OPC?	Operation complete query	Returns "1"
*WAI	Wait to continue	No action (commands complete immediately)
*TST?	Self-test query	Returns 0 if pass, non-zero if fail
*CLS	Clear status	Clears status registers and error queue
*ESE <value>	Event status enable	Sets enable register
*ESE?	Event status enable query	Returns current value
*ESR?	Event status register query	Returns and clears register
*SRE <value>	Service request enable	Sets enable register
*SRE?	Service request enable query	Returns current value
*STB?	Status byte query	Returns status byte
4.3.2 Frequency Subsystem

The FREQuency subsystem provides complete frequency control:
Command	Description	Example
:FREQuency <value> [UNIT]	Set output frequency	FREQ 10 MHz
:FREQuency?	Query current frequency	Returns "10000000"
:FREQuency:STEP <value> [UNIT]	Set tuning step	FREQ:STEP 1 kHz
:FREQuency:STEP?	Query tuning step	Returns step in Hz
:FREQuency:LIMIT? MIN	Query minimum frequency	Returns "0"
:FREQuency:LIMIT? MAX	Query maximum frequency	Returns "30000000"
:FREQuency:OFFSet <value> [UNIT]	Set IF offset	FREQ:OFFS 455 kHz
:FREQuency:OFFSet?	Query IF offset	Returns offset value
:FREQuency:OFFSet:STATe <0/1>	Enable/disable offset	FREQ:OFFS:STAT 1
:FREQuency:OFFSet:STATe?	Query offset state	Returns 0 or 1
4.3.3 Output Subsystem

Controls the signal output state:
Command	Description	Example
:OUTPut <0/1/OFF/ON>	Enable/disable output	OUTP ON
:OUTPut?	Query output state	Returns 1 if enabled
:OUTPut:PROTection?	Query protection status	Returns 0 (not tripped)
4.3.4 Display Subsystem

Manages the LCD display content:
Command	Description	Example
:DISPlay:TEXT <string>	Show custom text on line 1	DISP:TEXT "Test Mode"
:DISPlay:TEXT:CLEar	Clear custom text	DISP:TEXT:CLE
:DISPlay:CONTrast <value>	Set contrast (placeholder)	DISP:CONT 50
:DISPlay:CONTrast?	Query contrast	Returns "50"
4.3.5 System Subsystem

Provides system-level information and control:
Command	Description	Example
:SYSTem:ERRor?	Get next error from queue	Returns error string
:SYSTem:VERSion?	Query SCPI version	Returns "1999.0"
:SYSTem:PRESet	Preset to default values	SYST:PRES
:SYSTem:BEEPer <0/1>	Enable/disable beeper	SYST:BEEP 1
:SYSTem:BEEPer?	Query beeper state	Returns 0 or 1
4.3.6 Memory Subsystem

Provides 10 non-volatile memory locations:
Command	Description	Example
:MEMory:STORe <location>	Store current settings	MEM:STOR 1
:MEMory:RECall <location>	Recall stored settings	MEM:REC 1
:MEMory:CLEar <location>	Clear memory location	MEM:CLE 1
4.3.7 Calibration Subsystem

Password-protected calibration for reference clock adjustment:
Command	Description	Example
:CALibration:REFerence <value> [UNIT]	Set reference clock	CAL:REF 125 MHz
:CALibration:REFerence?	Query reference clock	Returns value in Hz
:CALibration:STORe	Store calibration to memory	CAL:STOR
:CALibration:SECure:CODE <code>	Enter password to unlock	CAL:SEC:CODE 1234
4.4 Error Handling

The system maintains an SCPI-compliant error queue with capacity for 10 errors. Standard error codes include:
Code	Description
0	No error
-100	Command error
-102	Invalid parameter
-131	Invalid suffix
-138	Suffix not allowed
-108	Parameter not allowed
-109	Missing parameter
-225	Out of memory
-230	Output protection tripped
-240	Calibration locked
-241	Wrong calibration password

Errors are retrieved with the :SYSTem:ERRor? command, which returns the oldest error and removes it from the queue.
5. Special Features and Applications
5.1 IF Offset Operation

The Intermediate Frequency offset feature is specifically designed for receiver applications:

Principle:

    When active, output frequency = displayed frequency - IF offset

    Display shows the intended receive frequency

    Output provides the local oscillator signal

Applications:

    Superheterodyne receiver alignment

    Tracking generator for filter testing

    VFO for amateur radio transceivers

Visual Feedback:

    Dot in LCD bottom-right corner indicates offset active

    Both momentary (button) and toggle (encoder) control modes

5.2 Comprehensive Tuning Steps

Ten tuning steps accommodate diverse applications:
Step	Primary Application
10 Hz	CW/SSB filter alignment, crystal testing
50 Hz	General fine adjustment
100 Hz	Medium-resolution tuning
500 Hz	Quick band scanning
1 kHz	Standard step for most applications
2.5 kHz	AM broadcast channel spacing
5 kHz	Rapid frequency changes
10 kHz	Coarse band coverage
100 kHz	Band segment selection
1 MHz	Wide frequency jumps
5.3 Memory and Persistence

The EEPROM management system provides:

    Automatic storage of last used frequency

    Power-off retention for weeks or years

    Validation headers preventing garbage data reads

    Wear leveling through 2-second delay before writing

5.4 Professional Integration Capabilities

The SCPI interface enables integration into automated test systems:

Remote Programming Examples:

Python control script:
python

import serial
import time

# Connect to generator
gen = serial.Serial('COM3', 115200, timeout=1)

# Set frequency to 14.150 MHz
gen.write(b'FREQ 14.150 MHz\n')
time.sleep(0.1)

# Query current frequency
gen.write(b'FREQ?\n')
freq = gen.readline()
print(f'Current frequency: {freq}')

# Sweep from 1 MHz to 30 MHz in 100 kHz steps
for f in range(1, 30, 0.1):
    gen.write(f'FREQ {f} MHz\n'.encode())
    time.sleep(0.05)  # Allow settling
    # Measure with external equipment...

LabVIEW Integration:

    Standard SCPI commands recognized by LabVIEW instrument drivers

    Can be controlled via VISA interface

    Compatible with NI-MAX instrument configuration

MATLAB Control:
matlab

% Create serial object
s = serial('COM3', 'BaudRate', 115200);
fopen(s);

% Set frequency and read back
fprintf(s, 'FREQ 7.050 MHz');
fprintf(s, 'FREQ?');
freq = fscanf(s, '%s');
disp(['Frequency: ' freq]);

% Clean up
fclose(s);
delete(s);

6. Performance Characteristics
6.1 Frequency Performance
Parameter			Value					Notes
Frequency range			0 - 30 MHz				Limited by AD9850 specification
Resolution			0.0291 Hz				At 125 MHz reference clock
Accuracy			± reference clock tolerance		Depends on crystal/oscillator
Switching speed			< 1 ms					Time to load new frequency word
Phase noise			-90 dBc/Hz typical @ 10 kHz offset	Per AD9850 datasheet
Spurious-free dynamic range	> 50 dB	Typical, varies with frequency

6.2 Output Characteristics
Parameter		Value		Notes
Output amplitude	0 - ~1 Vpp	Adjustable via attenuator
Output impedance	~50 ohms	After amplifier stage
Output connector	BNC female	Standard test equipment interface
DC offset		0 V		AC-coupled output
Harmonic distortion	< -40 dBc	Typical at mid-band

6.3 User Interface Performance
Parameter		Value			Notes
Tuning response		< 10 ms			From encoder turn to frequency change
Display update		20 ms			LCD update time
EEPROM write delay	2 seconds		After last frequency change
SCPI command rate	> 100 commands/second	At 115200 baud

7. Applications and Use Cases
7.1 Laboratory Applications

General Purpose Signal Source:

    Audio and low-frequency circuit testing

    Amplifier frequency response measurement

    Filter characterization

    Mixer and modulator testing

Receiver Development:

    Local oscillator for superheterodyne receivers

    IF alignment and testing

    Image rejection measurement

    Sensitivity testing with calibrated attenuator

7.2 Educational Applications

Teaching DDS Principles:

    Demonstrate digital frequency synthesis

    Show relationship between tuning word and output frequency

    Illustrate aliasing and Nyquist criteria

    Experiment with phase accumulation concepts

Electronics Laboratory:

    Affordable signal source for student labs

    Programmable via SCPI for automated experiments

    Open-source for modification and learning

7.3 Amateur Radio Applications

Transceiver VFO:

    Replace crystal oscillators with variable tuning

    Cover multiple bands with single oscillator

    Add RIT (Receiver Incremental Tuning) via IF offset

Antenna Analyzer Front-End:

    Sweep generator for antenna measurements

    Track generator for spectrum analyzer

    Marker generator for filter alignment

Test Equipment Integration:

    Signal injector for troubleshooting

    Alignment source for filters and discriminators

    BFO for SSB/CW reception

7.4 Industrial Applications

Automated Test Systems:

    Production testing with SCPI control

    Burn-in and life testing with programmed sequences

    Environmental chamber testing with remote operation

Embedded Systems Development:

    Clock source for digital circuits

    Stimulus for ADC testing

    Reference for PLL characterization

8. Conclusion

The AD9850 Professional DDS Signal Generator represents a significant achievement in open-source test equipment design. 
By combining accessible hardware (Arduino UNO, AD9850 module) with sophisticated software (SCPI implementation, interrupt-driven control), 
it delivers performance and features typically found only in commercial instruments costing significantly more.

The system's dual personality—intuitive local control through rotary encoder and LCD, plus professional remote control through SCPI commands—makes 
it equally suitable for the hobbyist's workbench and the automated test laboratory. Comprehensive features like IF offset, multiple tuning steps, 
and non-volatile memory address real-world needs in receiver development and general-purpose signal generation.

The hardware design, with its carefully considered pin allocation and RF output stage, provides a solid foundation for reliable operation up to 30 MHz. 
The software architecture, with its modular organization and robust error handling, ensures maintainability and extensibility for future enhancements.

Perhaps most importantly, this project demonstrates that sophisticated test equipment need not be a closed, proprietary system. 
By embracing open standards like SCPI and open-source hardware platforms, it invites users to understand, modify, and improve the instrument to suit 
their specific needs—a philosophy that advances both education and innovation in electronic design.
Appendix: Quick Reference
Local Controls Summary
Control	Function
Rotary encoder rotation	Tune frequency by current step
Rotary encoder press	Toggle IF offset on/off
Increment button (A0)	Cycle through tuning steps
IF button (A5)	Momentary IF offset enable
Tuning Step Sequence

10 Hz → 50 Hz → 100 Hz → 500 Hz → 1 kHz → 2.5 kHz → 5 kHz → 10 kHz → 100 kHz → 1 MHz → (repeat)

SCPI Quick Reference
Command		Description
*IDN?		Identify instrument
FREQ 10 MHz	Set frequency
FREQ?		Query frequency
FREQ:STEP 1 kHz	Set tuning step
OUTP ON		Enable output
SYST:ERR?	Check for errors

Pin Connections Summary
Component		Arduino Pin
AD9850 W_CLK		D8
AD9850 FQ_UD		D9
AD9850 DATA		D11
AD9850 RESET		D10
LCD RS			D12
LCD Enable		D13
LCD D4			D7
LCD D5			D6
LCD D6			D5
LCD D7			D4
Encoder A		D2
Encoder B		D3
Encoder Button		D4 (shared)
Increment Button	A0
IF Button		A5


