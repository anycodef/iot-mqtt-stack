# Guía de hardware — Armado y cableado del nodo Lab 08 (ESP32 DevKit V1)

Esta guía cubre **qué materiales usar, cómo montarlos en la protoboard y, sobre todo,
el cableado pin por pin** del nodo sensor cifrado de **Lab 08** (`publisher-ecc`).
El flasheo del firmware se hace después, en una sesión aparte con `arduino-cli`
(ver [`guia-esp32.md`](./guia-esp32.md) para el flujo de software).

> **Tu placa es un ESP32 DevKit V1** (ESP32-WROOM-32, el "clásico"), **no** un ESP32-S3.
> No importa: los pines que usa el firmware (GPIO4 / 21 / 22 / 35 / 36 / 26) existen
> igual en el DevKit V1, así que el sketch corre sin cambios. Lo único distinto es el
> identificador de placa al compilar (`esp32:esp32:esp32doit-devkit-v1`), que ya está
> fijado en `firmware/publisher-ecc/sketch.yaml`.

---

## 1. Materiales para Lab 08 (lo mínimo, confirmado con tu kit)

| # | Componente | Cant. | El que tú tienes | Nota |
|---|-----------|:---:|---|---|
| 1 | ESP32 **DevKit V1** | 1 | ✅ (tienes 2) | Para Lab 08 basta una |
| 2 | **DHT22** (módulo 3 pines, blanco) | 1 | ✅ `+ / out / -` | Trae pull-up interno → **no** necesitas resistencia |
| 3 | **BME280** (6 pines) | 1 | ✅ `VCC GND SCL SDA CSB SDO` | Ver cableado especial abajo |
| 4 | **MQ-2** (gas) | 1 | ✅ `VCC GND DO AO` | El calefactor pide **5 V** |
| 5 | **Protoboard** | 1 | ✅ (tienes 2) | |
| 6 | **Jumpers** | ~14 | ✅ | M-M y M-H |
| 7 | **Cable USB de datos** | 1 | ✅ (laptop) | Alimenta y flashea la placa |

**Opcional (no es parte de Lab 08):** el **relay de 1 canal** (`VCC GND IN / NO COM NC`)
pertenece al lazo de alarma de Lab 07. Si solo quieres terminar Lab 08 (cifrado + Google
Sheets), **déjalo para después**. Más abajo va su cableado por si lo quieres añadir.

**Sobre tus resistencias de 470 Ω:** para el DHT22 de 3 pines **no hacen falta**. Solo te
sugiero usar **2 de ellas** como divisor de tensión en la salida analógica del MQ-2 (paso 4),
porque esa salida puede subir por encima de los 3.3 V que tolera el ADC del ESP32.

---

## 2. Conoce tu DevKit V1 antes de cablear

- Se alimenta y se programa por el **micro-USB**. En Linux aparece como
  **`/dev/ttyUSB0`** (chip USB-serie CP2102 o CH340). No usa USB nativo, así que **no**
  toques la opción "USB CDC On Boot": no aplica a esta placa.
- Pines de energía que vas a usar:
  - **`3V3`** → para DHT22 y BME280 (lógica de 3.3 V).
  - **`VIN` (= 5 V del USB)** → para el calefactor del MQ-2 (y el relay si lo usas).
  - **`GND`** → hay varios; **todos los GND deben ir juntos** (masa común).
- Regla de oro: **nunca metas más de 3.3 V a un pin GPIO**. El ESP32 **no** tolera 5 V
  en sus entradas (de ahí el cuidado con la salida analógica del MQ-2).

---

## 3. Cableado pin por pin

Conecta **primero todas las masas (GND) y la alimentación**, luego las señales. Deja un
riel `-` (GND) y un riel `+` de 3.3 V en la protoboard, alimentados desde el ESP32.

### 3.1 DHT22 (3 pines: `+ / out / -`) — temperatura y humedad

| Pin DHT22 | Va a | GPIO ESP32 |
|---|---|---|
| `+` | 3.3 V | `3V3` |
| `out` | señal de datos | **`GPIO4`** |
| `-` | masa | `GND` |

> Módulo de 3 pines = pull-up ya incluido. No agregues resistencia.

### 3.2 BME280 (6 pines) — presión (I2C, dirección **0x76**)

| Pin BME280 | Va a | GPIO ESP32 | Por qué |
|---|---|---|---|
| `VCC` | 3.3 V | `3V3` | alimentación |
| `GND` | masa | `GND` | |
| `SCL` | reloj I2C | **`GPIO22`** | bus I2C por defecto del ESP32 |
| `SDA` | datos I2C | **`GPIO21`** | |
| `CSB` | **a VCC (3.3 V)** | `3V3` | **CSB en alto = modo I2C** (sin esto el módulo cree que es SPI) |
| `SDO` | **a GND** | `GND` | **SDO en bajo = dirección 0x76** (el firmware usa 0x76) |

> Así resuelves tu duda de la dirección: **no hay que "comprobarla", se elige por hardware.**
> `SDO → GND` ⇒ **0x76** (lo que espera el código). Si por error lo pones a 3.3 V sería 0x77
> y el firmware imprimiría `[BME280] not found at 0x76`. En ese caso, mueve `SDO` a GND.

### 3.3 MQ-2 (4 pines: `VCC GND DO AO`) — gas

| Pin MQ-2 | Va a | GPIO ESP32 | Nota |
|---|---|---|---|
| `VCC` | **5 V** | `VIN` | el calefactor necesita 5 V |
| `GND` | masa | `GND` | |
| `DO` | salida digital (umbral) | **`GPIO35`** | dispara la alarma de gas; el umbral se ajusta con el **potenciómetro** del módulo |
| `AO` | salida analógica | **`GPIO36`** | nivel de gas en crudo (`gas_raw`) — **ver aviso** |

