from machine import Pin
import time

led = Pin(21, Pin.OUT)

while True:
    led.value(not led.value())
    time.sleep(0.5)