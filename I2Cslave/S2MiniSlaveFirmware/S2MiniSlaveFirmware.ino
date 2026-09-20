#include <Arduino.h>
#include <Wire.h>

// ============================================================
// S2 Mini I2C Slave
// ============================================================
//
// ESP32-S2 / LOLIN S2 Mini
//
// I2C:
//   SDA = GPIO33
//   SCL = GPIO35
//
// Address:
//   GPIO21 = bit 6 (MSB)
//   GPIO34 = bit 5
//   GPIO36 = bit 4
//   GPIO37 = bit 3
//   GPIO38 = bit 2
//   GPIO39 = bit 1
//   GPIO40 = bit 0 (LSB)
//
// GPIO 1..16:
//   INPUT
//   INPUT_PULLUP
//   OUTPUT
//   PWM
//   ADC
//
// DAC:
//   DAC1 = GPIO17
//   DAC2 = GPIO18
//
// ============================================================


// ------------------------------------------------------------
// I2C
// ------------------------------------------------------------

#define I2C_SDA_PIN       33
#define I2C_SCL_PIN       35

// ------------------------------------------------------------
// Address configuration pins
// ------------------------------------------------------------

#define ADDR_BIT6_PIN     21
#define ADDR_BIT5_PIN     34
#define ADDR_BIT4_PIN     36
#define ADDR_BIT3_PIN     37
#define ADDR_BIT2_PIN     38
#define ADDR_BIT1_PIN     39
#define ADDR_BIT0_PIN     40

// ------------------------------------------------------------
// Controlled GPIOs
// ------------------------------------------------------------

#define FIRST_IO          1
#define LAST_IO           16
#define NUM_IO            16

// ------------------------------------------------------------
// DAC
// ------------------------------------------------------------

#define DAC1_PIN          17
#define DAC2_PIN          18

// ------------------------------------------------------------
// PWM
//
// ESP32-S2 has 8 LEDC channels.
// 12-bit PWM is used internally.
//
// The register exposed to the master is 16-bit:
//   0     = 0%
//   65535 = 100%
//
// It is converted to the 12-bit hardware range.
// ------------------------------------------------------------

#define PWM_FREQUENCY     5000
#define PWM_RESOLUTION    12
#define PWM_MAX           ((1UL << PWM_RESOLUTION) - 1)

// ESP32-S2 dispone de 8 canales LEDC
#define MAX_PWM_CHANNELS  8

// Número de GPIO que actualmente utilizan PWM
uint8_t activePWMChannels = 0;

// Estado de cada GPIO
bool pwmAttached[NUM_IO];

// ------------------------------------------------------------
// Protocol
// ------------------------------------------------------------

#define PROTOCOL_VERSION  1

#define REG_VERSION       0x00
#define REG_ADDRESS       0x01
#define REG_PWM_CHANNELS  0x02

#define REG_INPUTS        0x10
#define REG_OUTPUTS       0x12

#define REG_MODE_BASE     0x20
#define REG_PWM_BASE      0x40
#define REG_ADC_BASE      0x60

#define REG_DAC1          0x80
#define REG_DAC2          0x82


// ------------------------------------------------------------
// Pin modes
// ------------------------------------------------------------

enum PinModeS2 : uint8_t
{
  MODE_INPUT = 0,
  MODE_INPUT_PULLUP = 1,
  MODE_OUTPUT = 2,
  MODE_PWM = 3,
  MODE_ADC = 4
};


// ------------------------------------------------------------
// Global state
// ------------------------------------------------------------

// Configuration of GPIO1..16
volatile uint8_t pinModes[NUM_IO];

// Digital input register
volatile uint16_t inputRegister = 0;

// Digital output register
volatile uint16_t outputRegister = 0;

// PWM registers
volatile uint16_t pwmRegisters[NUM_IO];

// ADC registers
volatile uint16_t adcRegisters[NUM_IO];

// DAC registers
volatile uint16_t dac1Register = 0;
volatile uint16_t dac2Register = 0;

// I2C register pointer
volatile uint8_t registerPointer = 0;


// ------------------------------------------------------------
// Synchronization
// ------------------------------------------------------------

portMUX_TYPE dataMux = portMUX_INITIALIZER_UNLOCKED;


