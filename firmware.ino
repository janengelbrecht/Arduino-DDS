/*
 * PROFESSIONAL AD9850 DDS SIGNAL GENERATOR FIRMWARE
 * WITH SCPI COMPLIANT REMOTE CONTROL
 * =======================================================================================================================================
 * Project: 30MHz DDS Signal Generator with LCD, Rotary Encoder and SCPI
 * Version: 4.1 - Professional Edition with SCPI Remote Control
 * Date: 2024
 * Author: Jan Engelbrecht Pedersen
 * 
 * COMPLETE HARDWARE ARCHITECTURE EXPLANATION
 * =======================================================================================================================================
 * 
 * SYSTEM OVERVIEW:
 * This firmware controls an AD9850 DDS (Direct Digital Synthesis) module to
 * generate precise sine wave signals from 0-30 MHz. The system includes:
 * - AD9850 DDS Module: Generates the actual frequency output
 * - 16x2 LCD Display: Shows current frequency and settings
 * - Rotary Encoder: Provides tuning control with push button
 * - Control Buttons: For IF offset and increment selection
 * - EEPROM: Stores last used frequency between power cycles
 * - Serial/USB: SCPI compliant remote control interface
 * 
 * 
 * HARDWARE CONNECTION DETAILS:
 * =======================================================================================================================================
 * 
 * ARDUINO PIN ASSIGNMENTS:
 * 
 * PIN 2 (D2)  - Rotary Encoder Pin A (with interrupt)
 * PIN 3 (D3)  - Rotary Encoder Pin B (with interrupt)
 *     These pins use Pin Change Interrupts for instant response to rotation.
 *     Internal pull-up resistors are enabled.
 * 
 * PIN 4 (D4)  - LCD Data Bit 7 (DB7) - MSB of data nibble
 * PIN 5 (D5)  - LCD Data Bit 6 (DB6)
 * PIN 6 (D6)  - LCD Data Bit 5 (DB5)
 * PIN 7 (D7)  - LCD Data Bit 4 (DB4) - LSB of data nibble
 *     These 4 pins form the 4-bit data bus to the LCD. Using 4-bit mode
 *     saves Arduino pins while maintaining full functionality.
 * 
 * PIN 8 (D8)  - AD9850 Word Load Clock (W_CLK)
 *     Each rising edge clocks one data bit into the AD9850's internal shift
 *     register. Must be pulsed after setting each data bit.
 * 
 * PIN 9 (D9)  - AD9850 Frequency Update (FQ_UD)
 *     Rising edge updates the output frequency with newly loaded data.
 *     Must be pulsed after all 40 bits (32 freq + 8 control) are loaded.
 * 
 * PIN 10 (D10) - AD9850 Reset
 *     Resets the AD9850's internal state. Must be pulsed during initialization.
 * 
 * PIN 11 (D11) - AD9850 Serial Data (DATA)
 *     Serial data input. Bits are sent LSB first, clocked on W_CLK.
 * 
 * PIN 12 (D12) - LCD Register Select (RS)
 *     Selects command (0) or data (1) register in LCD.
 * 
 * PIN 13 (D13) - LCD Enable (E)
 *     Strobe pin - data is latched on falling edge.
 * 
 * PIN A0      - Increment Selection Button
 *     Active LOW with internal pull-up. Cycles through tuning steps:
 *     10Hz, 50Hz, 100Hz, 500Hz, 1kHz, 2.5kHz, 5kHz, 10kHz, 100kHz, 1MHz
 * 
 * PIN A5      - IF Offset Toggle Button
 *     Active LOW with internal pull-up. Enables/disables IF offset
 *     (subtracts intermediate frequency from output).
 * 
 * 
 * AD9850 DDS MODULE - DETAILED OPERATION
 * =======================================================================================================================================
 * 
 * DDS THEORY:
 * The AD9850 uses Direct Digital Synthesis to generate frequencies. It contains:
 * - A 32-bit phase accumulator that increments at the reference clock rate
 * - A sine lookup table that converts phase to amplitude
 * - A 10-bit DAC that outputs the analog sine wave
 * 
 * FREQUENCY CALCULATION:
 * The output frequency is determined by the formula:
 *     f_out = (ΔPhase × f_clock) / 2^32
 * 
 * Where:
 * - ΔPhase is the 32-bit frequency tuning word (what we send)
 * - f_clock is the reference clock (125 MHz for AD9850)
 * - 2^32 is the phase accumulator resolution
 * 
 * Solving for the tuning word:
 *     ΔPhase = (f_out × 2^32) / f_clock
 * 
 * Example for 10 MHz output:
 *     ΔPhase = (10,000,000 × 4,294,967,296) / 125,000,000
 *            = 343,597,383,680 / 125,000,000
 *            = 2,748,779 (approximately)
 * 
 * SERIAL PROGRAMMING SEQUENCE:
 * 1. Assert RESET pulse to initialize module
 * 2. Assert W_CLK pulse to prepare for serial mode
 * 3. Assert FQ_UD pulse to enable serial loading
 * 4. Send 40 bits (32 frequency + 8 control) LSB first:
 *    - Bit 0-31: Frequency tuning word (LSB first)
 *    - Bit 32-39: Control word (phase, power-down)
 * 5. Pulse FQ_UD to update output frequency
 * 
 * CONTROL BYTE FORMAT:
 * Bit 0-4: Phase modulation (bits 0-4)
 * Bit 5:   Reserved (must be 0)
 * Bit 6:   Power-down control (0=normal, 1=power-down)
 * Bit 7:   Reserved (must be 0)
 * 
 * 
 * LCD DISPLAY OPERATION (HD44780)
 * =======================================================================================================================================
 * 
 * 4-BIT INTERFACE MODE:
 * The LCD operates in 4-bit mode to save pins. Data is transferred as two
 * 4-bit nibbles (high nibble first). The interface works as follows:
 * 
 * 1. Set RS high (data) or low (command)
 * 2. Place high nibble on D4-D7
 * 3. Pulse Enable (high then low) - data is latched on falling edge
 * 4. Place low nibble on D4-D7
 * 5. Pulse Enable again
 * 
 * DISPLAY FORMAT:
 * Line 0: "XX.XXX.XXX MHz" (frequency with decimal separators)
 * Line 1: "   XXX Hz/KHz"   (tuning increment) with optional IF indicator
 * 
 * 
 * ROTARY ENCODER OPERATION
 * =======================================================================================================================================
 * 
 * QUADRATURE ENCODING:
 * The rotary encoder generates two square waves (A and B) that are 90° out
 * of phase. Direction is determined by which channel changes first:
 * 
 * Clockwise:     A leads B (A changes before B)
 * Counter-clockwise: B leads A (B changes before A)
 * 
 * The rotary library handles this decoding automatically using interrupts
 * for instant response without processor polling.
 * 
 * 
 * MEMORY MANAGEMENT (EEPROM)
 * =======================================================================================================================================
 * 
 * DATA STORAGE FORMAT:
 * Address 0:  Magic byte (0xAA) - validates that EEPROM contains our data
 * Address 1:  Version byte (0x01) - allows future format changes
 * Address 2-5: 32-bit frequency value (MSB first)
 * 
 * This robust format ensures we don't accidentally read garbage data
 * from uninitialized EEPROM.
 * 
 * 
 * INTERRUPT SYSTEM
 * =======================================================================================================================================
 * 
 * Pin Change Interrupts are used for the rotary encoder because:
 * 1. They provide instant response to rotation
 * 2. They work with any pin (unlike external interrupts on pins 2&3 only)
 * 3. They don't miss encoder steps during other operations
 * 
 * The PCINT2_vect ISR handles pins D2 and D3 (PCINT18 and PCINT19).
 * 
 * 
 * FREQUENCY LIMITS AND SAFETY
 * =======================================================================================================================================
 * 
 * Upper Limit: 30,000,000 Hz (30 MHz) - AD9850 maximum specified output
 * Lower Limit: 0 Hz - DC output (though practical minimum is ~1 Hz)
 * 
 * The firmware enforces these limits to prevent:
 * - Outputting invalid frequencies that could damage the DDS
 * - Aliasing effects from exceeding Nyquist limit
 * 
 * 
 * IF OFFSET OPERATION
 * =======================================================================================================================================
 * 
 * The IF offset feature subtracts a fixed frequency (g_intermediateFreq)
 * from the displayed frequency. This is useful in superheterodyne receivers
 * where the VFO needs to track the IF frequency.
 * 
 * When enabled (button pressed), the output frequency is:
 *     f_output = f_displayed - f_if_offset
 * 
 * A dot appears in the bottom-right corner when IF offset is active.
 * 
 * 
 * TUNING INCREMENT STEPS
 * =======================================================================================================================================
 * 
 * The firmware provides 10 tuning steps to cover different applications:
 * 10 Hz   - Fine tuning for CW/SSB
 * 50 Hz   - General purpose fine tuning
 * 100 Hz  - Medium resolution
 * 500 Hz  - Quick tuning
 * 1 kHz   - Standard step
 * 2.5 kHz - AM broadcast band step
 * 5 kHz   - Quick frequency changes
 * 10 kHz  - Coarse tuning
 * 100 kHz - Fast band scanning
 * 1 MHz   - Very coarse band changes
 * 
 * 
 * POWER-UP BEHAVIOR
 * =======================================================================================================================================
 * 
 * On startup, the system:
 * 1. Initializes all pins to known states
 * 2. Configures interrupts for encoder
 * 3. Initializes LCD display
 * 4. Resets AD9850 and prepares for serial mode
 * 5. Reads last frequency from EEPROM (or uses default)
 * 6. Sets initial tuning increment to 10 Hz
 * 7. Outputs the stored frequency
 * 
 * If EEPROM contains invalid data (first run or corruption),
 * it initializes to 7.050 MHz (a common amateur radio frequency).
 * 
 * 
 * SCPI (STANDARD COMMANDS FOR PROGRAMMABLE INSTRUMENTS) IMPLEMENTATION
 * =======================================================================================================================================
 * 
 * What is SCPI?
 * SCPI (pronounced "skippy") is an IEEE standard for instrument control
 * that defines a common command set and syntax for test and measurement
 * equipment. It provides:
 * - Standardized command structure
 * - Hierarchical command trees
 * - Consistent parameter formats
 * - Common queries for identification and status
 * 
 * IMPLEMENTED SCPI SUBSYSTEMS:
 * 
 * 1. IEEE 488.2 Common Commands:
 *    *IDN?        - Identification query
 *    *RST         - Reset to default state
 *    *OPC         - Operation complete
 *    *OPC?        - Operation complete query
 *    *WAI         - Wait to continue
 *    *TST?        - Self-test query
 *    *CLS         - Clear status
 *    *ESE, *ESE?  - Event status enable
 *    *ESR?        - Event status register
 *    *SRE, *SRE?  - Service request enable
 *    *STB?        - Status byte query
 * 
 * 2. Frequency Subsystem (FREQuency):
 *    :FREQuency <value> [UNIT]  - Set output frequency
 *    :FREQuency?                 - Query current frequency
 *    :FREQuency:STEP <value> [UNIT] - Set tuning step
 *    :FREQuency:STEP?             - Query tuning step
 *    :FREQuency:LIMIT? <MIN|MAX>  - Query frequency limits
 *    :FREQuency:OFFSet <value>    - Set IF offset
 *    :FREQuency:OFFSet?           - Query IF offset
 *    :FREQuency:OFFSet:STATe <0|1> - Enable/disable IF offset
 *    :FREQuency:OFFSet:STATe?      - Query IF offset state
 * 
 * 3. Output Subsystem (OUTPut):
 *    :OUTPut <0|1|OFF|ON>       - Enable/disable output
 *    :OUTPut?                    - Query output state
 *    :OUTPut:PROTection?         - Query protection status
 * 
 * 4. Display Subsystem (DISPlay):
 *    :DISPlay:TEXT <string>      - Display custom text
 *    :DISPlay:TEXT:CLEar         - Clear custom text
 *    :DISPlay:CONTrast <value>   - Set LCD contrast
 *    :DISPlay:CONTrast?          - Query LCD contrast
 * 
 * 5. System Subsystem (SYSTem):
 *    :SYSTem:ERRor?              - Query next error
 *    :SYSTem:VERSion?            - Query SCPI version
 *    :SYSTem:PRESet              - Preset to defaults
 *    :SYSTem:BEEPer <0|1>        - Enable/disable beeper
 *    :SYSTem:BEEPer?             - Query beeper state
 * 
 * 6. Memory Subsystem (MEMory):
 *    :MEMory:STORe <location>    - Store current settings
 *    :MEMory:RECall <location>   - Recall stored settings
 *    :MEMory:CLEar <location>    - Clear memory location
 * 
 * 7. Calibration Subsystem (CALibration):
 *    :CALibration:REFerence <value> - Set reference clock
 *    :CALibration:REFerence?        - Query reference clock
 *    :CALibration:STORe              - Store calibration
 *    :CALibration:SECure:CODE <code> - Set cal password
 * 
 * SCPI COMMAND SYNTAX:
 * - Commands are case-insensitive (FREQ, Freq, freq all accepted)
 * - Short form is uppercase (FREQuency, FREQ)
 * - Parameters can be numeric or string
 * - Units are supported (Hz, kHz, MHz, GHz)
 * - Multiple commands can be chained with semicolons
 * - Queries end with '?' and return values
 * 
 * EXAMPLES:
 *   FREQ 10 MHz              - Set frequency to 10 MHz
 *   FREQ?                    - Returns "10000000"
 *   FREQ:STEP 1 kHz          - Set step to 1 kHz
 *   OUTP ON                  - Enable output
 *   *IDN?                    - Returns "AD7C,AD9850 Generator,1.0,SCPI-1.0"
 * 
 * 
 * SERIAL COMMUNICATION SETTINGS:
 * =======================================================================================================================================
 * Baud Rate: 115200
 * Data Bits: 8
 * Parity: None
 * Stop Bits: 1
 * Flow Control: None
 * Line Ending: LF (ASCII 10) or CR+LF
 * 
 * The serial port uses the Arduino's hardware UART (pins 0 and 1) for
 * communication with a PC or controller via USB-to-Serial adapter.
 */

