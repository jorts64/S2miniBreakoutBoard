from machine import Pin
import time

leds = [Pin(pin, Pin.OUT) for pin in range(1, 17)]

while True:
    for led in leds:
        led.on()
        time.sleep_ms(200)
        led.off()