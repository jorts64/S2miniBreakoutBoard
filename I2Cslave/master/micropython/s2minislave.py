"""
S2MiniSlave
===========

Librería MicroPython para controlar una LOLIN S2 Mini
configurada como esclavo I2C mediante el firmware Arduino
S2MiniSlave.

Protocolo:
    Dirección I2C: 7 bits

GPIO controlables:
    1..16

Modos:
    INPUT
    INPUT_PULLUP
    OUTPUT
    PWM
    ADC

Registros de 16 bits:
    MSB primero (big endian)

PWM:
    0     = 0 %
    65535 = 100 %

ADC:
    0..4095

DAC:
    0     = 0 %
    65535 = 100 %
    El esclavo convierte a 8 bits.
"""


class S2MiniSlave:
    # ========================================================
    # Protocolo
    # ========================================================

    REG_VERSION = 0x00
    REG_ADDRESS = 0x01
    REG_PWM_CHANNELS = 0x02

    REG_INPUTS = 0x10
    REG_OUTPUTS = 0x12

    REG_MODE_BASE = 0x20
    REG_PWM_BASE = 0x40
    REG_ADC_BASE = 0x60

    REG_DAC1 = 0x80
    REG_DAC2 = 0x82

    PROTOCOL_VERSION = 1

    # ========================================================
    # Modos de GPIO
    # ========================================================

    INPUT = 0
    INPUT_PULLUP = 1
    OUTPUT = 2
    PWM = 3
    ADC = 4

    # ========================================================
    # Constructor
    # ========================================================

    def __init__(self, i2c, address):
        """
        i2c:
            objeto machine.I2C

        address:
            dirección I2C del esclavo, 0..127
        """

        if address < 0 or address > 127:
            raise ValueError("I2C address must be 0..127")

        self.i2c = i2c
        self.address = address

        self._tx = bytearray(3)

    # ========================================================
    # Acceso a registros
    # ========================================================

    def _write(self, register, data):
        """
        Escribe:

            [REGISTER][DATA...]
        """

        packet = bytearray(1 + len(data))

        packet[0] = register
        packet[1:] = data

        self.i2c.writeto(
            self.address,
            packet
        )


    def _read(self, register, length):
        """
        Selecciona un registro del esclavo y lee 'length' bytes.

        La selección del registro se hace con un write sin STOP,
        seguido de la lectura.
        """

        self.i2c.writeto(
            self.address,
            bytes([register])
        )

        return self.i2c.readfrom(
            self.address,
            length
        )





    # ========================================================
    # Registros de 8 bits
    # ========================================================

    def _write8(self, register, value):
        if value < 0 or value > 255:
            raise ValueError("8-bit value out of range")

        self._write(
            register,
            bytes([value])
        )

    def _read8(self, register):
        return self._read(
            register,
            1
        )[0]

    # ========================================================
    # Registros de 16 bits
    # ========================================================

    def _write16(self, register, value):
        if value < 0 or value > 65535:
            raise ValueError("16-bit value out of range")

        data = bytes([
            (value >> 8) & 0xFF,
            value & 0xFF
        ])

        self._write(
            register,
            data
        )

    def _read16(self, register):
        data = self._read(
            register,
            2
        )

        return (
            (data[0] << 8) |
            data[1]
        )

    # ========================================================
    # Información del esclavo
    # ========================================================

    def get_address(self):
        """
        Devuelve la dirección configurada en el objeto.
        """

        return self.address

    def get_slave_address(self):
        """
        Lee la dirección directamente del esclavo.
        """

        return self._read8(
            self.REG_ADDRESS
        )

    def get_protocol_version(self):
        return self._read8(
            self.REG_VERSION
        )

    def get_pwm_channel_count(self):
        return self._read8(
            self.REG_PWM_CHANNELS
        )

    # ========================================================
    # Configuración GPIO
    # ========================================================

    def set_pin_mode(self, pin, mode):
        """
        Configura GPIO1..GPIO16.

        mode:
            INPUT
            INPUT_PULLUP
            OUTPUT
            PWM
            ADC
        """

        self._check_pin(pin)

        if mode not in (
            self.INPUT,
            self.INPUT_PULLUP,
            self.OUTPUT,
            self.PWM,
            self.ADC
        ):
            raise ValueError("Invalid pin mode")

        self._write8(
            self.REG_MODE_BASE + pin - 1,
            mode
        )

    def get_pin_mode(self, pin):
        self._check_pin(pin)

        return self._read8(
            self.REG_MODE_BASE + pin - 1
        )

    # ========================================================
    # Entradas digitales
    # ========================================================

    def read_inputs(self):
        """
        Lee las 16 entradas digitales.

        bit 0  = GPIO1
        bit 1  = GPIO2
        ...
        bit 15 = GPIO16
        """

        return self._read16(
            self.REG_INPUTS
        )

    def read_input(self, pin):
        """
        Lee individualmente GPIO1..GPIO16.
        """

        self._check_pin(pin)

        value = self.read_inputs()

        return bool(
            value & (1 << (pin - 1))
        )

    # ========================================================
    # Salidas digitales
    # ========================================================

    def read_outputs(self):
        """
        Lee el registro de salidas.
        """

        return self._read16(
            self.REG_OUTPUTS
        )

    def write_outputs(self, value):
        """
        Actualiza las 16 salidas simultáneamente.
        """

        self._write16(
            self.REG_OUTPUTS,
            value
        )

    def write_output(self, pin, value):
        """
        Cambia individualmente una salida.
        """

        self._check_pin(pin)

        outputs = self.read_outputs()

        mask = 1 << (pin - 1)

        if value:
            outputs |= mask
        else:
            outputs &= ~mask

        self.write_outputs(
            outputs
        )

    # ========================================================
    # PWM
    # ========================================================

    def write_pwm(self, pin, value):
        """
        Configura el duty PWM de un GPIO.

        value:
            0     = 0 %
            65535 = 100 %
        """

        self._check_pin(pin)

        self._write16(
            self.REG_PWM_BASE + ((pin - 1) * 2),
            value
        )

    def read_pwm(self, pin):
        self._check_pin(pin)

        return self._read16(
            self.REG_PWM_BASE + ((pin - 1) * 2)
        )

    # ========================================================
    # ADC
    # ========================================================

    def read_adc(self, pin):
        """
        Lee el ADC de GPIO1..GPIO16.

        Resultado:
            0..4095
        """

        self._check_pin(pin)

        return self._read16(
            self.REG_ADC_BASE + ((pin - 1) * 2)
        )

    # ========================================================
    # DAC
    # ========================================================

    def write_dac1(self, value):
        """
        DAC1 / GPIO17.

        0     = 0 %
        65535 = 100 %
        """

        self._write16(
            self.REG_DAC1,
            value
        )

    def write_dac2(self, value):
        """
        DAC2 / GPIO18.
        """

        self._write16(
            self.REG_DAC2,
            value
        )

    def read_dac1(self):
        return self._read16(
            self.REG_DAC1
        )

    def read_dac2(self):
        return self._read16(
            self.REG_DAC2
        )

    # ========================================================
    # Utilidades
    # ========================================================

    @staticmethod
    def _check_pin(pin):
        if pin < 1 or pin > 16:
            raise ValueError(
                "Pin must be GPIO1..GPIO16"
            )