// ======================================================================================================================================
// LIBRARY INCLUDES
// ======================================================================================================================================
#include "Arduino.h"
#include <LiquidCrystal.h>  // Arduino LCD library for HD44780-compatible displays
#include <EEPROM.h>         // Built-in EEPROM library for non-volatile storage
#include "BasicEncoder.h"    // Rotary encoder library for quadrature decoding (Replaced rotary.h with BasicEncoder.h)

// ======================================================================================================================================
// HARDWARE ABSTRACTION LAYER (HAL) - PIN DEFINITIONS
// ======================================================================================================================================
// AD9850 DDS Module Interface Pins
// These pins control the AD9850 DDS module. They must be connected as shown.
#define PIN_AD9850_W_CLK   8   // Word Load Clock - clocks data into shift register
#define PIN_AD9850_FQ_UD   9   // Frequency Update - updates output with new frequency
#define PIN_AD9850_DATA    11  // Serial Data - data bits sent LSB first
#define PIN_AD9850_RESET   10  // Reset - resets the AD9850 internal state

// LCD Display Interface Pins (4-bit mode)
// These pins control the 16x2 LCD display in 4-bit mode to save Arduino pins.
#define PIN_LCD_RS         12  // Register Select - 0=command, 1=data
#define PIN_LCD_ENABLE     13  // Enable - strobes data into LCD (active high)
#define PIN_LCD_D4         7   // Data bit 4 (DB4) - LSB of 4-bit data
#define PIN_LCD_D5         6   // Data bit 5 (DB5)
#define PIN_LCD_D6         5   // Data bit 6 (DB6)
#define PIN_LCD_D7         4   // Data bit 7 (DB7) - MSB of 4-bit data

// Control Input Pins
// User interface buttons with internal pull-ups (active LOW)
#define PIN_BUTTON_INCREMENT A0    // Increment cycle button - changes tuning step
#define PIN_BUTTON_IF_OFFSET A5    // IF offset toggle button - enables/disables IF shift
#define PIN_ENCODER_BUTTON   4     // Rotary encoder push button - added as separate input (using pin 4, was unused)

// ======================================================================================================================================
// SYSTEM CONSTANTS AND CONFIGURATION
// ======================================================================================================================================
#define AD9850_CLOCK_FREQ   125000000UL  // AD9850 reference clock frequency in Hz (125 MHz typical)
                                         // Can be adjusted slightly for calibration

#define AD9850_PHASE_WORD   0x000        // Phase control byte (all zeros)
                                         // Bits: [4:0]=phase, bit5=reserved(0), 
                                         // bit6=power down(0=normal), bit7=reserved(0)

#define FREQ_UPPER_LIMIT    30000000     // Maximum frequency: 30 MHz (AD9850 hardware limit)
#define FREQ_LOWER_LIMIT    0            // Minimum frequency: 0 Hz (DC output)

// EEPROM Data Validation
// These ensure we only read valid data from EEPROM, not garbage from uninitialized memory
#define EEPROM_MAGIC_BYTE   0xAA         // Signature byte to validate EEPROM data is ours
#define EEPROM_VERSION      0x01         // Data structure version for future upgrades

// EEPROM Memory Map Addresses
// Organized for easy expansion and validation
#define EEPROM_ADDR_MAGIC    0   // 1 byte: Magic number for data validation
#define EEPROM_ADDR_VERSION  1   // 1 byte: Data structure version
#define EEPROM_ADDR_FREQ_MSB 2   // 4 bytes: Stored frequency (32-bit value, MSB at 2, LSB at 5)

// ======================================================================================================================================
// SCPI SPECIFIC CONSTANTS AND CONFIGURATION
// ======================================================================================================================================
#define SCPI_INPUT_BUFFER_SIZE 128  // Maximum size of SCPI command input buffer
#define SCPI_ERROR_QUEUE_SIZE   10  // Number of errors that can be queued
#define SCPI_MAX_MEMORY_LOCATIONS 10  // Number of memory locations for store/recall

// SCPI Standard Error Codes
#define SCPI_ERR_NO_ERROR                0  // No error (success)
#define SCPI_ERR_INVALID_COMMAND         -100 // Command error - unrecognized command
#define SCPI_ERR_INVALID_PARAMETER       -102 // Syntax error - invalid parameter format
#define SCPI_ERR_INVALID_SUFFIX          -131 // Invalid suffix - unknown unit
#define SCPI_ERR_SUFFIX_NOT_ALLOWED      -138 // Suffix not allowed - unit where not expected
#define SCPI_ERR_PARAMETER_NOT_ALLOWED   -108 // Parameter not allowed - extra parameter
#define SCPI_ERR_MISSING_PARAMETER       -109 // Missing parameter - needed but not provided
#define SCPI_ERR_OUT_OF_MEMORY           -225 // Out of memory - buffer overflow
#define SCPI_ERR_OUTPUT_PROTECTION        -230 // Output protection - output tripped
#define SCPI_ERR_CALIBRATION_LOCKED       -240 // Calibration locked - password required
#define SCPI_ERR_CALIBRATION_PASSWORD     -241 // Wrong calibration password
#define SCPI_ERR_HARDWARE_MISSING         -241 // Hardware missing - component not detected
#define SCPI_ERR_HARDWARE_ERROR           -240 // Hardware error - component failure

// SCPI Status Register Bits
#define SCPI_STB_MAV   (1 << 4)  // Message Available Bit - output available
#define SCPI_STB_ESB   (1 << 5)  // Event Status Bit - ESR has enabled bits set
#define SCPI_STB_SRQ   (1 << 6)  // Service Request Bit - request service from controller

#define SCPI_ESR_OPC   (1 << 0)  // Operation Complete - all pending operations done
#define SCPI_ESR_EXE   (1 << 4)  // Execution Error - error during execution
#define SCPI_ESR_CME   (1 << 5)  // Command Error - syntax error in command

// ======================================================================================================================================
// GLOBAL VARIABLES
// ======================================================================================================================================
// Rotary Encoder Object
// Uses pins 2 and 3 with internal pull-ups for quadrature decoding
BasicEncoder g_rotaryEncoder(2, 3);  // Create BasicEncoder instance on pins 2 and 3 (Replaced Rotary with BasicEncoder)

// LCD Display Object
// Initialized with the 6 control pins in 4-bit mode
LiquidCrystal g_lcd(PIN_LCD_RS, PIN_LCD_ENABLE,  // Create LCD object with RS and Enable pins
                    PIN_LCD_D4, PIN_LCD_D5,      // Data pins D4, D5
                    PIN_LCD_D6, PIN_LCD_D7);     // Data pins D6, D7

// Frequency Variables
int_fast32_t g_currentFreq = 0;           // Current operational frequency in Hz
                                         // Updated by encoder interrupts or SCPI commands
int_fast32_t g_lastDisplayedFreq = -1;    // Last frequency displayed on LCD
                                         // Used to detect changes and avoid redundant updates
int_fast32_t g_intermediateFreq = 0;      // IF offset frequency in Hz (default 0)
                                         // Subtracted from displayed when IF offset active

// Tuning Increment Configuration
int_fast32_t g_tuningIncrement = 10;       // Current tuning step size in Hz
                                         // Changes with increment button or SCPI
char g_incrementDisplay[12] = "10 Hz";    // Display string for current increment (char array for sprintf)
                                         // Fixed size 12 chars: enough for "1000.0 kHz" plus null
int g_incrementCursorPos = 5;               // LCD cursor position for increment display
                                         // Varies with text length for centering (5 for "10 Hz")

// Button State Variables
int g_incrementButtonState = HIGH;          // Last known state of increment button (HIGH = not pressed)
int g_ifButtonState = HIGH;                  // Current state of IF button (HIGH = not pressed)
int g_ifButtonPrevState = HIGH;              // Previous state for edge detection (to detect press/release)
int g_encoderButtonState = HIGH;             // Current state of encoder button (HIGH = not pressed)
int g_encoderButtonPrevState = HIGH;         // Previous state for edge detection
unsigned long g_lastEncoderButtonDebounceTime = 0; // For debouncing encoder button

// IF Offset Control
int g_ifOffsetActive = 1;                    // 1=disabled, 0=enabled
                                         // When enabled (0): output = displayed - IF offset

// Timing and Memory Management
uint32_t g_lastFreqChangeTime = 0;           // millis() value of last frequency change
                                         // Used for delayed EEPROM writes (2 second delay)
uint8_t g_memoryStatus = 1;                   // 0=needs EEPROM update, 1=up to date
                                         // Tracks if frequency needs saving to EEPROM

// Frequency Digit Placeholders
// These store individual digits for display formatting (for LCD line 0)
byte g_digit_millions;           // Millions place (MHz digit)
byte g_digit_hundredthousands;   // Hundred-thousands (100kHz digit)
byte g_digit_tenthousands;       // Ten-thousands (10kHz digit)
byte g_digit_thousands;          // Thousands (1kHz digit)
byte g_digit_hundreds;           // Hundreds (100Hz digit)
byte g_digit_tens;               // Tens (10Hz digit)
byte g_digit_ones;                // Ones (1Hz digit)

// ======================================================================================================================================
// SCPI GLOBAL VARIABLES
// ======================================================================================================================================
char g_scpiInputBuffer[SCPI_INPUT_BUFFER_SIZE];  // Buffer for incoming SCPI commands (128 bytes)
uint8_t g_scpiInputIndex = 0;                      // Current position in input buffer (next write location)

// SCPI Error Queue
int16_t g_scpiErrorQueue[SCPI_ERROR_QUEUE_SIZE];  // Circular buffer for errors (stores error codes)
uint8_t g_scpiErrorHead = 0;                       // Head index for error queue (where next error is written)
uint8_t g_scpiErrorTail = 0;                       // Tail index for error queue (where next error is read)

// SCPI Status Registers
uint8_t g_scpiStatusByte = 0;                      // Status Byte Register (STB) - summary status
uint8_t g_scpiServiceRequestEnable = 0;            // Service Request Enable (SRE) - enables SRQ generation
uint8_t g_scpiEventStatusRegister = 0;              // Event Status Register (ESR) - standard event status
uint8_t g_scpiEventStatusEnable = 0;                // Event Status Enable (ESE) - enables ESR bits

// SCPI Output State
bool g_scpiOutputEnabled = true;                    // Output enabled state (true = ON, false = OFF)
bool g_scpiOutputProtection = false;                 // Output protection tripped (true = protection active)

