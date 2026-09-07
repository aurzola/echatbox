# echatbox

Xbox 360 Chatpad → **BLE HID keyboard** para ESP32 clásico (con Bluetooth).

El chatpad es autónomo: su micro escanea las teclas y las saca por un
**UART de 3,3 V a 19200 8N1**. Este firmware lee ese bus, decodifica los
scancodes y se presenta como teclado Bluetooth al PC.

## Cableado (chatpad ↔ ESP32)

El conector del chatpad (el que iba al mando) o sus test points del PCB son
el mismo nodo eléctrico. Sobre la placa del chatpad:

| Señal | Punto         | ESP32 |
|-------|---------------|-------|
| +3,3 V| TP5           | 3V3   |
| GND   | TP8           | GND   |
| TX    | TP9           | GPIO16 (RX) |
| RX    | conector/pin 2 (síguelo hasta el chip con continuidad) | GPIO17 (TX) |

- No uses los pines de audio (tip/ring/shield).
- Antes el mando alimentaba el chatpad; ahora se lo das tú desde el 3V3 del
  ESP32 (comparten GND).
- Pines UART configurables al inicio de `echatbox.ino` (`CP_UART_RX/TX`).

## Build y flash

Requiere `arduino-cli` con el core `esp32:esp32` instalado.

```sh
bash build.sh
PORT=/dev/ttyUSB0 bash flash.sh   # puerto por defecto /dev/ttyUSB0
```

El ESP32 aparece como teclado **"Xbox 360 Chatpad"**; vincúlalo por Bluetooth
al PC/telefono y escribe.

## Protocolo (resumen)

- Frames de 8 bytes: `A5` (estado, se ignora) o `B4 C5 00 mod k0 k1 00 CS`.
- `mod`: `bit0` shift · `bit1` verde · `bit2` naranja · `bit3` people.
- Checksum = complemento a 2 de la suma de los 7 primeros bytes.
- Host envía init `87 02 8C 1F CC` y awake `87 02 8C 1B D0` (este firmware
  reenvía awake cada ~700 ms para que no se duerma).

## Notas y limitaciones

- **Layout del host**: el mapeo es el de un teclado físico US (QWERTY). Para
  que los símbolos coincidan con las leyendas del chatpad, deja el PC en
  distribución US o una basada en QWERTY-US.
- Capas shift/verde/naranja: shift + letras → mayúsculas; las capas verde y
  naranja usan las tablas de símbolos de fdufnews (ajustables en
  `src/keymap.cpp`). La tecla **people** se ignora a propósito.
- No hay auto-repeat local: al mantener una tecla pulsada, el host repite
  (typematic) porque el reporte se mantiene.
- El parseo está tomado de la librería de Cliff L. Biffle / notas de
  fdufnews y belse-de (licencia BSD-3); ver cabeceras de `src/*`.
- Probado solo a nivel de build en este repo; el pinout del chatpad debe
  verificarse con el multímetro contra TP5/TP8/TP9 en tu unidad.

## Estructura

```
echatbox.ino              setup/loop: UART + sincronización BLE
src/chatpad.{h,cpp}       lector UART, sync de frames, checksum
src/keymap.{h,cpp}        resolución de tecla según capa -> tecla host
libraries/ESP32-BLE-Keyboard   librería BLE-HID (parcheada para core 3.3.10/IDF5)
build.sh / flash.sh       compile / upload con arduino-cli
```
