from machine import Pin
import time

pines = [21, 34, 36, 37, 38, 39, 40]

# Sin pull_up
# entradas = [Pin(pin, Pin.IN) for pin in pines]
# Configurar entradas con pull-up interno
entradas = [Pin(pin, Pin.IN, Pin.PULL_UP) for pin in pines]

while True:
    valores = [entrada.value() for entrada in entradas]

    print(
        "GPIO21:", valores[0],
        "GPIO34:", valores[1],
        "GPIO36:", valores[2],
        "GPIO37:", valores[3],
        "GPIO38:", valores[4],
        "GPIO39:", valores[5],
        "GPIO40:", valores[6]
    )

    time.sleep(1)