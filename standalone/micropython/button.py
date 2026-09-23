from machine import Pin
import time

led = Pin(15, Pin.OUT)
boton = Pin(0, Pin.IN, Pin.PULL_UP)

while True:
    if boton.value() == 0:
        led.value(1)
    else:
        led.value(0)

    time.sleep_ms(10)