# R2RETRO v0.5.0 — GPU experimental y comprobación de marcos

## Motivo y alcance

El usuario confirmó que v0.4.5 indica «Marco: activado» en GBA pero no lo
ve. También indicó que N64 mantiene aproximadamente la mitad de velocidad
en PS4 original/Slim. La versión anterior seguía usando Angrylion en CPU:
presentar su imagen mediante SDL/GLES2 no trasladaba el RDP a la GPU.

Esta versión conecta GLideN64 GLES2 al frontend como opción experimental.
No presupone compatibilidad de Piglet, velocidad normal ni éxito en consola.
Angrylion sigue seleccionado al iniciar la aplicación.

## Cambios

- Ajustes → Gráficos de Nintendo 64 → GLideN64 GPU. Se aplica al abrir la
  próxima ROM y dura la sesión de la aplicación. La pausa muestra el backend
  efectivo, incluido un regreso a Angrylion si falla la inicialización GPU.
- Mismo contexto GLES2 de SDL, textura de salida y FBO reutilizables. El
  perfil pide 320×240 y dispone de un destino acotado a 640×480. Mantiene
  4:3, recompilador x64 comprobado, CXD4 y audio HLE reconocido existente.
  No incorpora HLE gráfico ni un nuevo planificador de hilos.
- Negociación libretro HW, reset/destrucción del contexto, frames hardware y
  estado GL preservado en ambos sentidos entre SDL y núcleo. El frontend
  no descarga ni vuelve a subir el framebuffer por cuadro. GLideN64 puede
  hacer sus propias copias GPU/RDRAM para emular efectos del juego; no se
  desactivan indiscriminadamente esas opciones de compatibilidad.
- La comprobación GLSL/FBO se hace al iniciar explícitamente una sesión GPU,
  nunca al arrancar la aplicación. Usa un píxel de diagnóstico. Si el contexto
  se pierde durante la sesión, no se ejecutan destructores GL en otro contexto
  ni se reutiliza un núcleo incompletamente cerrado: se requiere reiniciar.
- Parche 0007: negociación antes de reservar recursos de sesión, índices GL
  opacos y ubicaciones de uniforms comprobados, caché de arrays correcta,
  y ruta EGL image habilitada únicamente donde existe su implementación
  (Android). Esa última ruta producía errores de framebuffer en GLES2 desktop.
- Tras cargar N64 se libera la copia de ROM del frontend: el núcleo ya posee
  otra copia. Ahorra el tamaño del cartucho en RAM; no es una ganancia de FPS
  demostrada. Otros núcleos conservan su memoria prestada.
- Los marcos usan una textura RGB888 opaca con subida explícita comprobada.
  SDL_CreateTextureFromSurface del port puede ignorar el fallo de subida;
  recibir una textura no basta para afirmar que se cargaron sus píxeles.
  Se restablecen target, viewport y clip para la composición.
- Al seleccionar un marco se comprueban cuatro muestras exteriores una vez,
  después de dibujarlo. Pausa y log distinguen composición enviada, muestras
  visibles, discrepancia y lectura no disponible. Una lectura no disponible
  no desactiva un marco válido. No se decodifica/sube/comprueba por cuadro.

Originales de marcos, logo, datos, identificadores, controles y preferencias
`overlay:false` se conservan. No se incluye ninguna ROM comercial.

## Prueba física pendiente

1. Instalar el PKG v0.5.0 y abrir GBA. L3+R3 → Marco permite apagar/encender.
   Fotografiar el marco y el diagnóstico de muestras en la pausa si sigue
   sin aparecer; el log queda bajo la raíz de datos configurada.
2. Para N64, activar «Gráficos de Nintendo 64: GLideN64 GPU» antes de abrir
   Mario. Comprobar que la pausa indica realmente GLideN64 GPU. Si muestra
   Angrylion, conservar el motivo de respaldo en el log.
