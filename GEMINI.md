# Gemini.md — hid_to_vpad · refactor-network-logic branch

## Propósito del proyecto

Plugin WUPS (Wii U Plugin System) para Aroma que permite usar dispositivos HID y un cliente de red como controladores en la Wii U. Esta rama refactoriza la lógica de red añadiendo un servidor TCP/UDP propio en C++ y un cliente Python ligero para pruebas desde PC o Android/Termux, sin necesidad del Java Network Client original.

---

## Estructura de archivos relevante

```
src/
  main.cpp                        — Ciclo de vida del plugin (INITIALIZE, ON_APPLICATION_START, DEINITIALIZE, ON_APPLICATION_REQUESTS_EXIT)
  ConfigHooks.cpp                 — Menú WUPS config (rumble, network toggle, pad mapping por VID/PID)
  FunctionPatches.cpp             — Hooks de VPADRead y todas las funciones WPAD* (Pro Controllers virtuales)
  NetworkServer.cpp               — Servidor TCP (puerto 8112) + socket UDP (8113); hilo OSThread propio
  NSysNetCompat.cpp               — Shims socketclose/socketlasterr sobre POSIX close/errno
  WUPSConfigItemPadMapping.cpp/.h — Config item personalizado para seleccionar dispositivo por presión de botón
  utils/
    StringTools.cpp/.h            — Utilidades de string (split, strfmt, etc.)
    logger.h                      — Macros DEBUG_FUNCTION_LINE sobre WHBLog
tools/
  hid_to_vpad_keyboard_client.py  — Cliente Python teclado→XInput que se conecta al servidor TCP
controller_configs/
  python_keyboard_xinput.ini      — Config controller_patcher para VID 0x7331 / PID 0x1337
Dockerfile                        — Build image basada en ghcr.io/wiiu-env/devkitppc
Makefile                          — Build estándar WUPS; target: hidtovpad.wps
.github/workflows/ci.yml          — CI: clang-format → docker build → deploy nightly release
```

---

## Flujo de inicialización

```
INITIALIZE_PLUGIN()
  └─ WHBLogUdpInit()
  └─ InitConfigMenu()            ← registra callbacks de apertura/cierre del menú WUPS

ON_APPLICATION_START()
  └─ ControllerPatcher::Init()   ← carga configs de sd:/wiiu/controller/
  └─ ControllerPatcher::enableControllerMapping()
  └─ ConfigLoad()                ← lee WUPS storage: rumble, networkclient, pad mappings
  └─ ApplyNetworkServerState()   ← arranca o para el hilo de red según `runNetworkClient`
  └─ ControllerPatcher::disableWiiUEnergySetting()

ON_APPLICATION_REQUESTS_EXIT()
  └─ StopNetworkServer()
  └─ ControllerPatcher::resetCallbackData()
  └─ ControllerPatcher::restoreWiiUEnergySetting()

DEINITIALIZE_PLUGIN()
  └─ ControllerPatcher::DeInit()
  └─ StopNetworkServer()
```

---

## NetworkServer.cpp — detalles clave

| Constante | Valor | Notas |
|-----------|-------|-------|
| `NETWORK_PORT` | 8112 | TCP; acepta una conexión a la vez |
| `UDP_PORT` | 8113 | Socket abierto pero no implementado aún en esta rama |
| `PROTOCOL_VERSION` | 0x14 | Enviado en handshake; cliente debe responder con el mismo byte |

**Hilo:** `OSThread` con stack de 16 KB (`0x4000`), prioridad 20, cualquier core.

**Protocolo TCP (simplificado):**
```
S→C: version (1 byte = 0x14)
C→S: version (1 byte)
S→C: version echo (1 byte)
loop:
  C→S: cmd (1 byte)
  cmd=0x01 (ATTACH):
    C→S: handle (4 bytes) + vid (2) + pid (2)
    S→C: resp[5] = {0xE0, 0xE8, 0x00, 0x00, 0x00}
```

**Problemas conocidos / trabajo pendiente en esta rama:**
- `StopNetworkServer()` pone `networkThreadRunning = false` pero no hace join del hilo. Si el hilo está bloqueado en `accept()`, no se libera limpiamente.
- El `udp_socket` se crea pero nunca se usa; el procesamiento UDP está pendiente.
- La respuesta ATTACH es hardcoded (slot 0, pad 0). No hay lógica real de slot assignment.
- No hay manejo de errores en los `recv()` encadenados del bloque ATTACH (si llega menos de 8 bytes, se cuelga).

---

## FunctionPatches.cpp — hooks registrados

| Función hookeada | Biblioteca | Comportamiento |
|-----------------|-----------|----------------|
| `VPADRead` | VPAD | Inyecta datos HID en el buffer del GamePad; fuerza `VPAD_READ_SUCCESS` |
| `WPADProbe` | PADSCORE | Reporta `WPAD_EXT_PRO_CONTROLLER` para canales con controlador virtual activo |
| `WPADRead` | PADSCORE | Redirige a `ControllerPatcher::setProControllerDataFromHID` |
| `WPADSetConnectCallback` / `WPADSetExtensionCallback` / `KPADSetConnectCallback` | PADSCORE | Pasa al real; callbacks comentados (evitan crashes en transiciones) |
| `WPADGetBatteryLevel` | PADSCORE | Devuelve 4 (batería llena) para canales virtuales |
| `WPADGetDataFormat` / `WPADSetDataFormat` | PADSCORE | Fuerza `WPAD_FMT_PRO_CONTROLLER` |
| `WPADControlMotor` | PADSCORE | Redirige rumble a `ControllerPatcher::setRumble` |
| `WPADInit` | PADSCORE | Pass-through al real |

