from machine import Pin, I2C
from s2minislave import S2MiniSlave
import time


I2C_ADDRESS = 0x55


i2c = I2C(
    0,
    scl=Pin(22),
    sda=Pin(21),
    freq=100000
)


print("I2C devices:")
print([
    hex(x)
    for x in i2c.scan()
])


s2 = S2MiniSlave(
    i2c,
    I2C_ADDRESS
)


print("Protocol version:",
      s2.get_protocol_version())

print("Slave address:",
      hex(s2.get_slave_address()))

print("PWM channels:",
      s2.get_pwm_channel_count())


# ------------------------------------------------------------
# Configuración
# ------------------------------------------------------------

s2.set_pin_mode(
    1,
    S2MiniSlave.INPUT
)

s2.set_pin_mode(
    2,
    S2MiniSlave.INPUT_PULLUP
)

s2.set_pin_mode(
    3,
    S2MiniSlave.OUTPUT
)

s2.set_pin_mode(
    4,
    S2MiniSlave.PWM
)

s2.set_pin_mode(
    5,
    S2MiniSlave.ADC
)


# ------------------------------------------------------------
# Salida digital
# ------------------------------------------------------------

s2.write_output(3, True)


# ------------------------------------------------------------
# PWM
# ------------------------------------------------------------

s2.write_pwm(
    4,
    32768
)


# ------------------------------------------------------------
# DAC
# ------------------------------------------------------------

s2.write_dac1(32768)
s2.write_dac2(16384)


# ------------------------------------------------------------
# Bucle de prueba
# ------------------------------------------------------------

while True:

    inputs = s2.read_inputs()

    adc = s2.read_adc(5)

    print(
        "Inputs=0x{:04X}  ADC5={}".format(
            inputs,
            adc
        )
    )

    time.sleep_ms(500)
