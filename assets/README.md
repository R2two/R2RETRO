# Recursos de R2RETRO

## Logo de la aplicación

`logo.png` conserva íntegro el adjunto «Logotipo neón retro R2RETRO.png»
aportado por el usuario el 5 de octubre de 2026 (1254 × 1254 píxeles).
SHA-256: `4b5aa1fa95cb68f0643534cbd09ede7d93dedea7a0a829fd15d349a2871015b1`.

Desde v0.4.2, `pkg/icon0.png` utiliza este logo en un PNG RGB de 512 × 512:
la imagen completa ocupa 512 × 512 píxeles,
sin recortar ni deformar el diseño. Se puede regenerar desde PowerShell en
Windows con `./scripts/make-icon.ps1`. El PNG generado se conserva en el
repositorio; los builds Linux/PS4 no necesitan ejecutar ese conversor.
El empaquetador comprueba que el icono extraído del PKG coincida byte a byte.
Este es el icono de la aplicación; el encabezado del XMB sigue siendo texto SDL.

## Fondo XMB

`background.jpg` es una copia íntegra del adjunto aportado por el usuario el 5 de octubre de 2026:

`C:/Users/R2A/Downloads/Gemini_Generated_Image_slicg7slicg7slic.jpg`

SHA-256: `da2d600acd123b606bc4f9ef232ebece78a9fe2e2b27529e5af6b24253e2abbf`.

La aplicación ajusta el encuadre manteniendo la proporción y aplica contraste al renderizar; no modifica el original. El empaquetador compara el JPG extraído del PKG con este archivo.

Los iconos XMB se dibujan mediante primitivas SDL2 en `src/ui.cpp`. La fuente conserva su licencia en `fonts/LICENSE.txt`. La licencia del código no atribuye autoría ni relicencia las imágenes aportadas por el usuario.

## Diagnóstico N64 original

`diagnostic.z64` se genera desde `scripts/make_diagnostic_rom.py` durante la compilación y se incluye desde v0.2.0. Puede regenerarse con:

```bash
python3 scripts/make_diagnostic_rom.py assets/diagnostic.z64
```

Es un programa MIPS original de 4096 bytes que se ejecuta desde IPL3/DMEM bajo el arranque HLE del núcleo. No incorpora bootcode de Nintendo ni contenido de juegos, y no se presenta como una ROM para arrancar en una N64 física.

Desde **Acerca de → Prueba Nintendo 64**, dibuja bandas RGB, genera un tono estéreo de aproximadamente 500 Hz mediante AI y consulta el mando mediante SI/PIF. Un marcador de 16 × 16 píxeles en (16,16) pasa de negro a blanco al detectar botones o movimiento de stick.

La firma `R2N6`, el contador y la respuesta Joybus se escriben respectivamente en RDRAM `0x400`, `0x404` y `0x408`. El código y los detalles de la señal permiten verificar CPU, video, sonido y entrada sin distribuir una ROM comercial. Evidencia y límites: [CORE-LAB.md](../docs/CORE-LAB.md).
