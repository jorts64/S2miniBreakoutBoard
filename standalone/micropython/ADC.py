from machine import ADC, Pin
import time

# Crear los ADC 1 a 16
adcs = [ADC(Pin(pin)) for pin in range(1, 17)]

while True:
    valores = []

    for adc in adcs:
        valores.append(adc.read_u16())

    print(valores)

    time.sleep(1)