// ------------------------------------------------------------
// Calculate I2C address
// ------------------------------------------------------------

uint8_t readI2CAddress()
{
  uint8_t address = 0;

  if (digitalRead(ADDR_BIT6_PIN) == LOW)
    address |= 0x40;

  if (digitalRead(ADDR_BIT5_PIN) == LOW)
    address |= 0x20;

  if (digitalRead(ADDR_BIT4_PIN) == LOW)
    address |= 0x10;

  if (digitalRead(ADDR_BIT3_PIN) == LOW)
    address |= 0x08;

  if (digitalRead(ADDR_BIT2_PIN) == LOW)
    address |= 0x04;

  if (digitalRead(ADDR_BIT1_PIN) == LOW)
    address |= 0x02;

  if (digitalRead(ADDR_BIT0_PIN) == LOW)
    address |= 0x01;

  return address;
}


// ------------------------------------------------------------
// Read 16-bit value from register
// ------------------------------------------------------------

uint16_t getRegister16(uint8_t reg)
{
  switch (reg)
  {
    case REG_INPUTS:
      return inputRegister;

    case REG_OUTPUTS:
      return outputRegister;

    case REG_DAC1:
      return dac1Register;

    case REG_DAC2:
      return dac2Register;

    default:
      break;
  }

  // PWM registers
  if (reg >= REG_PWM_BASE &&
      reg < REG_PWM_BASE + NUM_IO * 2)
  {
    uint8_t index = (reg - REG_PWM_BASE) / 2;
    return pwmRegisters[index];
  }

  // ADC registers
  if (reg >= REG_ADC_BASE &&
      reg < REG_ADC_BASE + NUM_IO * 2)
  {
    uint8_t index = (reg - REG_ADC_BASE) / 2;
    return adcRegisters[index];
  }

  return 0;
}


// ------------------------------------------------------------
// Write 16-bit value to register
// ------------------------------------------------------------

void setRegister16(uint8_t reg, uint16_t value)
{
  switch (reg)
  {
    case REG_OUTPUTS:
      outputRegister = value;
      return;

    case REG_DAC1:
      dac1Register = value;
      return;

    case REG_DAC2:
      dac2Register = value;
      return;

    default:
      break;
  }

  // PWM
  if (reg >= REG_PWM_BASE &&
      reg < REG_PWM_BASE + NUM_IO * 2)
  {
    uint8_t index = (reg - REG_PWM_BASE) / 2;
    pwmRegisters[index] = value;
    return;
  }
}


// ------------------------------------------------------------
// Configure one GPIO
// ------------------------------------------------------------

void configurePin(uint8_t index, uint8_t mode)
{
  if (index >= NUM_IO)
    return;

  uint8_t pin = FIRST_IO + index;

  // ----------------------------------------------------------
  // Si estaba funcionando como PWM y cambia de modo,
  // liberar el canal LEDC.
  // ----------------------------------------------------------

  if (pwmAttached[index] && mode != MODE_PWM)
  {
    ledcDetach(pin);

    pwmAttached[index] = false;

    if (activePWMChannels > 0)
      activePWMChannels--;
  }


  // ----------------------------------------------------------
  // Configuración del nuevo modo
  // ----------------------------------------------------------

  switch (mode)
  {
    case MODE_INPUT:

      pinMode(pin, INPUT);

      break;


    case MODE_INPUT_PULLUP:

      pinMode(pin, INPUT_PULLUP);

      break;


    case MODE_OUTPUT:

      pinMode(pin, OUTPUT);

      break;


    case MODE_PWM:

      // ------------------------------------------------------
      // Comprobar que todavía queda un canal LEDC disponible.
      // ------------------------------------------------------

      if (!pwmAttached[index])
      {
        if (activePWMChannels >= MAX_PWM_CHANNELS)
        {
          // No quedan canales PWM.
          //
          // No cambiamos el modo del pin.
          // Lo dejamos como INPUT para tener un estado
          // conocido.

          pinMode(pin, INPUT);

          pinModes[index] = MODE_INPUT;

          return;
        }


        // ----------------------------------------------------
        // Asignar automáticamente un canal LEDC al GPIO.
        // ----------------------------------------------------

        if (ledcAttach(
              pin,
              PWM_FREQUENCY,
              PWM_RESOLUTION))
        {
          pwmAttached[index] = true;

          activePWMChannels++;
        }
        else
        {
          // Error al asignar LEDC.

          pinMode(pin, INPUT);

          pinModes[index] = MODE_INPUT;

          return;
        }
      }

      break;


    case MODE_ADC:

      // ADC utiliza el GPIO como entrada.
      pinMode(pin, INPUT);

      break;


    default:

      mode = MODE_INPUT;

      pinMode(pin, INPUT);

      break;
  }


  pinModes[index] = mode;
}

