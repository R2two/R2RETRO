# Hoja de ruta de R2RETRO

Estado editorial: 6 de octubre de 2026. Referencia: v0.5.1. Las prioridades se basan en el código y los registros existentes; no son promesas de fecha o compatibilidad.

## Completado e integrado

- [x] Aplicación nativa PS4 y frontend desktop con navegación XMB.
- [x] N64, GB, GBC, GBA, NES y SNES mediante núcleos libretro fijados.
- [x] Lectura de ROMs internas/USB y guardados separados por sistema.
- [x] Estados cuando el núcleo los soporta, capturas y preferencias por sistema.
- [x] Marcos GB/GBC/GBA/SNES y paletas GB.
- [x] Fichas/carátulas Libretro por HTTPS y caché sin conexión.
- [x] GLideN64 opcional y HLE gráfico conservador con respaldo CXD4.
- [x] PKG v0.5.1 compilado y validado; 49 pruebas locales registradas.

“Completado” describe implementación, no validación universal en hardware.

## Prioridad 1 · Evidencia en PS4

- [ ] Registrar modelo, firmware, GoldHEN y versión exacta del PKG.
- [ ] Comparar Angrylion, GPU + RSP LLE y GPU + HLE en las mismas escenas.
- [ ] Capturar CPU/backend efectivos, contadores HLE/LLE, VI/s y tiempos RSP/SDL.
- [ ] Verificar sonido, mando, pausa, retorno a biblioteca, segunda sesión y persistencia de partidas.
- [ ] Reprobar NES/SMB3 negro, marcos y descarga HTTPS después de los arreglos.

**Cierre:** resultados reproducibles por sistema y versión, logs y capturas; errores pendientes explícitos. La ausencia de mejora percibida en v0.5.1 sigue siendo un dato abierto.

## Prioridad 2 · Rendimiento N64

- [ ] Medir el coste real de RSP y copias de color/profundidad del framebuffer en PS4.
- [ ] Contrastar cambios de configuración sin desactivar globalmente funciones necesarias para compatibilidad.
- [ ] Investigar diferencias visuales HLE/LLE y confirmar fallback ante tareas desconocidas.
- [ ] Evaluar persistencia de selección gráfica con una migración de preferencias segura.

**Cierre:** mejora medida en consola, escenas equivalentes y regresiones aprobadas; no extrapolar cifras de Mesa. Vulkan/ParaLLEl-RDP no es una promesa de esta hoja de ruta: la revisión auditada tiene requisitos incompatibles documentados.

## Backlog de experiencia

- [ ] Favoritos y juegos recientes.
- [ ] Remapeo de controles y configuración por juego.
- [ ] Más mandos y evaluación de vibración.
- [ ] Shaders y rewind, sujetos a memoria/rendimiento.
- [ ] Tabla de compatibilidad con evidencia aportada por usuarios.

El orden se revisará después de resolver rendimiento y regresiones de consola.

## Investigación separada · Pokémon 3D

El laboratorio desktop explora Pokémon Red con SameBoy como autoridad y vista 2D/3D. Cubre mapas concretos; sprites, mapas no cubiertos y batallas tienen limitaciones. No se distribuyen ROMs ni datos derivados, y no está integrado en el PKG PS4. [Estado detallado](VOXEL-FEASIBILITY.md).

## Evidencia

[Estado cronológico](STATUS.md) · [GPU/HLE v0.5.1](GPU-HLE-V051.md) · [NES/SMB3](NES-SMB3-V045.md) · [Red](NETWORK.md) · [Auditoría Vulkan](N64-RETROARCH-VULKAN-AUDIT.md)