// SCPI System Settings
bool g_scpiBeeperEnabled = true;                    // Beeper enabled state (true = beep on, false = off)
uint8_t g_scpiMemoryLocations[SCPI_MAX_MEMORY_LOCATIONS][4];  // Memory for store/recall (10 locations, 4 bytes each)

// SCPI Calibration Settings
uint32_t g_scpiReferenceClock = AD9850_CLOCK_FREQ;  // Current reference clock value (may be calibrated)
bool g_scpiCalibrationLocked = true;                 // Calibration locked by default (true = locked)
uint16_t g_scpiCalibrationPassword = 1234;            // Password for calibration access (default 1234)
uint32_t g_scpiCalibrationStoredRef = AD9850_CLOCK_FREQ;  // Stored calibration value (saved in EEPROM)

// Custom display text
char g_scpiCustomText[17] = "";                      // Custom text for display (max 16 chars + null terminator)

// ======================================================================================================================================
// FORCED EEPROM INITIALIZATION CONTROL
// ======================================================================================================================================
// Set to 1 for first run to force initial frequency storage
// This writes a default frequency to EEPROM on first boot
// MUST BE SET TO 0 AFTER INITIAL PROGRAMMING FOR NORMAL OPERATION!
#define FORCE_EEPROM_INIT 0  // 0 = normal operation, 1 = force initialization on next boot

// ======================================================================================================================================
// FORWARD DECLARATIONS (Function Prototypes)
// ======================================================================================================================================
// Hardware Abstraction Layer (HAL) Functions
void HAL_AD9850_Init(void);                    // Initialize AD9850 DDS module
void HAL_AD9850_SendFrequency(int32_t frequency);  // Send frequency to AD9850
void HAL_AD9850_TransferByte(uint8_t data);    // Transfer one byte to AD9850 serially
void HAL_AD9850_PulsePin(uint8_t pin);          // Generate a pulse on specified pin

// User Interface (UI) Functions
void UI_LCD_Init(void);                         // Initialize LCD display
void UI_LCD_UpdateFrequency(void);               // Update frequency display on LCD
void UI_LCD_UpdateIncrement(void);               // Update tuning increment display
void UI_LCD_ShowIFOffsetIndicator(void);         // Show/hide IF offset indicator dot
void UI_LCD_ClearLine(uint8_t line);             // Clear a specific line on LCD
void UI_LCD_ShowCustomText(void);                 // Show custom text on LCD (from SCPI)
void UI_LCD_RestoreNormalDisplay(void);           // Restore normal display (clear custom text)

// Input Processing Functions
void Input_ProcessIncrementButton(void);         // Process increment button press
void Input_ProcessIFButton(void);                 // Process IF offset button press
void Input_ProcessEncoderButton(void);            // Process encoder push button press (added for separate button handling)
void Input_UpdateIncrement(void);                 // Update tuning increment value

// Memory Management Functions
void Memory_StoreFrequency(void);                 // Store frequency to EEPROM
void Memory_LoadFrequency(void);                  // Load frequency from EEPROM
bool Memory_IsValid(void);                        // Check if EEPROM contains valid data
void Memory_InitDefault(void);                     // Initialize EEPROM with default values

// ======================================================================================================================================
// SCPI FUNCTION PROTOTYPES
// ======================================================================================================================================
void SCPI_Init(void);                             // Initialize SCPI subsystem
void SCPI_Process(void);                           // Process incoming SCPI commands
void SCPI_ParseCommand(char* cmd);                 // Parse a SCPI command
void SCPI_ExecuteCommand(char* cmd, char* params); // Execute a parsed SCPI command
void SCPI_HandleIEEECommon(char* cmd, char* params); // Handle IEEE 488.2 common commands
void SCPI_HandleFrequency(char* cmd, char* params); // Handle FREQuency subsystem commands
void SCPI_HandleOutput(char* cmd, char* params);   // Handle OUTPut subsystem commands
void SCPI_HandleDisplay(char* cmd, char* params);  // Handle DISPlay subsystem commands
void SCPI_HandleSystem(char* cmd, char* params);   // Handle SYSTem subsystem commands
void SCPI_HandleMemory(char* cmd, char* params);   // Handle MEMory subsystem commands
void SCPI_HandleCalibration(char* cmd, char* params); // Handle CALibration subsystem commands

// SCPI Utility Functions
uint32_t SCPI_ParseNumber(char* str, bool* hasUnit, char* unit, uint32_t defaultUnit); // Parse number with unit
void SCPI_QueueError(int16_t error);               // Queue an error in error queue
int16_t SCPI_GetNextError(void);                   // Get next error from queue
void SCPI_PrintResponse(const char* response);     // Print response string
void SCPI_PrintNumber(uint32_t value);             // Print number as response
void SCPI_SetStatusBit(uint8_t bit, bool value);   // Set/clear a bit in status byte
void SCPI_UpdateESR(uint8_t bit);                   // Update Event Status Register

// ======================================================================================================================================
// HARDWARE ABSTRACTION LAYER (HAL) IMPLEMENTATION
// ======================================================================================================================================

/**
 * Initialize AD9850 DDS Module
 * 
 * This function performs the complete initialization sequence for the AD9850
 * as specified in the datasheet. The sequence is critical for proper operation:
 * 
 * 1. Configure all control pins as outputs
 * 2. Pulse RESET to ensure clean startup state
 * 3. Pulse W_CLK to clear any residual data
 * 4. Pulse FQ_UD to enable serial programming mode
 * 
 * After this sequence, the AD9850 is ready to accept frequency data.
 */
void HAL_AD9850_Init(void) {
    // Configure AD9850 control pins as outputs
    pinMode(PIN_AD9850_FQ_UD, OUTPUT);  // Set FQ_UD pin as output
    pinMode(PIN_AD9850_W_CLK, OUTPUT);  // Set W_CLK pin as output
    pinMode(PIN_AD9850_DATA, OUTPUT);   // Set DATA pin as output
    pinMode(PIN_AD9850_RESET, OUTPUT);  // Set RESET pin as output
    
    // Reset sequence (per AD9850 datasheet page 12)
    HAL_AD9850_PulsePin(PIN_AD9850_RESET);  // Pulse RESET - clears internal state
    HAL_AD9850_PulsePin(PIN_AD9850_W_CLK);  // Pulse W_CLK - clears shift register
    HAL_AD9850_PulsePin(PIN_AD9850_FQ_UD);  // Pulse FQ_UD - enables serial programming mode
}

/**
 * Generate a high pulse on specified pin
 * 
 * This utility function creates a clean high pulse on any output pin.
 * Pulse width is determined by digitalWrite timing (~microseconds).
 * Used extensively for AD9850 control signals.
 * 
 * @param pin: Arduino pin number to pulse
 */
void HAL_AD9850_PulsePin(uint8_t pin) {
    digitalWrite(pin, HIGH);  // Set pin high (pulse starts)
    digitalWrite(pin, LOW);   // Set pin low (pulse ends - completes the pulse)
}

/**
 * Transfer a single byte to AD9850 via serial interface
 * 
 * This function implements the serial data transfer protocol for the AD9850:
 * 
 * PROTOCOL:
 * 1. Data is sent LSB (Least Significant Bit) first
 * 2. Each bit is presented on DATA pin
 * 3. W_CLK is pulsed to clock the bit into the shift register
 * 4. Process repeats for all 8 bits
 * 
 * Timing requirements (from datasheet):
 * - Data setup time before W_CLK: min 3 ns
 * - W_CLK high pulse width: min 3.5 ns
 * - These are easily met by Arduino digitalWrite timing
 * 
 * @param data: Byte to transmit (8 bits)
 */
void HAL_AD9850_TransferByte(uint8_t data) {
    for (int i = 0; i < 8; i++) {           // Send all 8 bits, one at a time
        // Output current LSB (data & 0x01 extracts the least significant bit)
        digitalWrite(PIN_AD9850_DATA, data & 0x01);  // Set DATA pin to current LSB
        
        // Clock the bit into AD9850's shift register
        HAL_AD9850_PulsePin(PIN_AD9850_W_CLK);  // Pulse W_CLK to clock in the bit
        
        // Shift to next bit (move next bit into LSB position)
        data >>= 1;  // Right shift: next bit becomes new LSB
    }
}

/**
 * Calculate and send frequency to AD9850
 * 
 * This is the core frequency control function. It performs:
 * 1. IF offset calculation (if enabled)
 * 2. Frequency tuning word computation
 * 3. Serial data transmission (32 bits frequency + 8 bits control)
 * 4. Frequency update via FQ_UD pulse
 * 
 * FREQUENCY TUNING WORD CALCULATION:
 * The AD9850 uses a 32-bit phase accumulator. The tuning word (ΔPhase)
 * determines how much the accumulator increments each clock cycle.
 * 
 * Formula: ΔPhase = (f_out × 2^32) / f_clock
 * 
 * Where:
 * - f_out is desired output frequency
 * - 2^32 is phase accumulator resolution (4,294,967,296)
 * - f_clock is reference clock (125 MHz typical)
 * 
 * The result is a 32-bit integer that represents the phase increment
 * per reference clock cycle.
 * 
 * @param frequency: Desired output frequency in Hz (before IF offset)
 */
void HAL_AD9850_SendFrequency(int32_t frequency) {
    // Apply IF offset if enabled
    // When IF offset is active (g_ifOffsetActive == 0), we subtract the
    // intermediate frequency so the displayed frequency remains correct
    // while output is shifted for IF applications
    if (g_ifOffsetActive == 0) {  // Check if IF offset is enabled
        frequency = frequency - g_intermediateFreq;  // Subtract IF offset
    }
    
    // Check if output is disabled via SCPI
    if (!g_scpiOutputEnabled) {  // If output is disabled
        frequency = 0;  // Set frequency to 0 (effectively disable output)
    }
    
    // Calculate frequency tuning word using 64-bit arithmetic to prevent overflow
    // The calculation: tuningWord = (frequency * 2^32) / AD9850_CLOCK_FREQ
    // 2^32 = 4294967296 (exactly 4.294967296e9)
    // We use double floating point for maximum accuracy (avoids overflow)
    uint32_t tuningWord = (uint32_t)((double)frequency * 4294967296.0 / g_scpiReferenceClock);
    
    // Send the 32-bit tuning word (4 bytes) LSB first
    // The AD9850 expects data in LSB-first order, meaning the least significant
    // byte of the tuning word must be sent first
    for (int byteIndex = 0; byteIndex < 4; byteIndex++) {  // Send all 4 bytes
        // Send the least significant byte of current tuningWord
        HAL_AD9850_TransferByte(tuningWord & 0xFF);  // Send LSB first
        
        // Shift to next byte (move next byte into LSB position)
        tuningWord >>= 8;  // Right shift by 8 bits: next byte becomes new LSB
    }
    
    // Send control byte (phase and power-down control)
    // Format: [4:0]=phase, bit5=0, bit6=power-down, bit7=0
    // AD9850_PHASE_WORD = 0x000 means:
    // - Phase = 0 degrees (no phase shift)
    // - Power-down disabled (normal operation)
    // - All reserved bits = 0
    HAL_AD9850_TransferByte(AD9850_PHASE_WORD);  // Send control byte
    
    // Update frequency output
    // The FQ_UD pulse latches the newly loaded 40 bits (32 freq + 8 control)
    // into the DDS core, immediately changing the output frequency
    HAL_AD9850_PulsePin(PIN_AD9850_FQ_UD);  // Pulse FQ_UD to update output
}

// ======================================================================================================================================
// USER INTERFACE (UI) IMPLEMENTATION
// ======================================================================================================================================

/**
 * Initialize LCD display
 * 
 * This function configures the HD44780 LCD for operation:
 * - Sets display to 16 columns × 2 rows
 * - Clears display
 * - Sets cursor to home position
 * - Displays initial tuning increment
 * 
 * The LCD is initialized in 4-bit mode as configured in the LiquidCrystal
 * library constructor.
 */
void UI_LCD_Init(void) {
    g_lcd.begin(16, 2);  // Initialize 16x2 LCD (16 columns, 2 rows)
    g_lcd.setCursor(g_incrementCursorPos, 1);  // Position cursor at increment display position (line 1)
    g_lcd.print(g_incrementDisplay);            // Show initial increment value ("10 Hz")
}

