# Investigación N64 de v0.4.5

El usuario comunicó en PS4 original/Slim: Mario 64 al 49 %, 25,5 VI/s,
29,6 ms/VI de núcleo, recompilador x64 activo, RDP con 4 hilos y audio RSP
original (LLE). Se conservan esas observaciones: el porcentaje y los VI/s no
equivalen exactamente entre sí a 60 Hz y no se corrige la lectura suponiendo
una escena o ventana de medición. No se dispone de perfil físico por componente.

El perfil de Linux confirma que merece priorizarse RDP y RSP. No demuestra
que la distribución porcentual ni el coste absoluto sean iguales en PS4.
HLE de audio ya es el valor predeterminado de la configuración del frontend;
su selección y sus tareas procesadas deben verificarse en la sesión física.

## Comparación controlada local

ROM autorizada leída en Downloads, sin copiarla, con SHA-256
`17ce077343c6133f8c9f2d6d6d9a4ab62c8cd2aa57c40aea1f490b4c8bb21d91`.
Mismo core `12edd2c74a517ff86dfa8cfc71ad75e4c10486d5` y parches 0001–0006,
dynarec x64, CXD4 SSE2, Angrylion 4 hilos, sincronización Low, VI Unfiltered.
Core Release `-O3`; mismo ejecutable de prueba enlazado con cada biblioteca.
El frontend de pruebas usa el build Debug existente, por lo que los tiempos
no se mezclan con anteriores medidas de frontend Release.

Ocho procesos secuenciales de 6.000 VI, guardados nuevos aislados y traza
`scripted`: Start en VI 600–611; A desde 840; movimiento/cámara desde 4.800.
Se alternó base/candidato por modo y se invirtió el orden en la segunda
pasada. Compilaciones y pruebas del proyecto detenidas durante este A/B.
Se mide únicamente `core.run`, excluyendo PNG, hashes y escrituras del banco.
El perfil estuvo activo; no hubo fallos de reloj ni desbordes de scopes.

| Audio | Base ms/VI | Candidato ms/VI | Cambio de tiempo |
|---|---:|---:|---:|
| LLE | 3,392 | 3,305 | −2,6 % |
| HLE | 3,066 | 3,156 | +2,9 % |

Son medianas de dos sesiones por celda; la dispersión impide afirmar un
beneficio consistente. En base/LLE: RDP 1,890 ms/VI y RSP 1,140. En base/HLE:
RDP 1,779 y RSP 0,932, más 0,029 de síntesis HLE. Los tiempos incluyen esperas
del hilo que despacha; no son suma de CPU de los trabajadores ni FPS de PS4.

Todas las sesiones conservaron vídeo `9fe391f782a5a5cb`, cuadro final
`297d4c28c7490ea5`, 8.764.734 muestras PCM y 5.961 tareas HLE cuando se activó.
PCM LLE `7d68924ee356c00c`; HLE `e083c11542dddf9e`. Se exige igualdad por modo,
no entre dos algoritmos de síntesis distintos. La ROM original quedó intacta.

## Candidatos evaluados y decisión

El candidato reduce avisos de la barrera Angrylion: sólo el último trabajador
notifica al hilo principal. También sustituye cuatro escrituras escalares y
recarga temporal del selector CXD4 de elementos 2/3 por una carga alineada y
dos shuffles SSE2. No cambia hilos, comandos, aritmética, shaders ni GPU.
El desensamblado confirma que GCC no había eliminado el temporal original.

Los oráculos sintéticos del dispatcher real verificaron 49.152 combinaciones
de elementos, registros y alias, incluyendo `vt=31`. La barrera completó
30.000 lotes en 60 ciclos de creación/destrucción con 1/2/3/4/8 trabajadores.
Ambos pasaron ASan/UBSan, también contra el core base. Sin embargo, **el
candidato combinado no se integra al núcleo ni al PKG**, al no demostrar
mejora global uniforme. Las fuentes compartidas y parches 0001–0006 no cambian.

Otra ejecución instrumentada de 6.000 VI por modo contó 4.794 lotes RDP,
3.450.568 comandos y 1.400.160 comandos de dibujo: **ningún lote era sólo de
estado**. Evitar despertar hilos para tales lotes no ayudaría esta traza.
Los elementos RSP 2/3 sumaron 33.483.661 de 769.798.692 operaciones vectoriales
LLE (4,35 %) y 25.464.605 de 629.792.218 HLE (4,04 %). Se descarta atribuirles
una gran parte del coste global. Los tiempos instrumentados no se comparan.

## Evidencia y reproducción

- `build/n64-performance-v045/comparison-20261005T221057Z/comparison.json`:
  ocho informes con perfiles, hashes, capturas y PCM; resumen en
  `build/n64-performance-v045/comparison-summary.json`.
- `build/n64-performance-v045/counts/20261005T221801Z-1278/`: conteos por modo.
- Base `.so`: SHA-256
  `1dfc838462b596dea9d8d1d05fb526cce72364a60e3353fc46781818dfecd8ff`.
- Candidato conservado en `build/n64-performance-v045/candidate-combined.so`:
  `15402c064aea2c7884442ab1afe87e7f25c3e4fcf1007485c979126c8722e4e1`.
  Su diff queda sólo en `build/n64-performance-v045/candidate-combined.patch`.
  La copia `variant/source` recibió después contadores experimentales.
- `scripts/run-rom-matrix.py --core-library <biblioteca>` registra la biblioteca
  del probe aislado, en vez de atribuir siempre el hash de `core-lab`.
- `bash scripts/check-n64-hotpaths.sh <copia-core> <salida-aislada>` ejecuta los
  dos oráculos originales; no carga ROMs ni modifica la copia del núcleo.

Las cuatro sesiones preliminares no se usan para decidir rendimiento: se
solaparon con compilación/vídeo del frontend. Ningún resultado de este banco
confirma más velocidad en PS4 ni activa GLideN64/Piglet como renderer del N64.
