# Zelda OoT: contraste en PS4 original/Slim

Protocolo para **R2N64 v0.2.4** y **The Legend of Zelda: Ocarina of Time (U) V1.2**.
El banco `build/ps4-parity-linux` usa el núcleo y opciones actuales en Linux Release.
Sirve para contrastar escenas, resultados y perfiles; sus tiempos no predicen la velocidad de PS4.

## Preparación

1. Comprueba que el XMB muestre `v0.2.4` y utiliza el mismo archivo de Zelda del banco PC; contrasta su SHA-256 con el informe `matrix.json`.
2. Conserva los guardados de `/data/R2N64/saves`: no los borres ni los sustituyas para esta prueba.
3. Anota si ya había partida guardada. El banco puede empezar con datos nuevos; esa diferencia puede cambiar el arranque y las escenas.
4. Conserva una copia del log anterior si necesitas separar intentos. El archivo `/data/R2N64/logs/r2n64.log` acumula sesiones.

## Primera pasada

1. En **Ajustes → CPU de Nintendo 64**, selecciona **Modo: automático**.
2. En **Ajustes → Rendimiento**, selecciona **Renderizado: 4 hilos**.
3. En **Ajustes → Procesamiento de audio**, selecciona **Modo: acelerado (HLE)**.
4. En **Ajustes → Medir rendimiento**, selecciona **Medición: activada**. Los cambios se aplican al iniciar la próxima ROM y duran esta sesión de la aplicación.
5. Abre Zelda desde **Biblioteca** y suelta los controles. No pulses Start, botones ni sticks: deja avanzar la introducción hasta el título o demostración.
6. El escenario PC `intro` también mantiene entrada neutra. El banco de **6000 VI** representa aproximadamente **100 segundos nominales** para este juego NTSC; si la emulación va lenta, necesita más de 100 segundos reales.
7. Pulsa **OPTIONS** en una escena reconocible y fotografía la pausa completa, incluyendo la imagen del juego. El contador `VI` del perfil permite situar aproximadamente el avance; no hay reproducción automática de la misma traza en consola.
8. Comprueba la **CPU efectiva**: `Recompilador x64` o `Interprete cacheado`. Elegir automático no garantiza que la consola permita JIT. Comprueba también `RDP: 4 hilos` y el contador `Audio HLE: … tareas aceleradas`.
9. Registra porcentaje, VI/s, núcleo, presentación y tiempos del perfil. Anota si hay música, cortes, distorsión o defectos gráficos.
10. Selecciona **Volver a la biblioteca** para cerrar la sesión y conservar su resumen en el log. El juego puede actualizar su guardado normalmente.

## Contrastes del banco

Reinicia la ROM para cada fila y vuelve a fotografiar **la misma escena**. Mantén **Medir rendimiento** activado cuando compares componentes con una pasada PC instrumentada.

| Perfil PC | CPU de Nintendo 64 | Procesamiento de audio | Rendimiento |
|---|---|---|---|
| `auto-hle-4` | Automático | Acelerado (HLE) | 4 hilos |
| `cached-hle-4` | Intérprete | Acelerado (HLE) | 4 hilos |
| `auto-lle-4` | Automático | Original (LLE) | 4 hilos |
| `cached-lle-4` | Intérprete | Original (LLE) | 4 hilos |
| `auto-hle-1` | Automático | Acelerado (HLE) | 1 hilo |

El modo efectivo indicado en pausa decide qué CPU estás comparando. En LLE debe aparecer `Audio: RSP original (LLE)`.
Como control del coste de instrumentación, repite `auto-hle-4` con **Medir rendimiento → Medición: desactivada**, reiniciando la ROM y observando la misma escena.

## Evidencia y límites

Conserva las fotos etiquetadas con perfil y escena, y `/data/R2N64/logs/r2n64.log` tras las pasadas. Si hay cierre, conserva también `/data/R2N64/startup.log` o `/download0/R2N64-startup.log`.
Los tiempos por componente son promedios acumulados desde el arranque; el indicador de velocidad de la pausa muestra el último intervalo actualizado. No mezcles esos períodos al comparar.
VI/s mide intervalos de vídeo, no FPS internos del juego. PC y consola difieren en CPU, sistema, permisos JIT, presentación y salida de audio; ni igual tiempo real ni igual número de VI garantizan la misma escena con distintos guardados.