/**
 * Clear entire line on LCD
 * 
 * Utility function to clear a specific line by overwriting
 * all 16 positions with spaces.
 * 
 * @param line: Line number (0 for top, 1 for bottom)
 */
void UI_LCD_ClearLine(uint8_t line) {
    g_lcd.setCursor(0, line);          // Move to start of specified line (column 0)
    for (int i = 0; i < 16; i++) {      // Write 16 spaces to clear entire line
        g_lcd.print(" ");               // Print a space character
    }
}

/**
 * Update frequency display on LCD
 * 
 * This function formats the current frequency for display:
 * 
 * DISPLAY FORMAT:
 * Position: 0123456789012345
 * Line 0:   " 0.000.000 MHz  "  (for frequencies <10 MHz)
 *           "00.000.000 MHz  "  (for frequencies >=10 MHz)
 * 
 * The decimal points are placed every 3 digits for readability:
 * - First dot after MHz digit(s)
 * - Second dot after kHz digits
 * 
 * The function also:
 * 1. Records the time of frequency change for EEPROM timing
 * 2. Marks memory as needing update (g_memoryStatus = 0)
 */
void UI_LCD_UpdateFrequency(void) {
    // Extract individual digits using integer division and modulo operations
    // This breaks the frequency into displayable digits
    g_digit_millions = g_currentFreq / 1000000;           // Extract millions (MHz) - integer division gives whole MHz
    g_digit_hundredthousands = (g_currentFreq / 100000) % 10;  // 100kHz digit (divide by 100k, then get last digit)
    g_digit_tenthousands = (g_currentFreq / 10000) % 10;       // 10kHz digit (divide by 10k, then get last digit)
    g_digit_thousands = (g_currentFreq / 1000) % 10;           // 1kHz digit (divide by 1k, then get last digit)
    g_digit_hundreds = (g_currentFreq / 100) % 10;             // 100Hz digit (divide by 100, then get last digit)
    g_digit_tens = (g_currentFreq / 10) % 10;                  // 10Hz digit (divide by 10, then get last digit)
    g_digit_ones = g_currentFreq % 10;                         // 1Hz digit (modulo 10 gives last digit)
    
    // Clear current display line
    UI_LCD_ClearLine(0);  // Clear top line (line 0) before updating
    
    // Set cursor position based on number of digits
    // This centers the display and handles the extra digit for >=10 MHz
    if (g_digit_millions > 9) {  // If frequency is 10 MHz or higher
        g_lcd.setCursor(1, 0);  // Position cursor at column 1 (leave one space at left edge)
    } else {  // Frequency is 0-9 MHz
        g_lcd.setCursor(2, 0);  // Position cursor at column 2 (leave two spaces at left edge for centering)
    }
    
    // Format and display frequency: M.XXX.XXX MHz
    g_lcd.print(g_digit_millions);           // Print MHz digit(s) (1 or 2 digits)
    g_lcd.print(".");                         // Print first decimal separator after MHz
    g_lcd.print(g_digit_hundredthousands);    // Print 100kHz digit
    g_lcd.print(g_digit_tenthousands);        // Print 10kHz digit
    g_lcd.print(g_digit_thousands);           // Print 1kHz digit
    g_lcd.print(".");                         // Print second decimal separator after kHz
    g_lcd.print(g_digit_hundreds);            // Print 100Hz digit
    g_lcd.print(g_digit_tens);                // Print 10Hz digit
    g_lcd.print(g_digit_ones);                 // Print 1Hz digit
    g_lcd.print(" MHz  ");                     // Print unit and padding spaces
    
    // Update timestamp for memory management
    // We record when the frequency last changed to implement a 2-second
    // delay before writing to EEPROM (prevents excessive writes)
    g_lastFreqChangeTime = millis();  // Record current time in milliseconds
    
    // Mark memory as needing update
    // This triggers the EEPROM write after the 2-second delay
    g_memoryStatus = 0;  // 0 = needs update (frequency changed but not yet saved)
}

/**
 * Update tuning increment display
 * 
 * Shows the current tuning step size on line 1 of the LCD.
 * The position varies based on text length to keep it roughly centered.
 * 
 * Display examples:
 * - "10 Hz"   (short)   - cursor at position 5
 * - "100 Hz"  (medium)  - cursor at position 4
 * - "2.5 kHz" (with decimal) - cursor at position 4
 */
void UI_LCD_UpdateIncrement(void) {
    UI_LCD_ClearLine(1);  // Clear bottom line (line 1) completely
    g_lcd.setCursor(g_incrementCursorPos, 1);  // Set cursor to calculated position for centering
    g_lcd.print(g_incrementDisplay);            // Show increment value (e.g., "10 Hz", "1 kHz")
}

/**
 * Show/hide IF offset active indicator
 * 
 * When IF offset is active, displays a small dot in the bottom-right
 * corner of the LCD as a visual reminder that the output frequency
 * is different from the displayed frequency.
 * 
 * Dot appears at position (15,1) when active, disappears when inactive.
 */
void UI_LCD_ShowIFOffsetIndicator(void) {
    g_lcd.setCursor(15, 1);  // Move to bottom-right corner (column 15, line 1)
    if (g_ifOffsetActive == 0) {  // If IF offset is enabled (0 = enabled)
        g_lcd.print(".");  // Print a dot to indicate IF offset is active
    } else {  // IF offset is disabled (1 = disabled)
        g_lcd.print(" ");  // Print a space (no indicator) when inactive
    }
}

/**
 * Show custom text on LCD
 * 
 * Displays user-defined text from SCPI command on line 1
 * while preserving frequency display on line 0.
 */
void UI_LCD_ShowCustomText(void) {
    UI_LCD_ClearLine(1);  // Clear bottom line (line 1) completely
    g_lcd.setCursor(0, 1);  // Start at beginning of line 1 (column 0)
    g_lcd.print(g_scpiCustomText);  // Show custom text from SCPI command
    // Pad with spaces to clear any remaining characters beyond the text
    for (int i = strlen(g_scpiCustomText); i < 16; i++) {  // Loop from end of text to column 15
        g_lcd.print(" ");  // Print spaces to fill the rest of the line
    }
}

/**
 * Restore normal LCD display
 * 
 * Returns line 1 to showing tuning increment
 * and IF offset indicator.
 */
void UI_LCD_RestoreNormalDisplay(void) {
    g_scpiCustomText[0] = '\0';  // Clear custom text (set first character to null terminator)
    UI_LCD_UpdateIncrement();  // Restore increment display on line 1
    UI_LCD_ShowIFOffsetIndicator();  // Restore IF indicator dot if needed
}

// ======================================================================================================================================
// INPUT PROCESSING IMPLEMENTATION
// ======================================================================================================================================

/**
 * Process increment button with debouncing
 * 
 * Monitors the increment selection button (active LOW) and cycles through
 * tuning steps when pressed. The button is hardware debounced with a
 * capacitor, but we also add a software delay in Input_UpdateIncrement()
 * to prevent accidental double-cycling.
 */
void Input_ProcessIncrementButton(void) {
    g_incrementButtonState = digitalRead(PIN_BUTTON_INCREMENT);  // Read current button state (LOW = pressed)
    
    if (g_incrementButtonState == LOW) {  // Button is pressed (active LOW)
        Input_UpdateIncrement();  // Cycle to next tuning increment
    }
}

/**
 * Update tuning increment value
 * 
 * Cycles through predefined tuning steps in this order:
 * 10Hz → 50Hz → 100Hz → 500Hz → 1kHz → 2.5kHz → 5kHz → 10kHz → 100kHz → 1MHz → (back to 10Hz)
 * 
 * Each step updates:
 * - g_tuningIncrement: The actual step value in Hz
 * - g_incrementDisplay: String for LCD display
 * - g_incrementCursorPos: LCD cursor position for centering
 * 
 * The delay(250) provides:
 * - Debouncing (prevents multiple cycles from one press)
 * - Visual feedback (allows user to see each step)
 * - Menu scroll speed control
 */
void Input_UpdateIncrement(void) {
    // Determine next increment value based on current setting
    // This implements a circular menu of tuning steps
    if (g_tuningIncrement == 10) {  // Current is 10 Hz
        g_tuningIncrement = 50;  // Change to 50 Hz
        strcpy(g_incrementDisplay, "50 Hz");  // Update display string
        g_incrementCursorPos = 5;  // Set cursor position for centering (5 spaces from left)
    }
    else if (g_tuningIncrement == 50) {  // Current is 50 Hz
        g_tuningIncrement = 100;  // Change to 100 Hz
        strcpy(g_incrementDisplay, "100 Hz");  // Update display string
        g_incrementCursorPos = 4;  // Set cursor position for centering (4 spaces from left)
    }
    else if (g_tuningIncrement == 100) {  // Current is 100 Hz
        g_tuningIncrement = 500;  // Change to 500 Hz
        strcpy(g_incrementDisplay, "500 Hz");  // Update display string
        g_incrementCursorPos = 4;  // Set cursor position for centering
    }
    else if (g_tuningIncrement == 500) {  // Current is 500 Hz
        g_tuningIncrement = 1000;  // Change to 1 kHz (1000 Hz)
        strcpy(g_incrementDisplay, "1 kHz");  // Update display string (fixed capitalization)
        g_incrementCursorPos = 6;  // Set cursor position for centering
    }
    else if (g_tuningIncrement == 1000) {  // Current is 1 kHz
        g_tuningIncrement = 2500;  // Change to 2.5 kHz (2500 Hz)
        strcpy(g_incrementDisplay, "2.5 kHz");  // Update display string (fixed capitalization)
        g_incrementCursorPos = 4;  // Set cursor position for centering
    }
    else if (g_tuningIncrement == 2500) {  // Current is 2.5 kHz
        g_tuningIncrement = 5000;  // Change to 5 kHz (5000 Hz)
        strcpy(g_incrementDisplay, "5 kHz");  // Update display string (fixed capitalization)
        g_incrementCursorPos = 6;  // Set cursor position for centering
    }
    else if (g_tuningIncrement == 5000) {  // Current is 5 kHz
        g_tuningIncrement = 10000;  // Change to 10 kHz (10000 Hz)
        strcpy(g_incrementDisplay, "10 kHz");  // Update display string (fixed capitalization)
        g_incrementCursorPos = 5;  // Set cursor position for centering
    }
    else if (g_tuningIncrement == 10000) {  // Current is 10 kHz
        g_tuningIncrement = 100000;  // Change to 100 kHz (100000 Hz)
        strcpy(g_incrementDisplay, "100 kHz");  // Update display string (fixed capitalization)
        g_incrementCursorPos = 4;  // Set cursor position for centering
    }
    else if (g_tuningIncrement == 100000) {  // Current is 100 kHz
        g_tuningIncrement = 1000000;  // Change to 1 MHz (1000000 Hz)
        strcpy(g_incrementDisplay, "1 MHz");  // Update display string (fixed capitalization)
        g_incrementCursorPos = 6;  // Set cursor position for centering
    }
    else {  // Any other value (or after 1 MHz) - wrap around to starting value
        g_tuningIncrement = 10;  // Reset to 10 Hz
        strcpy(g_incrementDisplay, "10 Hz");  // Update display string
        g_incrementCursorPos = 5;  // Set cursor position for centering
    }
    
    // Update display with new increment
    UI_LCD_UpdateIncrement();  // Refresh the LCD with new increment value
    
    // Delay for debounce and visual feedback
    // This prevents multiple increments from a single button press
    // and gives the user time to see the new value
    delay(250);  // Wait 250ms before returning
}

/**
 * Process IF offset toggle button
 * 
 * Monitors the IF offset button and toggles the offset state on each press.
 * Uses edge detection (compares current state to previous state) to trigger
 * only once per press/release cycle.
 * 
 * When toggled:
 * 1. Updates g_ifOffsetActive flag
 * 2. Updates LCD indicator
 * 3. Immediately recalculates and sends new frequency
 * 
 * This provides instant feedback when enabling/disabling IF offset.
 */