// ------------------------------------------------------------
// Update PWM outputs
// ------------------------------------------------------------

void updatePWM()
{
  for (uint8_t i = 0; i < NUM_IO; i++)
  {
    // Sólo actualizamos GPIO configurados como PWM
    // y que realmente tengan un canal LEDC asignado.

    if (pinModes[i] != MODE_PWM)
      continue;

    if (!pwmAttached[i])
      continue;


    uint8_t pin = FIRST_IO + i;


    // --------------------------------------------------------
    // Registro de 16 bits:
    //
    // 0      = 0 %
    // 65535  = 100 %
    //
    // Convertir a la resolución LEDC de 12 bits.
    // --------------------------------------------------------

    uint16_t value =
      pwmRegisters[i];


    uint32_t duty =
      ((uint32_t)value * PWM_MAX) / 65535UL;


    // --------------------------------------------------------
    // IMPORTANTE:
    //
    // ledcWrite() recibe el GPIO, no el número de canal.
    // --------------------------------------------------------

    ledcWrite(
      pin,
      duty
    );
  }
}


// ------------------------------------------------------------
// Update digital outputs
// ------------------------------------------------------------

void updateOutputs()
{
  uint16_t value = outputRegister;

  for (uint8_t i = 0; i < NUM_IO; i++)
  {
    if (pinModes[i] != MODE_OUTPUT)
      continue;

    uint8_t pin = FIRST_IO + i;

    bool state = value & (1U << i);

    digitalWrite(pin, state ? HIGH : LOW);
  }
}


// ------------------------------------------------------------
// Read digital inputs
// ------------------------------------------------------------

void updateInputs()
{
  uint16_t value = 0;

  for (uint8_t i = 0; i < NUM_IO; i++)
  {
    if (pinModes[i] != MODE_INPUT &&
        pinModes[i] != MODE_INPUT_PULLUP)
    {
      continue;
    }

    uint8_t pin = FIRST_IO + i;

    if (digitalRead(pin))
      value |= (1U << i);
  }

  inputRegister = value;
}


// ------------------------------------------------------------
// Read ADC inputs
// ------------------------------------------------------------

void updateADC()
{
  for (uint8_t i = 0; i < NUM_IO; i++)
  {
    if (pinModes[i] != MODE_ADC)
      continue;

    uint8_t pin = FIRST_IO + i;

    int value = analogRead(pin);

    if (value < 0)
      value = 0;

    if (value > 4095)
      value = 4095;

    adcRegisters[i] = (uint16_t)value;
  }
}


// ------------------------------------------------------------
// Update DAC outputs
// ------------------------------------------------------------

void updateDAC()
{
  // ESP32-S2 DAC resolution = 8 bits.
  //
  // Master writes 0..65535.
  // Hardware receives 0..255.

  uint8_t dac1 =
    ((uint32_t)dac1Register * 255UL) / 65535UL;

  uint8_t dac2 =
    ((uint32_t)dac2Register * 255UL) / 65535UL;

  dacWrite(DAC1_PIN, dac1);
  dacWrite(DAC2_PIN, dac2);
}


// ------------------------------------------------------------
// I2C RECEIVE
// ------------------------------------------------------------
//
// Master writes:
//
//   [REGISTER]
//
// or:
//
//   [REGISTER][DATA...]
//
// If only REGISTER is received, it becomes the register
// pointer for a subsequent I2C read.
//
// ------------------------------------------------------------

