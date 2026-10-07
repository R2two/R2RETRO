# Instalación física v0.5.5 — 7 de octubre de 2026

El usuario confirmó «funcionó perfectamente» después de transferir por FTP el
PKG y seguir la instalación desde GoldHEN → Package Installer → HDD.

Antes de la instalación se verificó por lectura FTP que el paquete instalado
era v0.5.4: SHA256
`0b0ea347ff591c5bfddaf78651f5f90b1087e3cafa8d2daebe39921a366f356b`.
Se transfirió v0.5.5 a `/data/pkg/R2RETRO-v0.5.5-display-network.pkg` y se leyó
de nuevo para comprobar sus 65.994.752 bytes y SHA256
`1f6fef9555d84a13baba7f91c48b7bb09d6c5782b9cbb80ac174fadea6bbf6c9`.
El agente no desinstaló la aplicación ni modificó partidas o configuración.
La operación de instalación desde el menú la realizó el usuario.

Esto confirma el procedimiento FTP + instalador HDD en la PS4 del usuario.
No confirma el actualizador interno «Instalar y cerrar», la persistencia de
cada partida/configuración, todos los juegos ni una mejora de rendimiento.
No se compiló otro PKG; las fuentes posteriores a v0.5.5 siguen pendientes
de autorización para compilar. Evidencia de transferencia y paquete anterior:
`build/ps4-ftp-20261007/`.