void Input_ProcessIFButton(void) {
    g_ifButtonState = digitalRead(PIN_BUTTON_IF_OFFSET);  // Read current button state
    
    // Detect edge transition (only on change)
    // This ensures we only act once per button press, not continuously while held
    if (g_ifButtonState != g_ifButtonPrevState) {  // State has changed since last check
        if (g_ifButtonState == LOW) {  // Button pressed (active LOW)
            g_ifOffsetActive = 0;        // Enable IF offset (0 = enabled)
            UI_LCD_ShowIFOffsetIndicator(); // Show indicator dot on LCD
        }
        else {  // Button released (HIGH)
            g_ifOffsetActive = 1;         // Disable IF offset (1 = disabled)
            UI_LCD_ShowIFOffsetIndicator(); // Remove indicator dot (prints space instead)
        }
        
        // Update frequency with new offset setting
        // This immediately applies the change so user hears/measures the effect
        HAL_AD9850_SendFrequency(g_currentFreq);  // Send current frequency with new offset setting
        
        // Save current state for next comparison
        g_ifButtonPrevState = g_ifButtonState;  // Update previous state for next edge detection
    }
}

/**
 * Process encoder push button with debouncing
 * 
 * Monitors the rotary encoder's push button as a separate input.
 * Uses simple debounce timing (50ms) to prevent false triggers.
 * Toggles the IF offset when pressed (same function as the IF button).
 */
void Input_ProcessEncoderButton(void) {
    g_encoderButtonState = digitalRead(PIN_ENCODER_BUTTON);  // Read current button state
    
    // Debounce check - only process if state has been stable for 50ms
    if (g_encoderButtonState != g_encoderButtonPrevState) {
        g_lastEncoderButtonDebounceTime = millis();  // Reset debounce timer on state change
    }
    
    if ((millis() - g_lastEncoderButtonDebounceTime) > 50) {  // Stable for 50ms
        if (g_encoderButtonState == LOW) {  // Button pressed (active LOW)
            // Toggle IF offset state
            if (g_ifOffsetActive == 1) {  // Currently disabled
                g_ifOffsetActive = 0;        // Enable IF offset
            } else {  // Currently enabled
                g_ifOffsetActive = 1;         // Disable IF offset
            }
            UI_LCD_ShowIFOffsetIndicator(); // Update indicator dot on LCD
            HAL_AD9850_SendFrequency(g_currentFreq);  // Update frequency with new offset setting
            delay(250);  // Simple debounce delay to prevent multiple toggles
        }
    }
    
    g_encoderButtonPrevState = g_encoderButtonState;  // Save current state for next comparison
}

// ======================================================================================================================================
// MEMORY MANAGEMENT IMPLEMENTATION
// ======================================================================================================================================

/**
 * Check if EEPROM contains valid stored data
 * 
 * Validates EEPROM contents by checking:
 * 1. Magic byte at address 0 equals EEPROM_MAGIC_BYTE (0xAA)
 * 2. Version byte at address 1 equals EEPROM_VERSION (0x01)
 * 
 * This two-step validation prevents reading garbage from uninitialized
 * EEPROM or data from different firmware versions.
 * 
 * @return: true if valid data exists, false otherwise
 */
bool Memory_IsValid(void) {
    // Check magic number (signature) at address 0
    if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC_BYTE) {  // Read and compare magic byte
        return false;  // No signature or different firmware - data invalid
    }
    // Check version for compatibility at address 1
    if (EEPROM.read(EEPROM_ADDR_VERSION) != EEPROM_VERSION) {  // Read and compare version
        return false;  // Different data structure version - incompatible
    }
    return true;  // All checks passed - data is valid
}

/**
 * Initialize EEPROM with default values
 * 
 * Called when:
 * - EEPROM contains no valid data (first run)
 * - FORCE_EEPROM_INIT is set to 1
 * 
 * Writes:
 * 1. Magic byte (0xAA) to identify our data
 * 2. Version byte (0x01) for future compatibility
 * 3. Default frequency (7.050 MHz) as 32-bit value
 * 
 * Default frequency of 7.050 MHz is chosen as a common amateur radio
 * frequency (40m band) that's useful for testing.
 */
void Memory_InitDefault(void) {
    // Write validation bytes
    EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC_BYTE);  // Write magic byte at address 0
    EEPROM.write(EEPROM_ADDR_VERSION, EEPROM_VERSION);  // Write version byte at address 1
    
    // Store default frequency (7.050 MHz)
    // This is a common frequency that's useful for initial testing
    uint32_t defaultFreq = 7050000;  // 7.050 MHz in Hz
    
    // Write 32-bit frequency MSB first
    // We store MSB first for easier reading by humans if examined
    for (int i = 0; i < 4; i++) {  // Loop through 4 bytes
        // Extract each byte from most significant to least
        // (24 - (i * 8)) gives shift amounts: 24, 16, 8, 0
        uint8_t byteVal = (defaultFreq >> (24 - (i * 8))) & 0xFF;  // Extract byte and mask to 8 bits
        EEPROM.write(EEPROM_ADDR_FREQ_MSB + i, byteVal);  // Write byte at appropriate address
    }
}

/**
 * Load frequency from EEPROM
 * 
 * Retrieves stored frequency on startup. Process:
 * 1. If FORCE_EEPROM_INIT is set, use default (for initial programming)
 * 2. Else, validate EEPROM data
 * 3. If valid, reconstruct 32-bit frequency from stored bytes
 * 4. If invalid, initialize with default
 * 
 * The reconstructed frequency becomes g_currentFreq, which will be
 * output after initialization completes.
 */
void Memory_LoadFrequency(void) {
    // Check if forced initialization is requested
    // This is used only during initial programming
    if (FORCE_EEPROM_INIT == 1) {  // Force init flag is set
        Memory_InitDefault();  // Initialize EEPROM with defaults
        g_currentFreq = 7050000;  // Set current frequency to default (7.050 MHz)
        return;  // Exit early
    }
    
    // Validate stored data
    if (Memory_IsValid()) {  // Check if EEPROM contains valid data
        // Reconstruct 32-bit frequency from individual bytes
        // We stored MSB first, so we reconstruct by shifting and ORing
        uint32_t storedFreq = 0;  // Start with 0
        for (int i = 0; i < 4; i++) {  // Loop through 4 bytes
            // Build the 32-bit value: shift existing left, add new byte
            storedFreq = (storedFreq << 8) | EEPROM.read(EEPROM_ADDR_FREQ_MSB + i);  // Shift and add next byte
        }
        g_currentFreq = storedFreq;  // Set current frequency to stored value
    } else {
        // No valid data found - initialize with defaults
        Memory_InitDefault();  // Initialize EEPROM with defaults
        g_currentFreq = 7050000;  // Set current frequency to default (7.050 MHz)
    }
}

/**
 * Store current frequency to EEPROM
 * 
 * Called 2 seconds after the last frequency change to:
 * 1. Prevent excessive EEPROM writes (limited to ~100,000 cycles)
 * 2. Avoid writing during rapid tuning
 * 3. Ensure stable frequency is stored
 * 
 * Stores the frequency as 4 bytes (32-bit value) MSB first, maintaining
 * the same format as Memory_LoadFrequency() expects.
 */
void Memory_StoreFrequency(void) {
    // Store 32-bit frequency value MSB first
    for (int i = 0; i < 4; i++) {  // Loop through 4 bytes
        // Extract each byte from most significant to least
        // (24 - (i * 8)) gives shift amounts: 24, 16, 8, 0
        uint8_t byteVal = (g_currentFreq >> (24 - (i * 8))) & 0xFF;  // Extract byte and mask to 8 bits
        EEPROM.write(EEPROM_ADDR_FREQ_MSB + i, byteVal);  // Write byte to EEPROM
    }
    
    g_memoryStatus = 1;  // Mark as stored (1 = up to date, prevents immediate re-write)
}

// ======================================================================================================================================
// INTERRUPT SERVICE ROUTINES
// ======================================================================================================================================

/**
 * Pin Change Interrupt for Rotary Encoder
 * 
 * This ISR (Interrupt Service Routine) is triggered automatically whenever
 * pins D2 or D3 change state. It processes encoder rotation in real-time,
 * independent of the main loop() execution.
 * 
 * WHY INTERRUPTS?
 * - Ensures no encoder steps are missed
 * - Provides immediate response to user input
 * - Works even during LCD updates or other operations
 * 
 * PROCESSING:
 * 1. Call rotary library to decode quadrature signals
 * 2. If rotation detected, update frequency by current increment
 * 3. Apply frequency limits to prevent out-of-range values
 * 
 * IMPORTANT:
 * - ISRs should be kept short and fast
 * - No LCD or serial operations here (they're too slow)
 * - Variables shared with main loop must be declared volatile
 *   (though we use int_fast32_t which is fine on AVR)
 */
ISR(PCINT2_vect) {  // Pin Change Interrupt for Port D (pins 0-7)
    // Process the encoder to update its internal state
    g_rotaryEncoder.service();  // Call BasicEncoder service routine to handle pin changes (Replaced .process() with .service())
}

// ======================================================================================================================================
// SCPI IMPLEMENTATION
// ======================================================================================================================================

/**
 * Initialize SCPI subsystem
 * 
 * Sets up serial communication and initializes
 * SCPI status registers and error queue.
 */
void SCPI_Init(void) {
    Serial.begin(115200);  // Initialize serial communication at 115200 baud
    while (!Serial) { ; }  // Wait for serial port to connect (needed for USB)
    
    // Clear SCPI buffers and registers
    g_scpiInputIndex = 0;  // Reset input buffer index to start
    memset(g_scpiInputBuffer, 0, SCPI_INPUT_BUFFER_SIZE);  // Clear input buffer (fill with zeros)
    
    // Initialize error queue
    g_scpiErrorHead = 0;  // Reset error queue head (write pointer) to start
    g_scpiErrorTail = 0;  // Reset error queue tail (read pointer) to start
    for (int i = 0; i < SCPI_ERROR_QUEUE_SIZE; i++) {  // Loop through all queue slots
        g_scpiErrorQueue[i] = 0;  // Clear each error slot (set to 0)
    }
    
    // Initialize status registers
    g_scpiStatusByte = 0;  // Clear status byte (all bits 0)
    g_scpiServiceRequestEnable = 0;  // Clear service request enable (no SRQ generation)
    g_scpiEventStatusRegister = 0;  // Clear event status register (all bits 0)
    g_scpiEventStatusEnable = 0;  // Clear event status enable (no bits enabled)
    
    // Queue success message
    SCPI_QueueError(SCPI_ERR_NO_ERROR);  // Queue "no error" as initial state
}

/**
 * Queue an error in the SCPI error queue
 * 
 * @param error: Error code to queue
 */
void SCPI_QueueError(int16_t error) {
    uint8_t nextHead = (g_scpiErrorHead + 1) % SCPI_ERROR_QUEUE_SIZE;  // Calculate next head position (wrap around)
    
    if (nextHead != g_scpiErrorTail) {  // Check if queue is not full (head+1 != tail means space available)
        g_scpiErrorQueue[g_scpiErrorHead] = error;  // Store error at current head position
        g_scpiErrorHead = nextHead;  // Advance head pointer to next position
    }
    // If queue is full, oldest error is overwritten (typical SCPI behavior)
}

/**
 * Get the next error from the error queue
 * 
 * @return: Next error code, or 0 if queue empty
 */
int16_t SCPI_GetNextError(void) {
    if (g_scpiErrorHead == g_scpiErrorTail) {  // Check if queue is empty (head == tail means empty)
        return 0;  // Return 0 (no error) if queue empty
    }
    
    int16_t error = g_scpiErrorQueue[g_scpiErrorTail];  // Get error from tail position (oldest)
    g_scpiErrorTail = (g_scpiErrorTail + 1) % SCPI_ERROR_QUEUE_SIZE;  // Advance tail pointer (wrap around)
    return error;  // Return the error code
}

/**
 * Print a response string to serial
 * 
 * @param response: String to print (will add CRLF)
 */
void SCPI_PrintResponse(const char* response) {
    Serial.print(response);  // Print the response string
    Serial.print("\r\n");  // Add CRLF (Carriage Return + Line Feed) termination
}

/**
 * Print a number as response
 * 
 * @param value: Number to print
 */
void SCPI_PrintNumber(uint32_t value) {
    Serial.print(value);  // Print the number as decimal
    Serial.print("\r\n");  // Add CRLF (Carriage Return + Line Feed) termination
}

/**
 * Set a bit in the Status Byte Register
 * 
 * @param bit: Bit number to set (0-7)
 * @param value: true to set, false to clear
 */