3. Comparar la misma escena durante al menos 30 segundos, con idénticos
   CPU, audio y perfil de medición. Registrar porcentaje, VI/s, núcleo y
   presentación. VI/s no equivale a FPS internos del juego.
4. Comprobar audio, pausa, reinicio y regreso a la biblioteca. Los estados
   rápidos quedan deshabilitados en GPU en este primer hito; SRAM permanece.

## Validación

49/49 CTest aprobados en 299,53 segundos (`build/v050-ctest.log`). Incluyen
el puente HW con núcleo simulado, el host GLES2 y la integración real de
puente/host/GLideN64/CXD4: tres sesiones de 40 VI, reinicio, bandas RGB visibles
e idénticas, retorno a SDL y transición a Angrylion. Los diagnósticos son
originales, no cartuchos comerciales. Se conservaron las regresiones de
núcleos, almacenamiento, audio, red, biblioteca y frontend de otros sistemas.

Controles adicionales: `build/gpu-core-work/final-check.log` contiene la
prueba estricta del núcleo GLES2 (90 VI GPU, 24 VI software), cero errores GL
y prueba GLSM ASan/UBSan. `build/gpu-session-v046/` conserva las pruebas del
host y sintaxis PS4. `build/overlay-v046-audit/results.json` conserva los
cuatro recorridos de marcos originales/sintéticos en software/GLES2.

La primera ejecución XMB excedió el límite antiguo de 20/30 segundos al
generar PNG; la ejecución directa completó correctamente en 40,73 segundos.
El límite se amplió a 90 segundos y ambos recorridos aprobaron en la suite
final. El test nuevo del puente requería crear su directorio de logs; se
corrigió esa preparación antes de la ejecución final.

PKG compilado, extraído y validado: `dist/R2RETRO-v0.5.0-gpu-preview.pkg`,
64.094.208 bytes, SFO 00.50, SHA-256:
`0ea552579f5acbdc2fd8636db4f33dc9aae1fb77228172cc7da441e74788cb15`.
Arte, CA, identidad y payload fueron comprobados durante extracción.
v0.4.5 conserva su SHA-256
`446bd9ea738f191523834dacf0eae8745d12fb5cdce4c7b5d3516ab212f40be2`.
Logs: `build/v050-package-build.log`, `build/v050-final-ps4-build.log` y
`build/v050-final-package.log`; metadatos `dist/build-info-v0.5.0.json`.

Pruebas optativas con archivos autorizados del usuario, leídos desde Downloads:

| Archivo | Recorrido local | Resultado |
| --- | --- | --- |
| Super Mario 64 USA | 2×1.200 VI, x64 + CXD4/audio HLE + GLideN64 GLES2 | Salida hardware 320×240 real, sin respaldo a software, imagen de la pantalla de Mario, PCM, captura y retorno a XMB |
| Pokémon FireRed Rev1 | 2×1.200 cuadros, mGBA + marco GBA | Imagen de la introducción, PCM, avance rápido, captura y marco con 4/4 muestras visibles |

Ambas sesiones terminaron normalmente y los SHA-256 antes/después de las ROMs
coinciden: Mario `17ce077343c6133f8c9f2d6d6d9a4ab62c8cd2aa57c40aea1f490b4c8bb21d91`;
FireRed `729041b940afe031302d630fdbe57c0c145f3f7b6d9b8eca5e98678d0ca4d059`.
El log de Mario confirma liberar 8.388.608 bytes de copia del frontend.
No es una prueba de completar niveles ni de precisión visual universal.
Evidencia: `build/v050-rom-smoke/results.json`, logs y capturas de cada caso;
reproducción local `python3 build/v050-rom-smoke.py`. Guardados aislados bajo
ese directorio; no se copiaron las ROMs ni se incluyeron en el PKG.

Mesa software no representa el rendimiento de PS4.
La comprobación de estado GL entre SDL y núcleo tiene un coste que necesita
medición física. No se promete que todos los shaders o juegos funcionen en
Piglet por el hecho de compilar con OpenOrbis.
