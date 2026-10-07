# Colorización GB opcional — fuentes sin compilar

> Actualización v0.5.6: cambios compilados y pruebas completadas con autorización.
> [Resultados, paquete y límites](releases/v0.5.6.md). Las menciones a fuentes
> sin compilar que siguen registran el estado anterior a esta autorización.

Solicitud del 7 de octubre de 2026: aspecto de color para Game Boy, activable y
desactivable. La implementación añade **Color GB: activado (4 tonos) / desactivado**
a la pausa de juegos GB (L3+R3). Se aplica al continuar, sin reiniciar el juego.
La selección queda en `configs/systems/gb.json`, campo opcional booleano
`gbColor`, falso por defecto y al leer preferencias anteriores.

El modo utiliza una paleta artística: azul muy oscuro, verde, salmón y crema.
Se aplica sobre los índices DMG mediante SameBoy, sin analizar los píxeles por
cuadro ni añadir otra pasada GPU. Al desactivar recupera Gris/Verde/Oliva/Turquesa
según la selección previa. Mientras está activo, la fila de paleta permite
elegir qué paleta recuperar al desactivarlo y explica ese comportamiento.

Se puede combinar con los shaders ya implementados **LCD** y **CRT**, el marco,
el escalado y el suavizado. El selector de shader mantiene su control separado;
colorizar no activa un shader ni depende de que GLES2 compile uno correctamente.
Se mantienen los cuatro tonos del juego: no es una edición GBC, no equivale a
las paletas de compatibilidad por juego de un GBC real y no distingue colores
independientes de personaje, césped o edificios cuando comparten índice DMG.
No afecta GBC/GBA/NES/SNES/N64 ni el laboratorio Pokémon 3D separado.

## Implementación

- Índice interno de paleta 4 (`r2retro_color`), mientras `gbPalette` persistente
  sigue admitiendo 0–3 para preservar el aspecto que se restaura al apagar color.
- `scripts/build-handheld-cores.sh` aplica un reemplazo acotado e idempotente
  al adaptador de la copia SameBoy, sin modificar `external/sameboy`.
- `GB_palette_t` estático de cinco entradas: cuatro niveles y LCD apagado.
  Se usa la API existente `GB_set_palette`, sin alterar RAM ni modelo emulado.
- Cambio de firma del build para reconstruir cuando se autorice, y registro del
  parche en provenance/avisos de fuentes. Sin dependencias o BIOS nuevas.
- La interfaz reutiliza el cambio de paleta en vivo. Las preferencias sólo se
  actualizan tras aceptar el cambio; si falla su persistencia, se informa que
  el ajuste sólo dura la sesión.

## Validación preparada y límites

Fuentes de pruebas ampliadas para comprobar: migración de preferencias sin el
campo, lectura/escritura y restauración de la paleta anterior, rechazo de tipos
inválidos/duplicados, cinco paletas diferenciadas, color inicial, carga de estado
sin reactivar color apagado y alternancia con estado del diagnóstico/PCM iguales
a una sesión de control. Tras apagar se comparan también los píxeles originales.

Estas pruebas **no se compilaron ni ejecutaron**. Sólo se revisaron diferencias,
sintaxis de scripts y aplicación/idempotencia del parche en memoria. No se
generó ni instaló PKG nuevo. La v0.5.5 instalada tiene LCD/CRT y paletas clásicas,
pero no este nuevo interruptor. Pendientes compilación autorizada, regresiones
desktop, presentación GLES2 con marco/shader y prueba visual de Pokémon Red y
otros GB en PS4. No se afirma mejora de rendimiento medida.