void SCPI_SetStatusBit(uint8_t bit, bool value) {
    if (value) {  // If setting the bit
        g_scpiStatusByte |= (1 << bit);  // Set the bit using OR (bitwise OR)
    } else {  // If clearing the bit
        g_scpiStatusByte &= ~(1 << bit);  // Clear the bit using AND with inverted mask
    }
}

/**
 * Update Event Status Register and check for service request
 * 
 * @param bit: Bit number that changed
 */
void SCPI_UpdateESR(uint8_t bit) {
    g_scpiEventStatusRegister |= (1 << bit);  // Set the bit in ESR using OR
    
    // Check if this bit is enabled in ESE (Event Status Enable)
    if (g_scpiEventStatusEnable & (1 << bit)) {  // If bit is enabled in ESE
        SCPI_SetStatusBit(SCPI_STB_ESB, true);  // Set ESB (Event Status Bit) in STB
        
        // Check if ESB is enabled in SRE (Service Request Enable)
        if (g_scpiServiceRequestEnable & (1 << SCPI_STB_ESB)) {  // If ESB enabled in SRE
            SCPI_SetStatusBit(SCPI_STB_SRQ, true);  // Set SRQ (Service Request) bit
            // Note: Hardware interrupt would be triggered here in a real implementation
        }
    }
}

/**
 * Parse a number with optional unit suffix
 * 
 * Supports suffixes: Hz, kHz, MHz, GHz
 * Examples: "10 MHz", "1000", "2.5 kHz"
 * 
 * @param str: String to parse
 * @param hasUnit: Output parameter, true if unit was found
 * @param unit: Output parameter, unit string if found
 * @param defaultUnit: Default multiplier (1 = Hz)
 * @return: Parsed numeric value in defaultUnit (Hz)
 */
uint32_t SCPI_ParseNumber(char* str, bool* hasUnit, char* unit, uint32_t defaultUnit) {
    char* endPtr;  // Pointer for strtod end detection (where parsing stopped)
    double value = strtod(str, &endPtr);  // Parse the numeric part as double
    
    // Skip whitespace after number
    while (*endPtr == ' ') endPtr++;  // Move endPtr past any spaces
    
    // Check if there's a unit
    if (*endPtr != '\0') {  // If endPtr is not at null terminator, there's text after number
        *hasUnit = true;  // Unit found - set flag
        strcpy(unit, endPtr);  // Copy unit string to output parameter
        
        // Convert unit to uppercase for comparison
        for (int i = 0; unit[i]; i++) {  // Loop through each character
            unit[i] = toupper(unit[i]);  // Convert to uppercase
        }
        
        // Apply unit multiplier
        if (strcmp(unit, "HZ") == 0) {  // Hertz
            return (uint32_t)(value);  // Return as is (multiply by 1)
        } else if (strcmp(unit, "KHZ") == 0) {  // Kilohertz
            return (uint32_t)(value * 1000);  // Multiply by 1000 to convert to Hz
        } else if (strcmp(unit, "MHZ") == 0) {  // Megahertz
            return (uint32_t)(value * 1000000);  // Multiply by 1,000,000 to convert to Hz
        } else if (strcmp(unit, "GHZ") == 0) {  // Gigahertz
            return (uint32_t)(value * 1000000000UL);  // Multiply by 1,000,000,000 to convert to Hz
        }
    } else {
        *hasUnit = false;  // No unit found - clear flag
        unit[0] = '\0';  // Set empty unit string
    }
    
    // No unit or unknown unit, return raw value multiplied by defaultUnit
    return (uint32_t)(value * defaultUnit);  // Apply default multiplier (usually 1 for Hz)
}

/**
 * Process incoming SCPI commands from serial
 * 
 * Called in main loop to handle serial input.
 * Accumulates characters until newline, then parses.
 */
void SCPI_Process(void) {
    while (Serial.available() > 0) {  // Check if data is available in serial buffer
        char c = Serial.read();  // Read next character from serial
        
        if (c == '\n' || c == '\r') {  // Check for line ending (LF or CR)
            if (g_scpiInputIndex > 0) {  // Only process if buffer not empty
                g_scpiInputBuffer[g_scpiInputIndex] = '\0';  // Null terminate the string
                SCPI_ParseCommand(g_scpiInputBuffer);  // Parse and execute command
                g_scpiInputIndex = 0;  // Reset buffer index for next command
                memset(g_scpiInputBuffer, 0, SCPI_INPUT_BUFFER_SIZE);  // Clear buffer (fill with zeros)
            }
        } else if (g_scpiInputIndex < SCPI_INPUT_BUFFER_SIZE - 1) {  // Check if buffer has space
            g_scpiInputBuffer[g_scpiInputIndex++] = c;  // Store character and increment index
        } else {
            // Buffer overflow - should never happen with proper size
            SCPI_QueueError(SCPI_ERR_OUT_OF_MEMORY);  // Queue overflow error
        }
    }
}

/**
 * Parse and execute a SCPI command
 * 
 * Splits command and parameters, then routes to appropriate handler.
 * 
 * @param cmd: Complete command string
 */
void SCPI_ParseCommand(char* cmd) {
    // Remove leading whitespace
    while (*cmd == ' ') cmd++;  // Skip any spaces at beginning
    
    // Check if empty line
    if (*cmd == '\0') return;  // Nothing to parse
    
    char* params = strchr(cmd, ' ');  // Find first space (parameter separator)
    if (params != NULL) {  // If space found (parameters exist)
        *params = '\0';  // Split command and parameters by replacing space with null terminator
        params++;  // Move to start of parameters (after the space)
        // Remove leading whitespace from parameters
        while (*params == ' ') params++;  // Skip any spaces at beginning of parameters
    }
    
    // Check for IEEE common commands (start with *)
    if (cmd[0] == '*') {  // Commands starting with * are IEEE 488.2 common commands
        SCPI_HandleIEEECommon(cmd, params);  // Handle IEEE 488.2 commands
    }
    // Check for frequency subsystem
    else if (strcasecmp(cmd, "FREQuency") == 0 || strcasecmp(cmd, "FREQ") == 0) {  // FREQ or FREQUENCY
        SCPI_HandleFrequency(cmd, params);  // Handle frequency commands
    }
    // Check for output subsystem
    else if (strcasecmp(cmd, "OUTPut") == 0 || strcasecmp(cmd, "OUTP") == 0) {  // OUTP or OUTPUT
        SCPI_HandleOutput(cmd, params);  // Handle output commands
    }
    // Check for display subsystem
    else if (strcasecmp(cmd, "DISPlay") == 0 || strcasecmp(cmd, "DISP") == 0) {  // DISP or DISPLAY
        SCPI_HandleDisplay(cmd, params);  // Handle display commands
    }
    // Check for system subsystem
    else if (strcasecmp(cmd, "SYSTem") == 0 || strcasecmp(cmd, "SYST") == 0) {  // SYST or SYSTEM
        SCPI_HandleSystem(cmd, params);  // Handle system commands
    }
    // Check for memory subsystem
    else if (strcasecmp(cmd, "MEMory") == 0 || strcasecmp(cmd, "MEM") == 0) {  // MEM or MEMORY
        SCPI_HandleMemory(cmd, params);  // Handle memory commands
    }
    // Check for calibration subsystem
    else if (strcasecmp(cmd, "CALibration") == 0 || strcasecmp(cmd, "CAL") == 0) {  // CAL or CALIBRATION
        SCPI_HandleCalibration(cmd, params);  // Handle calibration commands
    }
    else {  // Unknown command
        SCPI_QueueError(SCPI_ERR_INVALID_COMMAND);  // Queue error for invalid command
        SCPI_UpdateESR(5);  // Set Command Error bit (bit 5) in ESR
    }
}

/**
 * Handle IEEE 488.2 common commands
 * 
 * @param cmd: Command (with *)
 * @param params: Parameters (if any)
 */
void SCPI_HandleIEEECommon(char* cmd, char* params) {
    // *IDN? - Identification query
    if (strcasecmp(cmd, "*IDN?") == 0) {  // Check for identification query
        SCPI_PrintResponse("AD7C,AD9850 Generator,4.1,SCPI-1.0");  // Return identification string
    }
    // *RST - Reset to default state
    else if (strcasecmp(cmd, "*RST") == 0) {  // Check for reset command
        g_currentFreq = 7050000;  // Reset to default frequency (7.050 MHz)
        g_tuningIncrement = 10;  // Reset to 10 Hz step
        g_ifOffsetActive = 1;  // Disable IF offset (1 = disabled)
        g_scpiOutputEnabled = true;  // Enable output
        g_scpiCustomText[0] = '\0';  // Clear custom text (set first char to null)
        HAL_AD9850_SendFrequency(g_currentFreq);  // Send frequency to hardware
        UI_LCD_UpdateFrequency();  // Update display with new frequency
        UI_LCD_RestoreNormalDisplay();  // Restore normal display (clear custom text)
        SCPI_UpdateESR(0);  // Set Operation Complete bit (bit 0) in ESR
    }
    // *OPC - Operation complete
    else if (strcasecmp(cmd, "*OPC") == 0) {  // Check for operation complete command
        SCPI_UpdateESR(0);  // Set Operation Complete bit (bit 0) in ESR
    }
    // *OPC? - Operation complete query
    else if (strcasecmp(cmd, "*OPC?") == 0) {  // Check for operation complete query
        SCPI_PrintNumber(1);  // Return 1 (always complete in this implementation)
    }
    // *WAI - Wait to continue
    else if (strcasecmp(cmd, "*WAI") == 0) {  // Check for wait command
        // In this implementation, all commands complete immediately
        // So WAI does nothing
    }
    // *TST? - Self-test query
    else if (strcasecmp(cmd, "*TST?") == 0) {  // Check for self-test query
        // Perform simple self-test
        uint8_t testResult = 0;  // Start with 0 = pass
        if (g_scpiReferenceClock != AD9850_CLOCK_FREQ) {  // Check if reference clock changed
            testResult |= 1;  // Bit 0: Clock error - set bit 0
        }
        SCPI_PrintNumber(testResult);  // Return test result (0 = pass, non-zero = fail)
    }
    // *CLS - Clear status
    else if (strcasecmp(cmd, "*CLS") == 0) {  // Check for clear status command
        g_scpiStatusByte = 0;  // Clear status byte (all bits to 0)
        g_scpiEventStatusRegister = 0;  // Clear event status register (all bits to 0)
        g_scpiErrorHead = g_scpiErrorTail;  // Clear error queue (head = tail means empty)
    }
    // *ESE <value> - Event status enable
    else if (strcasecmp(cmd, "*ESE") == 0 && params != NULL) {  // Check for ESE with parameter
        g_scpiEventStatusEnable = atoi(params);  // Set ESE register from parameter (convert string to int)
    }
    // *ESE? - Event status enable query
    else if (strcasecmp(cmd, "*ESE?") == 0) {  // Check for ESE query
        SCPI_PrintNumber(g_scpiEventStatusEnable);  // Return ESE value
    }
    // *ESR? - Event status register query
    else if (strcasecmp(cmd, "*ESR?") == 0) {  // Check for ESR query
        SCPI_PrintNumber(g_scpiEventStatusRegister);  // Return ESR value
        g_scpiEventStatusRegister = 0;  // Clear ESR after read (standard behavior)
    }
    // *SRE <value> - Service request enable
    else if (strcasecmp(cmd, "*SRE") == 0 && params != NULL) {  // Check for SRE with parameter
        g_scpiServiceRequestEnable = atoi(params);  // Set SRE register from parameter
    }
    // *SRE? - Service request enable query
    else if (strcasecmp(cmd, "*SRE?") == 0) {  // Check for SRE query
        SCPI_PrintNumber(g_scpiServiceRequestEnable);  // Return SRE value
    }
    // *STB? - Status byte query
    else if (strcasecmp(cmd, "*STB?") == 0) {  // Check for STB query
        SCPI_PrintNumber(g_scpiStatusByte);  // Return STB value
    }
    else {  // Unknown IEEE command
        SCPI_QueueError(SCPI_ERR_INVALID_COMMAND);  // Queue error for invalid command
    }
}

/**
 * Handle frequency subsystem commands
 * 
 * @param cmd: Command
 * @param params: Parameters
 */
