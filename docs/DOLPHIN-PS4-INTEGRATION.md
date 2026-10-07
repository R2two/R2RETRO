# GameCube / Wii: auditoría de integración

**Aplazado por el usuario:** prioridad actual N64. Conservar esta auditoría como
referencia; no continuar instalando dependencias ni integrando GameCube/Wii.
Sí se permite estudiar sus técnicas de plataforma para mejorar N64.

## Aportes potenciales a N64

La revisión local identifica tres candidatos, todavía sin mejora física medida:

- RADV/GNM (`port/mesa/ac_ps4_drm.c`): memoria directa, separación garlic/onion,
  colas y fences. Podría servir a un backend Vulkan N64, sujeto a comprobar las
  capacidades compute/almacenamiento de ParaLLEl-RDP y desarrollar presentación,
  sincronización y host libretro. No conecta directamente con GLideN64 GLES2.
- Perfilado (`port/ps4_sampler.cpp`): muestreo por hilo, direcciones JIT y llamadas
  a bibliotecas. La idea sirve para localizar el coste físico; su manejador de
  señales/ABI y offsets no deben copiarse sin validar. Empezar con el perfil por
  componentes y frontera SDL/núcleo ya incluido en R2RETRO.
- Reutilización y cachés: reducir asignaciones y sincronizaciones, y estudiar
  caché persistente de shaders. R2RETRO ya conserva FBO/textura y no lee el
  framebuffer por cuadro en su presentador. Su opción GLideN64
  `EnableShadersStorage` está desactivada; activarla exige verificar soporte de
  binarios GLES/Piglet, formato/invalidation y rutas. No equivale a compilar en
  otro hilo ni promete mejorar lentitud sostenida.

El JIT PowerPC de Dolphin no reemplaza al recompilador MIPS R4300i. Tampoco sus
perfiles por juego, reducción automática de reloj emulado o límite 60/30 prueban
corrección o velocidad de N64. Próxima prioridad: medición física comparable de
Mario/Zelda (RSP HLE/respaldo, CPU y resto, entrada/salida SDL, presentación) y
optimización del componente que esa evidencia identifique.

Estado: **no integrado ni jugable desde R2RETRO todavía**. No se añade un core
libretro ficticio, entradas de juegos que no puedan arrancar ni un nuevo PKG.
La reorganización de la biblioteca sí está implementada por separado.

## Fuente fijada

