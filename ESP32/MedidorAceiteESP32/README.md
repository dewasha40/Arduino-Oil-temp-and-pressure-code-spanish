# Medidor HELLA · ESP32-WROOM + GC9B72

Firmware ESP-IDF en español que porta la interfaz del medidor HELLA a la pantalla TFT redonda GC9B72 de 360×360 por SPI. Conserva las fórmulas del sketch Arduino para temperatura y presión, añade lecturas del canal auxiliar y medición de batería. Proyecto independiente; el sketch Arduino original permanece intacto.

![Captura del simulador del medidor clásico](images/preview-simulador.png)

## Compilar

Requiere ESP-IDF 5.5 o posterior, componente LVGL 9.5 e IDF target `esp32`:

```powershell
idf.py build
```

El binario se genera en `build/medidor_hella_esp32_gc9b72.bin`.

El directorio `firmware` contiene también `MedidorAceiteESP32-flash.bin`, imagen combinada lista para grabar desde la dirección `0x0` en un ESP32-WROOM con flash de 4 MB.

## Pinout

| Señal | WROOM |
|---|---:|
| SCLK / MOSI | GPIO18 / GPIO23 |
| CS / DC / RESET | GPIO13 / GPIO14 / GPIO27 |
| Retroiluminación | GPIO26 |
| HELLA PWM acondicionado | GPIO34 |
| Medida de batería (ADC1) | GPIO35 |

Las fórmulas portadas son `((4096/periodo_bajo)*pulso_alto-128)/19.2-40` para temperatura y `(((4096/periodo_bajo)*pulso_alto-128)/384)+0.5-0.88` para presión en bar. La correspondencia exacta de los tres pulsos debe confirmarse con osciloscopio y el sensor concreto; aún no se ha validado en banco.

## Acondicionamiento y seguridad eléctrica

**No conectes la salida del sensor ni la batería del coche directamente a GPIO34/GPIO35.** GPIO34 solo admite entrada y no dispone de pull-up/down internos. La señal HELLA debe pasar por una etapa de adaptación/protección que entregue pulsos de 0–3,3 V; su diseño depende de medir la salida real del sensor. Para GPIO35 se conserva el divisor base del sketch (27 kΩ / 4,7 kΩ), pero la entrada ADC requiere protección automotriz y calibración con multímetro: la lectura de batería es aproximada hasta calibrarla. Usa una fuente automotriz protegida para alimentar la placa.

La simulación de PC utiliza telemetría ficticia: no valida el cableado, el sensor, la precisión ADC ni una prueba física.