void SCPI_HandleFrequency(char* cmd, char* params) {
    // Check for :FREQuency? (query)
    if (params == NULL) {  // No parameters means query
        SCPI_PrintNumber(g_currentFreq);  // Return current frequency in Hz
        return;  // Exit
    }
    
    // Check for :FREQuency:STEP
    if (strncasecmp(params, "STEP", 4) == 0) {  // Check if parameter starts with "STEP"
        char* stepParams = params + 4;  // Move pointer past "STEP"
        while (*stepParams == ' ') stepParams++;  // Skip any spaces
        
        // Check for :FREQuency:STEP? (query)
        if (stepParams[0] == '?') {  // If next character is '?'
            SCPI_PrintNumber(g_tuningIncrement);  // Return current step value in Hz
            return;  // Exit
        }
        
        // :FREQuency:STEP <value> (set)
        bool hasUnit;  // Flag for unit presence
        char unit[10];  // Unit string buffer
        uint32_t value = SCPI_ParseNumber(stepParams, &hasUnit, unit, 1);  // Parse with default Hz
        if (value >= 1 && value <= 1000000) {  // Check valid range (1 Hz to 1 MHz)
            g_tuningIncrement = value;  // Set new step value
            
            // Update display string based on value
            if (value < 1000) {  // Less than 1 kHz
                sprintf(g_incrementDisplay, "%ld Hz", value);  // Format as Hz (e.g., "500 Hz")
            } else {  // 1 kHz or more
                // Format as kHz with one decimal place (e.g., "2.5 kHz")
                sprintf(g_incrementDisplay, "%ld.%ld kHz", value/1000, (value%1000)/100);  // Format as kHz
            }
            UI_LCD_UpdateIncrement();  // Update display with new increment
        } else {  // Value out of range
            SCPI_QueueError(SCPI_ERR_INVALID_PARAMETER);  // Queue error
        }
    }
    // Check for :FREQuency:OFFSet
    else if (strncasecmp(params, "OFFSet", 6) == 0) {  // Check if parameter starts with "OFFSET"
        char* offsetParams = params + 6;  // Move pointer past "OFFSET"
        while (*offsetParams == ' ') offsetParams++;  // Skip any spaces
        
        // Check for :FREQuency:OFFSet:STATe
        if (strncasecmp(offsetParams, "STATe", 5) == 0) {  // Check for ":STATE" after OFFSET
            char* stateParams = offsetParams + 5;  // Move past "STATE"
            while (*stateParams == ' ') stateParams++;  // Skip any spaces
            
            // Check for :FREQuency:OFFSet:STATe? (query)
            if (stateParams[0] == '?') {  // If next character is '?'
                // Return state (1 = active, 0 = inactive) - note g_ifOffsetActive is inverted (0=active)
                SCPI_PrintNumber(g_ifOffsetActive == 0 ? 1 : 0);  // Return 1 if active, 0 if inactive
                return;  // Exit
            }
            
            // :FREQuency:OFFSet:STATe <0|1> (set)
            uint8_t state = atoi(stateParams);  // Parse state parameter
            if (state == 0) {  // Disable offset (state 0)
                g_ifOffsetActive = 1;  // Disable offset (1 = disabled)
                UI_LCD_ShowIFOffsetIndicator();  // Update indicator (removes dot)
                HAL_AD9850_SendFrequency(g_currentFreq);  // Update frequency with new offset setting
            } else if (state == 1) {  // Enable offset (state 1)
                g_ifOffsetActive = 0;  // Enable offset (0 = enabled)
                UI_LCD_ShowIFOffsetIndicator();  // Update indicator (shows dot)
                HAL_AD9850_SendFrequency(g_currentFreq);  // Update frequency with new offset setting
            } else {  // Invalid state value
                SCPI_QueueError(SCPI_ERR_INVALID_PARAMETER);  // Queue error
            }
        }
        // Check for :FREQuency:OFFSet? (query)
        else if (offsetParams[0] == '?') {  // If next character is '?' after OFFSET
            SCPI_PrintNumber(g_intermediateFreq);  // Return IF offset value in Hz
        }
        // :FREQuency:OFFSet <value> (set)
        else {  // Must be setting the offset value
            bool hasUnit;  // Flag for unit presence
            char unit[10];  // Unit string buffer
            uint32_t value = SCPI_ParseNumber(offsetParams, &hasUnit, unit, 1);  // Parse with default Hz
            if (value >= 0 && value <= 10000000) {  // Check valid range (0-10 MHz)
                g_intermediateFreq = value;  // Set new IF offset value
                HAL_AD9850_SendFrequency(g_currentFreq);  // Update frequency with new offset
            } else {  // Value out of range
                SCPI_QueueError(SCPI_ERR_INVALID_PARAMETER);  // Queue error
            }
        }
    }
    // Check for :FREQuency:LIMIT?
    else if (strncasecmp(params, "LIMIT?", 6) == 0) {  // Check for "LIMIT?" parameter
        char* limitParams = params + 6;  // Move past "LIMIT?"
        while (*limitParams == ' ') limitParams++;  // Skip any spaces
        
        if (strncasecmp(limitParams, "MIN", 3) == 0) {  // Check for MIN parameter
            SCPI_PrintNumber(FREQ_LOWER_LIMIT);  // Return minimum frequency (0 Hz)
        } else if (strncasecmp(limitParams, "MAX", 3) == 0) {  // Check for MAX parameter
            SCPI_PrintNumber(FREQ_UPPER_LIMIT);  // Return maximum frequency (30 MHz)
        } else {  // Invalid parameter
            SCPI_QueueError(SCPI_ERR_INVALID_PARAMETER);  // Queue error
        }
    }
    // :FREQuency <value> (set)
    else {  // Must be setting the frequency value
        bool hasUnit;  // Flag for unit presence
        char unit[10];  // Unit string buffer
        uint32_t value = SCPI_ParseNumber(params, &hasUnit, unit, 1);  // Parse with default Hz
        if (value >= FREQ_LOWER_LIMIT && value <= FREQ_UPPER_LIMIT) {  // Check valid range
            g_currentFreq = value;  // Set new frequency
            // Note: Actual hardware update happens in main loop (when g_currentFreq != g_lastDisplayedFreq)
        } else {  // Value out of range
            SCPI_QueueError(SCPI_ERR_INVALID_PARAMETER);  // Queue error
        }
    }
}

/**
 * Handle output subsystem commands
 * 
 * @param cmd: Command
 * @param params: Parameters
 */
void SCPI_HandleOutput(char* cmd, char* params) {
    // Check for :OUTPut? (query)
    if (params == NULL || params[0] == '?') {  // No parameters or '?' means query
        SCPI_PrintNumber(g_scpiOutputEnabled ? 1 : 0);  // Return output state (1=ON, 0=OFF)
        return;  // Exit
    }
    
    // Check for :OUTPut:PROTection?
    if (strncasecmp(params, "PROTection?", 11) == 0) {  // Check for "PROTECTION?" parameter
        SCPI_PrintNumber(g_scpiOutputProtection ? 1 : 0);  // Return protection state (1=tripped)
        return;  // Exit
    }
    
    // :OUTPut <0|1|OFF|ON> (set)
    if (strncasecmp(params, "ON", 2) == 0 || strcasecmp(params, "1") == 0) {  // ON or 1
        g_scpiOutputEnabled = true;  // Enable output
        HAL_AD9850_SendFrequency(g_currentFreq);  // Update hardware (send current frequency)
    } else if (strncasecmp(params, "OFF", 3) == 0 || strcasecmp(params, "0") == 0) {  // OFF or 0
        g_scpiOutputEnabled = false;  // Disable output
        HAL_AD9850_SendFrequency(0);  // Send 0 Hz to effectively disable output
    } else {  // Invalid parameter
        SCPI_QueueError(SCPI_ERR_INVALID_PARAMETER);  // Queue error
    }
}

/**
 * Handle display subsystem commands
 * 
 * @param cmd: Command
 * @param params: Parameters
 */
void SCPI_HandleDisplay(char* cmd, char* params) {
    // Check for :DISPlay:TEXT
    if (strncasecmp(params, "TEXT", 4) == 0) {  // Check if parameter starts with "TEXT"
        char* textParams = params + 4;  // Move pointer past "TEXT"
        while (*textParams == ' ') textParams++;  // Skip any spaces
        
        // Check for :DISPlay:TEXT:CLEar
        if (strncasecmp(textParams, "CLEar", 5) == 0) {  // Check for "CLEAR" after TEXT
            UI_LCD_RestoreNormalDisplay();  // Restore normal display (clear custom text)
            return;  // Exit
        }
        
        // :DISPlay:TEXT <string> (set)
        if (*textParams != '\0') {  // If there's text to display
            // Remove quotes if present
            if (textParams[0] == '"') {  // Check for opening quote
                textParams++;  // Skip opening quote
                char* endQuote = strchr(textParams, '"');  // Find closing quote
                if (endQuote != NULL) {  // If closing quote found
                    *endQuote = '\0';  // Terminate at closing quote (replace with null)
                }
            }
            
            strncpy(g_scpiCustomText, textParams, 16);  // Copy custom text (max 16 chars)
            g_scpiCustomText[16] = '\0';  // Ensure null termination at position 16
            UI_LCD_ShowCustomText();  // Display custom text on LCD
        }
    }
    // :DISPlay:CONTrast <value> (set)
    else if (strncasecmp(params, "CONTrast", 8) == 0) {  // Check for "CONTRAST" parameter
        char* contrastParams = params + 8;  // Move past "CONTRAST"
        while (*contrastParams == ' ') contrastParams++;  // Skip any spaces
        
        // Check for :DISPlay:CONTrast? (query)
        if (contrastParams[0] == '?') {  // If next character is '?'
            SCPI_PrintNumber(50);  // Return fixed contrast (hardware dependent - 50% default)
            return;  // Exit
        }
        
        // :DISPlay:CONTrast <value> (set)
        uint8_t contrast = atoi(contrastParams);  // Parse contrast value (0-100)
        if (contrast >= 0 && contrast <= 100) {  // Check valid range
            // Note: Actual contrast control would require PWM output to LCD Vo pin
            // This is a placeholder for hardware-specific implementation
            // In real implementation, you'd set a PWM duty cycle here
        } else {  // Invalid contrast value
            SCPI_QueueError(SCPI_ERR_INVALID_PARAMETER);  // Queue error
        }
    }
    else {  // Unknown display command
        SCPI_QueueError(SCPI_ERR_INVALID_COMMAND);  // Queue error
    }
}

/**
 * Handle system subsystem commands
 * 
 * @param cmd: Command
 * @param params: Parameters
 */
void SCPI_HandleSystem(char* cmd, char* params) {
    // Check for :SYSTem:ERRor?
    if (strncasecmp(params, "ERRor?", 6) == 0) {  // Check for "ERROR?" parameter
        int16_t error = SCPI_GetNextError();  // Get next error from queue
        if (error == 0) {  // No error
            SCPI_PrintResponse("0,\"No error\"");  // Return standard "no error" response
        } else {  // Error exists
            char errorStr[50];  // Buffer for error string
            sprintf(errorStr, "%d,\"Error %d\"", error, error);  // Format error message
            SCPI_PrintResponse(errorStr);  // Return error string
        }
    }
    // Check for :SYSTem:VERSion?
    else if (strncasecmp(params, "VERSion?", 8) == 0) {  // Check for "VERSION?" parameter
        SCPI_PrintResponse("1999.0");  // Return SCPI version (1999.0 is standard)
    }
    // Check for :SYSTem:PRESet
    else if (strncasecmp(params, "PRESet", 6) == 0) {  // Check for "PRESET" parameter
        g_currentFreq = 7050000;  // Preset frequency to 7.050 MHz
        g_tuningIncrement = 10;  // Preset step to 10 Hz
        g_ifOffsetActive = 1;  // Disable IF offset (1 = disabled)
        g_scpiOutputEnabled = true;  // Enable output
        g_scpiCustomText[0] = '\0';  // Clear custom text
        HAL_AD9850_SendFrequency(g_currentFreq);  // Update hardware
        UI_LCD_UpdateFrequency();  // Update display
        UI_LCD_RestoreNormalDisplay();  // Restore normal display
    }
    // Check for :SYSTem:BEEPer
    else if (strncasecmp(params, "BEEPer", 6) == 0) {  // Check for "BEEPER" parameter
        char* beeperParams = params + 6;  // Move past "BEEPER"
        while (*beeperParams == ' ') beeperParams++;  // Skip any spaces
        
        // Check for :SYSTem:BEEPer? (query)
        if (beeperParams[0] == '?') {  // If next character is '?'
            SCPI_PrintNumber(g_scpiBeeperEnabled ? 1 : 0);  // Return beeper state (1=enabled)
            return;  // Exit
        }
        
        // :SYSTem:BEEPer <0|1> (set)
        uint8_t state = atoi(beeperParams);  // Parse state parameter
        if (state == 0) {  // Disable beeper
            g_scpiBeeperEnabled = false;  // Set beeper disabled
        } else if (state == 1) {  // Enable beeper
            g_scpiBeeperEnabled = true;  // Set beeper enabled
            // Beep once to confirm
            Serial.print("\a");  // ASCII bell character - beeps if terminal supports it
        } else {  // Invalid state
            SCPI_QueueError(SCPI_ERR_INVALID_PARAMETER);  // Queue error
        }
    }
    else {  // Unknown system command
        SCPI_QueueError(SCPI_ERR_INVALID_COMMAND);  // Queue error
    }
}