- [DolphinPS4](https://github.com/iHaiDeeZ/DolphinPS4), revisión
  `6b0c6bdfde673c214245250181b1584b2c914787`.
- Base Dolphin: `771fb154059c5812d6a715a23226249cc42877a2`.
- Mesa 26.0.8, runtime LLVM/libc++ 21.1.6, cabeceras libdrm 2.4.125.
- Versiones registradas en `external/dolphinps4.lock.json`.
- Copia de consulta local: `build/references/DolphinPS4`; no se modifica ningún
  submódulo de emulación existente.

Se revisaron `BUILDING.md`, `patches/BASE.txt`, `patches/dolphin-ps4.patch`,
`toolchain/{env,build-libcxx,prepare-sysroot}.sh`,
`toolchain/ps4-love-style.cmake`, `scripts/{configure-dolphin,build-mesa,package-dolphin}.sh`
y `port/mesa/ac_ps4_drm.c`.

## Qué cambia respecto a los cores actuales

DolphinPS4 es una aplicación con entrada, runtime, asignador de memoria, JIT,
audio, mandos y presentación propios. Su configuración desactiva SDL y habilita
Vulkan. No proporciona el contrato libretro de `CoreManager`.

Su RADV incorpora un winsys que envía comandos mediante GNM y asigna memoria
directa. **No es la biblioteca vulkan-ps4 analizada para ParaLLEl-RDP**. La
limitación `storageBuffer16BitAccess=false` de aquella revisión no permite
concluir que esta otra implementación tenga la misma limitación. Tampoco se
ha probado este RADV en nuestra aplicación ni se ha portado N64 a él.

El port cambia el CRT, malloc, pthread_once, mmap, el linker script y libc++.
El propio script de configuración documenta una incompatibilidad de estructuras
curl/mbedTLS y utiliza su curl independiente. Enlazar todas estas sustituciones
directamente con R2RETRO arriesgaría los cores, SDL y HTTPS que ya funcionan.

## Arquitectura propuesta, todavía por implementar

Un ejecutable SELF adicional dentro del mismo paquete, con ciclo de vida
separado de SDL/Piglet. El usuario seleccionaría el juego desde la carpeta de
consola habitual; el cambio de proceso sería interno.

El port ya usa `sceSystemServiceLoadExec` y un archivo de lanzamiento para
separar su propio XMB del emulador. Se puede aprovechar ese diseño, pero **no
basta con copiar su eboot**:

1. Su `Platform::GetPS4Arguments()` reemplaza argv. Hay que definir un contrato
   de lanzamiento acotado, validado y consumido una sola vez; pasar argumentos
   al ejecutable sin adaptarlo no seleccionaría el juego.
2. `ps4_self_path`, `AssetsDir`, `ps4_return_to_menu` y `ps4_relaunch_game` deben
   distinguir reiniciar Dolphin de volver a R2RETRO. Conservar `RNTD00064` y
   restaurar la consola/juego seleccionado después del regreso.
3. Sus datos predeterminados son `/data/DolphinPS4`, con User, NAND, tarjetas y
   estados propios. La integración necesita una raíz derivada de la configuración
   de R2RETRO, sin sobrescribir una instalación independiente ni los guardados
   existentes. No heredar silenciosamente su actualizador o descargas al arrancar.
4. El scanner actual está diseñado para cartuchos pequeños. Para ISO/GCM y
   contenedores RVZ/WBFS/etc. hacen falta DiscIO o lectores equivalentes acotados:
   distinguir GC/Wii por contenido, identidad estable, multidisco y errores de
   contenedor. No cargar una imagen de varios GiB completa en RAM ni tratar toda
   extensión `.iso` como GameCube.
5. Ajustar mandos GC/Wii, pausa y guardado; Wiimote, puntero, movimiento y títulos
   que requieren periféricos necesitan validación específica.
6. Compilar y validar el SELF y sus recursos/licencias. Probar arranque, retorno,
   memoria liberada, reinicio, audio y guardado en PS4 original/Slim antes de
   declarar compatibilidad.

Licencias: revisar y conservar los avisos de Dolphin (GPL-2.0-or-later), Mesa,
runtime y dependencias. No incluir ROMs, NAND, claves ni firmware externos.

## Bloqueo de compilación reproducido

OpenOrbis existente funciona para R2RETRO. El entorno WSL no contiene clang 21,
sus herramientas LLVM, ninja, meson, glslangValidator, el sysroot libc++ 21 ni
Mesa/RADV preparado. El arranque de `toolchain/build-libcxx.sh` en un árbol aislado
terminó con `missing tool: clang-21`, antes de modificar OpenOrbis.

La copia descargada desde Git de Windows tenía CRLF; se regeneró exclusivamente
esa copia de consulta con `core.autocrlf=false` antes del intento. El fallo CRLF
inicial no se confunde con el fallo real de dependencias.

Inventario reproducible, sin instalar ni ejecutar scripts upstream:

```sh
python3 scripts/check-dolphinps4-build.py > build/dolphin-build-preflight.json
```

Devuelve 2 si faltan dependencias. Un inventario completo tampoco certifica ABI,
parches, enlace, ejecución ni velocidad. Evidencia local adicional:
`build/dolphin-bootstrap-check.log`.

No se ha compilado Dolphin, ejecutado GameCube/Wii ni reproducido las cifras
de la tabla upstream en R2RETRO. Quedan pendientes preparar ese entorno aislado
y desarrollar/probar el adaptador de proceso descrito arriba.