void onI2CReceive(int count)
{
  if (count <= 0)
    return;

  uint8_t reg = Wire.read();

  registerPointer = reg;

  // No data: only change register pointer
  if (count == 1)
    return;

  // Configuration registers
  if (reg >= REG_MODE_BASE &&
      reg < REG_MODE_BASE + NUM_IO)
  {
    uint8_t mode = Wire.read();

    configurePin(
      reg - REG_MODE_BASE,
      mode
    );

    return;
  }

  // 16-bit registers
  if (count >= 3)
  {
    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();

    uint16_t value =
      ((uint16_t)msb << 8) | lsb;

    setRegister16(reg, value);
  }
}


// ------------------------------------------------------------
// I2C REQUEST
// ------------------------------------------------------------

void onI2CRequest()
{
  uint8_t reg = registerPointer;

  switch (reg)
  {
    case REG_VERSION:

      Wire.write(PROTOCOL_VERSION);
      return;

    case REG_ADDRESS:

      // The address is fixed after boot.
      Wire.write(readI2CAddress());
      return;

    case REG_PWM_CHANNELS:

      Wire.write(8);
      return;

    default:
      break;
  }

  // Configuration registers
  if (reg >= REG_MODE_BASE &&
      reg < REG_MODE_BASE + NUM_IO)
  {
    Wire.write(
      pinModes[reg - REG_MODE_BASE]
    );

    return;
  }

  // 16-bit registers
  uint16_t value = getRegister16(reg);

  Wire.write((uint8_t)(value >> 8));
  Wire.write((uint8_t)(value & 0xFF));
}


// ------------------------------------------------------------
// SETUP
// ------------------------------------------------------------

void setup()
{
  Serial.begin(115200);

  delay(20);

  // ----------------------------------------------------------
  // Address pins
  // ----------------------------------------------------------

  pinMode(ADDR_BIT6_PIN, INPUT_PULLUP);
  pinMode(ADDR_BIT5_PIN, INPUT_PULLUP);
  pinMode(ADDR_BIT4_PIN, INPUT_PULLUP);
  pinMode(ADDR_BIT3_PIN, INPUT_PULLUP);
  pinMode(ADDR_BIT2_PIN, INPUT_PULLUP);
  pinMode(ADDR_BIT1_PIN, INPUT_PULLUP);
  pinMode(ADDR_BIT0_PIN, INPUT_PULLUP);

  // Address is sampled once after reset.
  uint8_t address = readI2CAddress();

  Serial.print("I2C address: 0x");

  if (address < 16)
    Serial.print("0");

  Serial.println(address, HEX);

  // ----------------------------------------------------------
  // Initialize controlled GPIOs
  // ----------------------------------------------------------

  for (uint8_t i = 0; i < NUM_IO; i++)
  {
    pinModes[i] = MODE_INPUT;
    pwmRegisters[i] = 0;
    adcRegisters[i] = 0;

    pinMode(
      FIRST_IO + i,
      INPUT
    );
  }
  
  // ----------------------------------------------------------
  // PWM
  // ----------------------------------------------------------


  for (uint8_t i = 0; i < NUM_IO; i++)
  {
    pinModes[i] = MODE_INPUT;
    pwmRegisters[i] = 0;
    adcRegisters[i] = 0;
    pwmAttached[i] = false;
    pinMode(
      FIRST_IO + i,
      INPUT
    );
  }
  activePWMChannels = 0;
  

  // ----------------------------------------------------------
  // DAC
  // ----------------------------------------------------------

  dac1Register = 0;
  dac2Register = 0;

  dacWrite(DAC1_PIN, 0);
  dacWrite(DAC2_PIN, 0);

  // ----------------------------------------------------------
  // I2C slave
  // ----------------------------------------------------------

  Wire.onReceive(onI2CReceive);
  Wire.onRequest(onI2CRequest);

  if (!Wire.begin(
        address,
        I2C_SDA_PIN,
        I2C_SCL_PIN,
        100000))
  {
    Serial.println("ERROR: I2C slave initialization failed");
    while (true)
      delay(1000);
  }

  Serial.println("S2MiniSlave ready");
}


// ------------------------------------------------------------
// LOOP
// ------------------------------------------------------------

void loop()
{
  // The registers are synchronized with the physical
  // inputs and outputs on every loop iteration.

  updateInputs();
  updateADC();
  updateOutputs();
  updatePWM();
  updateDAC();

  delay(1);
}
