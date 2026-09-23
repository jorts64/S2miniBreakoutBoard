from machine import DAC, Pin
import time

dac1 = DAC(Pin(17))
dac2 = DAC(Pin(18))

for v in [0, 64, 128, 192, 255]:
    dac1.write(v)
    dac2.write(v)
    print("DAC =", v)
    time.sleep(3)