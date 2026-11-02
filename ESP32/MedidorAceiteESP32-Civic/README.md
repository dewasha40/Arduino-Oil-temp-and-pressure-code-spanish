# Medidor HELLA · ESP32-WROOM · estilo Civic

Segundo firmware ESP-IDF independiente para la misma GC9B72 y el mismo sensor HELLA. La captura PWM, las conversiones, el ADC y el controlador de pantalla pertenecen a este medidor; la interfaz adopta una composición deportiva de aro verde/cian. No requiere el concentrador Innovate ni los sensores del proyecto Civic original.

![Captura del simulador del medidor estilo deportivo](images/preview-simulador.png)

## Compilar

Con ESP-IDF 5.5 o posterior y LVGL 9.5:

```powershell
idf.py build
```

El binario se genera en `build/medidor_hella_esp32_estilo_civic.bin`.

El directorio `firmware` contiene también `MedidorAceiteESP32-Civic-flash.bin`, imagen combinada lista para grabar desde la dirección `0x0` en un ESP32-WROOM con flash de 4 MB.

El pinout de la pantalla y las entradas GPIO34 (PWM HELLA ya acondicionado a 3,3 V) y GPIO35 (batería mediante divisor y protección) coinciden con la variante clásica. Consulta su [pinout y guía eléctrica](../MedidorAceiteESP32/README.md#pinout).

Los recursos Civic de referencia siguen identificados mediante [`LICENSE.md`](LICENSE.md) y [`NOTICE`](NOTICE). La demo es referencia visual; su hardware de adquisición no se utiliza. El simulador de PC muestra telemetría ficticia. Valida la adaptación eléctrica y el sensor real antes de conectar el coche.
