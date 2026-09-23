from machine import DAC, Pin

# Voltaje deseado
voltaje = 1.00

# Referencia del DAC
vref = 3.3

# DAC1 = GPIO17
dac1 = DAC(Pin(17))

# Calcular valor de 8 bits
valor_dac = round(voltaje / vref * 255)

# Limitar entre 0 y 255
valor_dac = max(0, min(255, valor_dac))

# Escribir en el DAC
dac1.write(valor_dac)

print("Voltaje solicitado:", voltaje, "V")
print("Valor DAC:", valor_dac)