Todos los hooks usan `WUPS_MUST_REPLACE` → el build falla si la función no se encuentra en la librería indicada.

---

## ConfigHooks.cpp — almacenamiento WUPS

| Clave storage | Tipo | Default | Efecto |
|--------------|------|---------|--------|
| `rumble` | bool | valor actual de ControllerPatcher | activa/desactiva rumble |
| `networkclient` | bool | `true` | arranca/para NetworkServer |
| `gamepadmapping` | string | `""` | VID,PID,pad,type para Gamepad |
| `pro1`–`pro4` | string | `""` | Igual para Pro Controllers 1–4 |

Formato string de mapping: `"vid,pid,pad,type"` (enteros decimales, 4 tokens separados por coma).

---

## Cliente Python (tools/hid_to_vpad_keyboard_client.py)

- Conecta al puerto TCP 8112, hace handshake de versión.
- Lee teclas del terminal con `tty` raw mode.
- Mapeo por defecto: WASD → stick izquierdo, flechas → stick derecho, Space=A, J=B, K=X, L=Y, U/I=L/R, Q=Minus, E/Enter=Plus, H=Home.
- Flag `--hold` (default 0.18 s): tiempo que cada tecla se mantiene activa (los terminales no emiten key-release).
- Flag `--rate` (default 60): frecuencia de envío en Hz.
- VID/PID emitido: `0x7331` / `0x1337` → debe coincidir con `python_keyboard_xinput.ini`.

---

## Build

```sh
# Una vez
docker build . -t hid-to-vpad-plugin-builder

# Compilar
docker run -it --rm -v ${PWD}:/project hid-to-vpad-plugin-builder make

# Limpiar
docker run -it --rm -v ${PWD}:/project hid-to-vpad-plugin-builder make clean
```

Output: `hidtovpad.wps` → copiar a `sd:/wiiu/plugins/`.

Base image: `ghcr.io/wiiu-env/devkitppc:20260225` + `ghcr.io/wiiu-env/wiiupluginsystem:20260418` (WUPS 0.9.1+, requerido por Aroma Beta 26+).

---

## CI (.github/workflows/ci.yml)

1. **clang-format** — `ghcr.io/wiiu-env/clang-format:13.0.0-2 -r ./src`
2. **build-binary** — inyecta `src/version.h` con el hash de commit, luego `docker build + make`
3. **deploy-binary** — crea GitHub Release prerelease con el `.wps` zipeado; tag `REPONAME-YYYYMMDD-HHMMSS`

Trigger: `pull_request` o `workflow_dispatch`.

---

## Dependencias externas

| Librería | Propósito |
|---------|-----------|
| `libcontrollerpatcher` | Motor de emulación HID→VPad; gestiona mappings, slots, rumble |
| `libnotifications` | `NotificationModule_AddInfoNotification` / `AddErrorNotification` |
| `libwups` | WUPS hooks, storage API, config API |
| `libwut` | WUT runtime, POSIX compat, nn::ac |

---

## Instrucciones de Migración y Entorno

### Estado actual de la rama `refactor/network-logic`
Se ha refactorizado el protocolo de comunicación entre cliente y plugin. Ya no se depende exclusivamente de paquetes HID crudos.
- **Nuevo protocolo (TCP):** Se utiliza la estructura `ControllerState` (definida en `src/ControllerProtocol.h`) enviada a través del nuevo comando `CMD_INPUT` (0x02).
- **Servidor:** `NetworkServer.cpp` recibe `ControllerState` y actualiza `lastControllerState`.
- **Inyección:** `FunctionPatches.cpp` (en `VPADRead`) inyecta `lastControllerState` en el buffer de `VPADStatus` antes del procesamiento de `ControllerPatcher`.
- **Cliente:** `tools/hid_to_vpad_keyboard_client.py` ha sido actualizado para enviar `ControllerState`.

### Configuración en Codespaces / Docker
Para continuar el desarrollo en un entorno nuevo (como Codespaces):

1.  **Entorno Docker:** Este proyecto requiere `devkitppc` para compilar. Utiliza el `Dockerfile` en la raíz para construir la imagen necesaria:
    ```sh
    docker build . -t hid-to-vpad-plugin-builder
    ```

2.  **Compilación:**
    ```sh
    docker run -it --rm -v ${PWD}:/project hid-to-vpad-plugin-builder make
    ```

3.  **Formato de código:** El proyecto utiliza `clang-format`. Asegúrate de tenerlo instalado y aplicado antes de commitear:
    ```sh
    clang-format -i src/*.cpp src/*.h tools/*.py
    ```
    *(Nota: Evitar bucles de formateo excesivos si la versión de `clang-format` difiere del entorno original)*.
