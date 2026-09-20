from machine import Pin, I2C
from s2minislave import S2MiniSlave

# ------------------------------------------------------------
# I2C maestro
# ------------------------------------------------------------

i2c = I2C(
    0,
    scl=Pin(22),
    sda=Pin(21),
    freq=100000
)

# ------------------------------------------------------------
# Esclavo
#
# Ejemplo: dirección 0x55
# ------------------------------------------------------------

s2 = S2MiniSlave(
    i2c,
    0x55
)

# ------------------------------------------------------------
# Comprobar comunicación
# ------------------------------------------------------------

print("Protocol:",
      s2.get_protocol_version())

print("Address:",
      hex(s2.get_slave_address()))

print("PWM channels:",
      s2.get_pwm_channel_count())
      




# Configurar GPIO

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



#  Entradas digitales

inputs = s2.read_inputs()

print("Inputs:", hex(inputs))

if s2.read_input(1):
    print("GPIO1 HIGH")

if s2.read_input(8):
    print("GPIO8 HIGH")
    
    

# Salidas

# Todas las salidas
s2.write_outputs(0xAAAA)

# GPIO3 HIGH
s2.write_output(3, True)

# GPIO3 LOW
s2.write_output(3, False)



# PWM

# GPIO4 al 25 %
s2.write_pwm(
    4,
    16384
)

# 50 %
s2.write_pwm(
    4,
    32768
)

# 100 %
s2.write_pwm(
    4,
    65535
)



# ADC

value = s2.read_adc(5)

print("ADC5:", value)





# DAC

# DAC1 ~50 %
s2.write_dac1(32768)

# DAC2 ~25 %
s2.write_dac2(16384)