> ⚠️ **Aviso de 3.3 V (importante):** con el MQ-2 alimentado a 5 V, su `AO` puede superar
> los 3.3 V y **dañar** el pin `GPIO36`. Dos opciones:
>
> 1. **Sencilla y segura para Lab 08:** la alarma de gas funciona con **`DO`** (digital),
>    así que puedes **no conectar `AO`** y listo. Perderías solo el valor numérico `gas_raw`,
>    pero el cifrado + envío a Sheets funciona igual.
> 2. **Si quieres el valor analógico:** pon un **divisor con tus 2 resistencias de 470 Ω**
>    en `AO` (AO → 470 Ω → nodo → 470 Ω → GND; del nodo sale el cable a `GPIO36`). Eso baja
>    la tensión a la mitad (máx ~2.5 V, seguro). Como el valor llega a la mitad, el umbral
>    `GAS_THRESHOLD` del firmware quedaría "más sensible"; lo recalibramos al flashear si lo necesitas.
>
> El MQ-2 necesita **calentar** (decenas de segundos, idealmente unos minutos) antes de dar
> lecturas estables. No te asustes si al inicio marca alto.

### 3.4 Relay de 1 canal — **OPCIONAL** (lazo de alarma, no es Lab 08)

Solo si además quieres cerrar el lazo de Lab 07 (en la **otra** placa, el sketch
`subscriber-relay`). No lo necesitas para terminar Lab 08.

| Pin relay | Va a | GPIO ESP32 |
|---|---|---|
| `VCC` | 5 V | `VIN` |
| `GND` | masa | `GND` |
| `IN` | señal de control | **`GPIO26`** |

> Es **active-LOW**: el firmware lo arranca en HIGH (apagado) en `setup()`. Los contactos
> de potencia (`NO / COM / NC`) van al equipo que quieras conmutar, nunca al ESP32.

---

## 4. Diagrama resumido (un solo vistazo)

```
ESP32 DevKit V1
  3V3 ──┬── DHT22(+)
        ├── BME280(VCC)
        └── BME280(CSB)        (CSB a 3V3 = modo I2C)

  VIN(5V) ─── MQ-2(VCC)        (+ relay VCC, si lo usas)

  GND ──┬── DHT22(-)
        ├── BME280(GND)
        ├── BME280(SDO)        (SDO a GND = dir. 0x76)
        └── MQ-2(GND)          (+ divisor 470Ω y relay GND, si aplican)

  GPIO4  ── DHT22(out)
  GPIO21 ── BME280(SDA)
  GPIO22 ── BME280(SCL)
  GPIO35 ── MQ-2(DO)
  GPIO36 ── MQ-2(AO)           (directo si NO supera 3.3V; o vía divisor 470Ω+470Ω)
  GPIO26 ── relay(IN)          (opcional)
```

---

## 5. Orden de armado en la protoboard

1. Con el ESP32 **desconectado del USB**, colócalo a caballo del canal central.
2. Lleva `3V3` al riel `+` y `GND` al riel `-` de la protoboard.
3. Cablea **masas y alimentaciones** de los 3 sensores primero (incluye `CSB→3V3` y `SDO→GND`).
4. Cablea las **señales**: GPIO4, 21, 22, 35 (y 36 directo o por divisor).
5. Revisa dos veces que **ningún 5 V** llegue a un GPIO y que `SDO` esté a **GND**.
6. Recién entonces conecta el USB a la laptop.

---

## 6. Verificación de software (ya hecha) y reproducibilidad

El entorno ya quedó instalado y **el firmware de Lab 08 compila para tu DevKit V1**
(prueba real: 73 % de flash, sin errores). Para que cualquier máquina reproduzca el mismo
build, el sketch trae un **`sketch.yaml`** (equivalente a un `package.json` con versiones
fijas: core `esp32 3.3.10` + las 7 librerías pinneadas).

```bash
# Compilar de forma reproducible (usa las versiones fijadas en sketch.yaml):
arduino-cli compile --profile devkitv1 firmware/publisher-ecc

# (Ya tienes instalado: core esp32:esp32@3.3.10 y las librerías
#  PubSubClient, DHT, Adafruit Unified Sensor, Adafruit BME280, BusIO,
#  ArduinoJson, micro-ecc.)
```

---

## 7. Siguiente paso: flasheo interactivo

Con el hardware armado y el USB conectado, en la sesión de flasheo:

```bash
# 1) Detectar la placa y su puerto:
arduino-cli board list          # debe salir algo como /dev/ttyUSB0

# 2) Compilar + subir con el perfil reproducible:
arduino-cli upload --profile devkitv1 -p /dev/ttyUSB0 firmware/publisher-ecc

# 3) Ver el monitor serie a 115200 baudios:
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

> En Arch, si `arduino-cli` no puede abrir el puerto por permisos, agrega tu usuario al
> grupo del puerto serie (`uucp`) y reinicia la sesión:
> `sudo usermod -aG uucp $USER`.

Antes de flashear necesitas dos cosas del lado servidor (las maneja el otro equipo, o tú
mismo si corres todo en una máquina): tus credenciales Wi-Fi/IP del broker en
`firmware/publisher-ecc/arduino_secrets.h`, y el header `ecc_public_key.h` (la clave
**pública**) ya generado. Ver [`guia-esp32.md`](./guia-esp32.md) y [`guia-stack.md`](./guia-stack.md).