/**
 * Handle memory subsystem commands
 * 
 * @param cmd: Command
 * @param params: Parameters
 */
void SCPI_HandleMemory(char* cmd, char* params) {
    // Check for :MEMory:STORe
    if (strncasecmp(params, "STORe", 5) == 0) {  // Check for "STORE" parameter
        char* storeParams = params + 5;  // Move past "STORE"
        while (*storeParams == ' ') storeParams++;  // Skip any spaces
        
        uint8_t location = atoi(storeParams);  // Parse memory location number
        if (location >= 0 && location < SCPI_MAX_MEMORY_LOCATIONS) {  // Check valid range
            // Store frequency in memory location (4 bytes, MSB first)
            g_scpiMemoryLocations[location][0] = (g_currentFreq >> 24) & 0xFF;  // MSB (bits 31-24)
            g_scpiMemoryLocations[location][1] = (g_currentFreq >> 16) & 0xFF;  // Next byte (bits 23-16)
            g_scpiMemoryLocations[location][2] = (g_currentFreq >> 8) & 0xFF;   // Next byte (bits 15-8)
            g_scpiMemoryLocations[location][3] = g_currentFreq & 0xFF;          // LSB (bits 7-0)
        } else {  // Invalid location
            SCPI_QueueError(SCPI_ERR_INVALID_PARAMETER);  // Queue error
        }
    }
    // Check for :MEMory:RECall
    else if (strncasecmp(params, "RECall", 6) == 0) {  // Check for "RECALL" parameter
        char* recallParams = params + 6;  // Move past "RECALL"
        while (*recallParams == ' ') recallParams++;  // Skip any spaces
        
        uint8_t location = atoi(recallParams);  // Parse memory location number
        if (location >= 0 && location < SCPI_MAX_MEMORY_LOCATIONS) {  // Check valid range
            // Recall frequency from memory location (reconstruct 32-bit value)
            g_currentFreq = (g_scpiMemoryLocations[location][0] << 24) |  // MSB to bits 31-24
                            (g_scpiMemoryLocations[location][1] << 16) |  // Next byte to bits 23-16
                            (g_scpiMemoryLocations[location][2] << 8) |   // Next byte to bits 15-8
                            g_scpiMemoryLocations[location][3];           // LSB to bits 7-0
            // Note: Hardware update happens in main loop
        } else {  // Invalid location
            SCPI_QueueError(SCPI_ERR_INVALID_PARAMETER);  // Queue error
        }
    }
    // Check for :MEMory:CLEar
    else if (strncasecmp(params, "CLEar", 5) == 0) {  // Check for "CLEAR" parameter
        char* clearParams = params + 5;  // Move past "CLEAR"
        while (*clearParams == ' ') clearParams++;  // Skip any spaces
        
        uint8_t location = atoi(clearParams);  // Parse memory location number
        if (location >= 0 && location < SCPI_MAX_MEMORY_LOCATIONS) {  // Check valid range
            // Clear memory location (set all bytes to 0)
            g_scpiMemoryLocations[location][0] = 0;  // Clear MSB
            g_scpiMemoryLocations[location][1] = 0;  // Clear next byte
            g_scpiMemoryLocations[location][2] = 0;  // Clear next byte
            g_scpiMemoryLocations[location][3] = 0;  // Clear LSB
        } else {  // Invalid location
            SCPI_QueueError(SCPI_ERR_INVALID_PARAMETER);  // Queue error
        }
    }
    else {  // Unknown memory command
        SCPI_QueueError(SCPI_ERR_INVALID_COMMAND);  // Queue error
    }
}

/**
 * Handle calibration subsystem commands
 * 
 * @param cmd: Command
 * @param params: Parameters
 */
void SCPI_HandleCalibration(char* cmd, char* params) {
    // Check if calibration is locked
    if (g_scpiCalibrationLocked) {  // Calibration is locked
        // Check for :CALibration:SECure:CODE (password entry)
        if (strncasecmp(params, "SECure:CODE", 11) == 0) {  // Check for "SECURE:CODE" parameter
            char* codeParams = params + 11;  // Move past "SECURE:CODE"
            while (*codeParams == ' ') codeParams++;  // Skip any spaces
            
            uint16_t password = atoi(codeParams);  // Parse password
            if (password == g_scpiCalibrationPassword) {  // Check if password matches
                g_scpiCalibrationLocked = false;  // Unlock calibration
                SCPI_PrintResponse("Calibration unlocked");  // Confirm to user
            } else {  // Wrong password
                SCPI_QueueError(SCPI_ERR_CALIBRATION_PASSWORD);  // Queue error
            }
            return;  // Exit
        }
        
        // If locked and not trying to unlock, return error
        SCPI_QueueError(SCPI_ERR_CALIBRATION_LOCKED);  // Queue "calibration locked" error
        return;  // Exit
    }
    
    // Calibration is unlocked - handle commands
    // Check for :CALibration:REFerence
    if (strncasecmp(params, "REFerence", 9) == 0) {  // Check for "REFERENCE" parameter
        char* refParams = params + 9;  // Move past "REFERENCE"
        while (*refParams == ' ') refParams++;  // Skip any spaces
        
        // Check for :CALibration:REFerence? (query)
        if (refParams[0] == '?') {  // If next character is '?'
            SCPI_PrintNumber(g_scpiReferenceClock);  // Return current reference clock in Hz
            return;  // Exit
        }
        
        // :CALibration:REFerence <value> (set)
        bool hasUnit;  // Flag for unit presence
        char unit[10];  // Unit string buffer
        uint32_t value = SCPI_ParseNumber(refParams, &hasUnit, unit, 1000000);  // Parse with default MHz
        if (value >= 1000000 && value <= 200000000) {  // Check valid range (1-200 MHz)
            g_scpiReferenceClock = value;  // Set new reference clock value
        } else {  // Value out of range
            SCPI_QueueError(SCPI_ERR_INVALID_PARAMETER);  // Queue error
        }
    }
    // Check for :CALibration:STORe
    else if (strncasecmp(params, "STORe", 5) == 0) {  // Check for "STORE" parameter
        g_scpiCalibrationStoredRef = g_scpiReferenceClock;  // Store current reference to calibration memory
        // Note: In a real implementation, you'd save to EEPROM here
        SCPI_PrintResponse("Calibration stored");  // Confirm to user
    }
    // Check for :CALibration:LOCK (lock calibration again)
    else if (strncasecmp(params, "LOCK", 4) == 0) {  // Check for "LOCK" parameter
        g_scpiCalibrationLocked = true;  // Lock calibration
        SCPI_PrintResponse("Calibration locked");  // Confirm to user
    }
    else {  // Unknown calibration command
        SCPI_QueueError(SCPI_ERR_INVALID_COMMAND);  // Queue error
    }
}

// ======================================================================================================================================
// MAIN SETUP AND LOOP FUNCTIONS
// ======================================================================================================================================

/**
 * Arduino setup function - runs once at startup
 * 
 * Initialization sequence (in order):
 * 1. Configure input pins with pull-ups
 * 2. Initialize LCD display
 * 3. Configure pin change interrupts for encoder
 * 4. Initialize AD9850 hardware
 * 5. Load frequency from EEPROM
 * 6. Set initial increment display
 * 7. Initialize SCPI subsystem
 * 
 * After setup(), the system is ready and the main loop() begins.
 */
void setup() {
    // Configure input pins with internal pull-up resistors
    // This eliminates need for external pull-up resistors
    pinMode(PIN_BUTTON_INCREMENT, INPUT_PULLUP);  // Increment button with internal pull-up
    pinMode(PIN_BUTTON_IF_OFFSET, INPUT_PULLUP);  // IF offset button with internal pull-up
    pinMode(PIN_ENCODER_BUTTON, INPUT_PULLUP);    // Encoder push button with internal pull-up (added for separate button)
    
    // Initialize LCD display
    UI_LCD_Init();  // Set up LCD and show initial values
    
    // Configure Pin Change Interrupt for rotary encoder (pins D2, D3)
    // PCICR: Pin Change Interrupt Control Register
    PCICR |= (1 << PCIE2);  // Enable Pin Change Interrupt for Port D (pins 0-7)
    
    // PCMSK2: Pin Change Mask Register for Port D
    // Set bits for specific pins we want to monitor
    PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);  // Enable for pins D2 (PCINT18) and D3 (PCINT19)
    
    sei();  // Enable global interrupts - now encoder will work
    
    // Initialize AD9850 hardware
    HAL_AD9850_Init();  // Set up AD9850 control pins and reset sequence
    
    // Load frequency from memory
    Memory_LoadFrequency();  // Read last used frequency from EEPROM or use default
    
    // Set initial increment display
    UI_LCD_UpdateIncrement();  // Show tuning step on LCD
    
    // Initialize SCPI subsystem
    SCPI_Init();  // Set up serial and SCPI command processing
}

/**
 * Arduino main loop - runs continuously
 * 
 * Main program tasks:
 * 1. Monitor for frequency changes (from interrupt)
 * 2. Update LCD when frequency changes
 * 3. Send new frequency to AD9850
 * 4. Process button inputs
 * 5. Handle automatic EEPROM storage
 * 6. Process SCPI commands from serial
 * 
 * This loop runs constantly, but most operations are conditional
 * to save processing power and prevent unnecessary updates.
 */
void loop() {
    // Check for encoder rotation and update frequency
    int encoderChange = g_rotaryEncoder.get_change();  // Get accumulated encoder steps (positive = CW, negative = CCW)
    if (encoderChange != 0) {
        g_currentFreq += (encoderChange * g_tuningIncrement);  // Apply encoder change to frequency
        
        // Apply frequency limits to protect hardware
        if (g_currentFreq > FREQ_UPPER_LIMIT) {
            g_currentFreq = FREQ_UPPER_LIMIT;
        }
        if (g_currentFreq < FREQ_LOWER_LIMIT) {
            g_currentFreq = FREQ_LOWER_LIMIT;
        }
    }
    
    // Update frequency and display if changed by interrupt
    // This comparison detects encoder rotation or SCPI frequency changes
    if (g_currentFreq != g_lastDisplayedFreq) {  // Frequency has changed
        UI_LCD_UpdateFrequency();               // Update display with new frequency
        HAL_AD9850_SendFrequency(g_currentFreq); // Send new frequency to AD9850
        g_lastDisplayedFreq = g_currentFreq;     // Update tracking variable to match
    }
    
    // Process user input buttons
    Input_ProcessIncrementButton();  // Check for increment button press and cycle step
    Input_ProcessIFButton();          // Check for IF offset button press and toggle
    Input_ProcessEncoderButton();     // Check for encoder push button press and toggle IF offset (added for separate button)
    
    // Automatic frequency storage in EEPROM
    // Saves frequency 2 seconds after last change to:
    // - Prevent excessive EEPROM writes (limited lifespan)
    // - Avoid saving during rapid tuning
    // - Ensure stable frequency is saved
    if (g_memoryStatus == 0) {  // Memory needs update (frequency changed)
        if (g_lastFreqChangeTime + 2000 < millis()) {  // 2 seconds have passed since last change
            Memory_StoreFrequency();  // Save current frequency to EEPROM
        }
    }
    
    // Process SCPI commands from serial
    SCPI_Process();  // Handle any incoming SCPI commands
}

// ======================================================================================================================================
// END OF FIRMWARE
// ======================================================================================================================================