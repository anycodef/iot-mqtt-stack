<!-- Informe generado para la entrega del Laboratorio 08 — IoT UNMSM.
     Fundamentado en el código real del repositorio iot-mqtt-stack. -->

# Universidad Nacional Mayor de San Marcos
## Facultad de Ingeniería de Sistemas e Informática
### Curso: Internet de las Cosas

# Guía de Laboratorio Nº 08 — Protocolos de Comunicación IoT con Cifrado ECC y Persistencia en Google Sheets

| | |
|---|---|
| **Integrantes** | Pumapillo Sarmiento, Bruno |
| | Sota Rios, Pedro Josue |
| | Quineche Casana, Luiz Ignacio |
| | Davalos Benito, Rodrigo |
| **Docente** | Msc. Jorge L. Guerra Guerra |
| **Ciclo** | 2026-I (8.º ciclo) |
| **Fecha** | _[completar]_ |

## 1. Introducción

El presente laboratorio integra tres pilares del Internet de las Cosas en un único flujo
extremo a extremo: la **comunicación liviana mediante MQTT**, la **confidencialidad de los
datos mediante criptografía de curva elíptica (ECC)** y la **persistencia en la nube usando
Google Sheets**. El objetivo es construir un nodo sensor basado en **ESP32-S3** que adquiere
variables ambientales (temperatura, humedad, presión y gas), las serializa en JSON y, antes
de transmitirlas, las **cifra con el esquema ECIES** (Elliptic Curve Integrated Encryption
Scheme) sobre la curva **P-256**. El mensaje cifrado se publica en un broker **Mosquitto**, de
modo que el intermediario nunca observa el contenido en claro: únicamente **Node-RED**, que
custodia la clave privada, puede descifrarlo. Finalmente, cada lectura descifrada se persiste
en una hoja de cálculo de Google a través de un **endpoint de Apps Script (`doPost`)**.

A diferencia de un despliegue MQTT tradicional, donde cualquier suscriptor o el propio broker
podría leer la telemetría, aquí se introduce la **confidencialidad de extremo a extremo**: el
cifrado se realiza en el dispositivo y el descifrado en el receptor autorizado. El uso de
**pares de claves efímeros por mensaje** aporta **Perfect Forward Secrecy (PFS)**, una
propiedad central que se analiza a lo largo del informe. La práctica demuestra que es viable
ejecutar criptografía asimétrica moderna en un microcontrolador de bajo costo y combinarla con
servicios gratuitos de la nube para obtener un sistema IoT seguro, observable y reproducible.

## 2. Marco Teórico

### 2.1 Arquitectura MQTT

**MQTT** (Message Queuing Telemetry Transport) es un protocolo de mensajería de patrón
**publicación/suscripción** diseñado para redes con ancho de banda limitado y dispositivos de
recursos restringidos. Sus actores son:

- **Broker:** servidor central que recibe los mensajes y los reenvía a los suscriptores. En
  este laboratorio es **Eclipse Mosquitto**, escuchando en el puerto **1883**.
- **Publicador (publisher):** cliente que envía mensajes a un **tópico** (p. ej. el ESP32-S3
  que publica en `unmsm/iot2025/lab-a/g1/sensores/json_enc`).
- **Suscriptor (subscriber):** cliente que recibe los mensajes de los tópicos a los que se
  suscribe (p. ej. Node-RED).

Los **tópicos** se organizan jerárquicamente con `/`, lo que permite filtrar con comodines
(`+`, `#`). El protocolo define tres **niveles de Calidad de Servicio (QoS)**:

- **QoS 0 — *at most once*:** entrega "dispara y olvida", sin confirmación. Mínimo overhead.
- **QoS 1 — *at least once*:** garantiza la entrega con confirmación `PUBACK`; admite duplicados.
- **QoS 2 — *exactly once*:** entrega única garantizada mediante un *handshake* de cuatro fases.

Un mecanismo clave para la fiabilidad operativa es el **Last Will and Testament (LWT)**: un
mensaje que el cliente registra al conectarse y que el broker publica automáticamente si la
conexión se pierde de forma inesperada, permitiendo notificar la caída del nodo.

### 2.2 Criptografía de Curva Elíptica y el esquema ECIES

La **criptografía de curva elíptica (ECC)** ofrece el mismo nivel de seguridad que RSA con
claves mucho más pequeñas (una clave ECC de 256 bits equivale aproximadamente a RSA-3072), lo
que la hace idónea para dispositivos embebidos. Su seguridad se basa en la dificultad del
**problema del logaritmo discreto en curvas elípticas (ECDLP)**. En este laboratorio se emplea
la curva estándar **P-256 (secp256r1 / prime256v1)**.

El esquema implementado es **ECIES** (Elliptic Curve Integrated Encryption Scheme), un esquema
híbrido que combina un acuerdo de claves asimétrico (ECDH) con un cifrado simétrico (AES). El
flujo, **paso a paso**, es el siguiente:

1. **Par efímero:** el emisor genera un par de claves *de un solo uso* `(r, R = r·G)`, donde
   `G` es el punto generador de la curva.
2. **ECDH:** calcula el secreto compartido `S = r·Q`, siendo `Q` la **clave pública del
   receptor** (embebida en el firmware).
3. **KDF (derivación de clave):** deriva la clave simétrica con
   `k = SHA-256(S.x ‖ R.x)[:16]` (los primeros 16 bytes → AES-128).
4. **Cifrado simétrico:** cifra el JSON con **AES-128 en modo CTR** usando un *nonce*
   aleatorio de 16 bytes.
5. **Transmisión:** publica `R.x ‖ R.y ‖ nonce ‖ ciphertext` (codificado en Base64).
6. El **broker reenvía únicamente el ciphertext**; nunca puede descifrarlo.
7. **ECDH del receptor:** Node-RED recupera `R` del mensaje y calcula `S = d·R`, donde `d` es
   su **clave privada**. Por la propiedad del ECDH, `r·Q = r·(d·G) = d·(r·G) = d·R`.
8. **KDF + AES idénticos:** deriva la misma `k` y descifra, obteniendo el JSON en claro.

Como **cada mensaje usa un par efímero distinto**, las claves de sesión no se reutilizan: esto
es lo que sustenta la propiedad de **Perfect Forward Secrecy (PFS)** (ver Cuestionario, P4).

![Figura 1 — Arquitectura del sistema (extremo a extremo)](docs/img/arquitectura-sistema.png)
*Figura 1. Flujo de datos: el ESP32-S3 cifra con ECIES y publica en Mosquitto, que solo ve
ciphertext; Node-RED descifra y persiste en Google Sheets. El stack incluye además InfluxDB y
Grafana para visualización.*

![Figura 2 — Diagrama de secuencia ECIES](docs/img/secuencia-ecies.png)
*Figura 2. Los 8 pasos del intercambio ECIES entre el ESP32-S3 (emisor) y Node-RED (receptor),
con el broker como intermediario que nunca accede al texto en claro.*

### 2.3 Flujo de datos hacia Google Sheets

Tras el descifrado, Node-RED envía cada lectura mediante una petición **HTTP POST** a un
**web app de Google Apps Script**. La función `doPost(e)` recibe el cuerpo JSON, lo parsea y
**añade una fila** (`appendRow`) a la hoja de cálculo, mapeando los campos a 8 columnas y
agregando una marca de tiempo ISO 8601. Apps Script responde con un JSON
`{ status: 'ok', fila: N, timestamp }`, lo que permite confirmar la persistencia. Este enfoque
convierte una hoja de cálculo en un **almacén de series temporales** accesible y gratuito,
adecuado para prototipos y laboratorios.

## 3. Actividades 1–3 — Verificación base MQTT

Antes de introducir el cifrado se valida el pipeline MQTT en claro, reutilizando los sketches
base del proyecto (`publisher-dht22`, `publisher-multisensor` y `subscriber-relay`) sobre el
mismo broker Mosquitto. Se verifica: (a) la publicación de telemetría del **DHT22**
(temperatura/humedad); (b) la publicación multisensor incorporando **BME280** (presión) y
**MQ-2** (gas); y (c) el lazo de actuación con el **relé** y la señalización de presencia
mediante **LWT** en el tópico `status`.

La jerarquía de tópicos del grupo, con su QoS y su condición de cifrado, se resume en la
Figura 3.

![Figura 3 — Jerarquía de tópicos MQTT](docs/img/topicos-mqtt.png)
*Figura 3. Espacio de nombres `unmsm/iot2025/lab-a/g1/...`. En verde, el tópico cifrado
(`sensores/json_enc`); en rojo, los tópicos operativos en claro (telemetría base, alarmas,
actuadores y `status`/LWT).*

![Figura 4 — Monitor Serie del ESP32-S3 (telemetría en claro)](docs/img/04-monitor-serie-base.png)
> Captura requerida: el Monitor Serie del Arduino IDE a 115200 baudios mostrando la conexión
> Wi-Fi (`[WiFi] Connected. IP: ...`), la conexión al broker (`[MQTT] ... connected`) y las
> líneas de publicación con los valores leídos de temperatura/humedad del DHT22.

![Figura 5 — MQTT Explorer con la telemetría base en claro](docs/img/05-mqtt-explorer-claro.png)
> Captura requerida: MQTT Explorer conectado al broker, mostrando el árbol de tópicos
> `unmsm/iot2025/lab-a/g1/...` y el contenido **legible** (JSON en claro) de
> `sensores/dht22` y/o del multisensor, además del tópico `status` con el valor `online`.

![Figura 6a — LWT: el broker publica «offline» al caer el nodo](docs/img/06-relay-lwt.png)
*Figura 6a. Al desconectarse el ESP32, el broker detecta la pérdida de conexión y publica
automáticamente el mensaje **Last Will and Testament** `offline` en el tópico `status`, sin
intervención del dispositivo.*

![Figura 6b — El nodo vuelve a «online» al reconectarse](docs/img/06-relay-lwt-online.png)
*Figura 6b. Cuando el ESP32 se reconecta al broker, publica `online` (retenido) en el mismo
tópico, restableciendo la señalización de presencia del nodo.*

## 4. Actividad 4 — Cifrado ECC (ECIES)

El nodo `publisher-ecc` (identificador de cliente MQTT **`esp32-g1-ecc`**) adquiere los
sensores, serializa el JSON y lo cifra con ECIES antes de publicarlo en
`unmsm/iot2025/lab-a/g1/sensores/json_enc`. El **formato en el cable** (antes de Base64) es:

```
[ eph_pub_x(32) | eph_pub_y(32) | nonce(16) | ciphertext(N) ]
```

es decir, la **clave pública efímera sin comprimir** (coordenadas X e Y, 64 B), el *nonce* de
16 B y el texto cifrado. A continuación se incrustan las primitivas reales del firmware.

### 4.1 Derivación de clave (KDF) — `firmware/publisher-ecc/publisher-ecc.ino`

La clave AES de 128 bits se obtiene como los primeros 16 bytes de `SHA-256(S.x ‖ R.x)`, donde
`S.x` es la coordenada X del secreto compartido ECDH y `R.x` la coordenada X de la clave
efímera. Esta derivación es **idéntica** en el emisor y el receptor, condición necesaria para
que ambos obtengan la misma clave.

```cpp
// aes_key(16) = SHA256( shared_x(32) || eph_pub_x(32) )[:16]
static void kdf(const uint8_t* shared_x, const uint8_t* eph_pub_x, uint8_t* aes_key) {
  uint8_t in[64];
  memcpy(in,      shared_x,  32);
  memcpy(in + 32, eph_pub_x, 32);
  uint8_t hash[32];
  mbedtls_sha256(in, 64, hash, 0);   // 0 => SHA-256
  memcpy(aes_key, hash, 16);
}
```

### 4.2 Cifrado simétrico AES-128-CTR — `publisher-ecc.ino`

Se emplea **AES-128 en modo contador (CTR)**, provisto por **mbedtls** (incluido en el core
del ESP32). El modo CTR no requiere relleno (*padding*): el texto cifrado tiene exactamente la
misma longitud que el texto plano, y la misma rutina sirve para cifrar y descifrar.

```cpp
// AES-128-CTR (same call encrypts & decrypts; CTR uses the forward key only).
static void aes_ctr_crypt(const uint8_t* key, const uint8_t* nonce16,
                          const uint8_t* in, uint8_t* out, size_t len) {
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, key, 128);
  uint8_t nonce_counter[16];
  memcpy(nonce_counter, nonce16, 16);
  uint8_t stream_block[16] = {0};
  size_t  nc_off = 0;
  mbedtls_aes_crypt_ctr(&aes, len, &nc_off, nonce_counter, stream_block, in, out);
  mbedtls_aes_free(&aes);
}
```

### 4.3 Cifrado ECIES completo — `publisher-ecc.ino`

La función orquesta el esquema: genera el par efímero con **micro-ecc** (`uECC_make_key`),
calcula el secreto compartido con la clave pública del receptor (`uECC_shared_secret`), deriva
la clave (KDF), genera el *nonce* y ensambla el mensaje `[X|Y|nonce|ct]`.

```cpp
// Output: [eph_pub_x(32)|eph_pub_y(32)|nonce(16)|ct(N)]. Returns total len or 0.
static size_t ecies_encrypt(const uint8_t* pt, size_t pt_len,
                            uint8_t* out, size_t out_max) {
  const struct uECC_Curve_t* curve = uECC_secp256r1();
  uECC_set_rng(rng_function);

  uint8_t eph_priv[32], eph_pub[64];            // (r, R = r*G)
  if (!uECC_make_key(eph_pub, eph_priv, curve)) return 0;

  uint8_t pub_recv[64];                         // Q = subscriber public key
  memcpy(pub_recv,      ecc_pub_x, 32);
  memcpy(pub_recv + 32, ecc_pub_y, 32);

  uint8_t shared[32];                           // S.x = (r*Q).x
  if (!uECC_shared_secret(pub_recv, eph_priv, shared, curve)) return 0;

  uint8_t aes_key[16];
  kdf(shared, eph_pub, aes_key);                // eph_pub first 32 bytes = X

  uint8_t nonce[16];
  esp_fill_random(nonce, 16);

  const size_t header = 64 + 16;
  if (header + pt_len > out_max) return 0;
  memcpy(out,      eph_pub, 64);
  memcpy(out + 64, nonce,   16);
  aes_ctr_crypt(aes_key, nonce, pt, out + header, pt_len);
  return header + pt_len;
}
```

El entero aleatorio se obtiene del **TRNG por hardware** del ESP32 (`esp_fill_random`),
garantizando entropía adecuada para las claves efímeras y los *nonces*.

### 4.4 Descifrado en Node-RED — nodo «Descifrado ECC» (`node-red/flows.json`)

El receptor reconstruye la clave pública efímera `R` a partir de los 64 bytes recibidos
(empaquetándola en formato SPKI/DER), calcula el secreto compartido con su clave privada `d`
mediante la primitiva nativa de Node.js **`crypto.diffieHellman`**, deriva la **misma** clave
con la KDF y descifra con **AES-128-CTR**. La clave privada se lee del volumen montado
`/data/ecc_privada.pem` y se cachea en el contexto del flujo.

```javascript
// Requires settings.js -> functionGlobalContext: { crypto, fs }.
// Reads the ECC private key from /data/ecc_privada.pem (mounted volume).
const crypto = global.get('crypto');
const fs     = global.get('fs');

let privKey = flow.get('eccPrivKey');
if (!privKey) {
    const pem = fs.readFileSync('/data/ecc_privada.pem', 'utf8');
    privKey = crypto.createPrivateKey({ key: pem, format: 'pem' });
    flow.set('eccPrivKey', privKey);
}

try {
    const raw   = Buffer.from(msg.payload, 'base64');   // [X|Y|nonce|ct]
    const ephX  = raw.subarray(0, 32);
    const ephY  = raw.subarray(32, 64);
    const nonce = raw.subarray(64, 80);
    const ct    = raw.subarray(80);

    // Rebuild the ephemeral public key R as a KeyObject (SPKI DER).
    const spki = Buffer.from('3059301306072a8648ce3d020106082a8648ce3d030107034200', 'hex');
    const ephPub = crypto.createPublicKey({
        key: Buffer.concat([spki, Buffer.from([0x04]), ephX, ephY]),
        format: 'der', type: 'spki'
    });

    // ECDH: S.x = (d * R).x  — diffieHellman avoids manual scalar extraction.
    const S = crypto.diffieHellman({ privateKey: privKey, publicKey: ephPub });

    // KDF mirrors the firmware exactly: SHA256(S.x || eph_pub_x)[:16].
    const aesKey = crypto.createHash('sha256')
                         .update(Buffer.concat([S, ephX])).digest().subarray(0, 16);

    const decipher = crypto.createDecipheriv('aes-128-ctr', aesKey, nonce);
    const plain = Buffer.concat([decipher.update(ct), decipher.final()]);

    msg.payload = JSON.parse(plain.toString('utf8'));
    return msg;
} catch (e) {
    node.error('ECC decrypt failed: ' + e.message, msg);
    return null;   // drop malformed / forged messages
}
```

La derivación `SHA256(S.x ‖ R.x)` reproduce **byte a byte** la del firmware, por lo que ambos
extremos obtienen la misma clave AES. Los mensajes mal formados o falsificados lanzan una
excepción y se descartan (`return null`).

![Figura 7 — Ciphertext Base64 en el broker](docs/img/evidencia-ciphertext-broker.png)
*Figura 7. Suscripción al tópico `unmsm/iot2025/lab-a/g1/sensores/json_enc`: el broker entrega
únicamente una cadena **Base64 ilegible** (el ciphertext ECIES), evidenciando que solo observa
bytes opacos. Sin la clave privada ECC el contenido no es descifrable.*

![Figura 8 — JSON descifrado por Node-RED](docs/img/evidencia-json-descifrado.png)
*Figura 8. Resultado del nodo «Descifrado ECC»: las mismas lecturas, ya en claro tras aplicar la
clave privada, con los campos `dispositivo`, `temperatura`, `humedad`, `presion_hpa`, `gas_raw`,
`alerta_gas` y `rssi`.*

## 5. Actividad 5 — Persistencia en Google Sheets

El nodo de descifrado entrega el JSON a un nodo de preparación y, de ahí, a un nodo
**HTTP request** que hace **POST** al web app de Apps Script (cuya URL se inyecta por variable
de entorno `APPS_SCRIPT_URL`, nunca *hardcodeada*). La función `doPost` persiste cada lectura.

### 5.1 Endpoint `doPost` — `apps-script/Codigo.gs`

```javascript
const SHEET_ID  = 'PEGAR_ID_DE_LA_HOJA_AQUI';      // from the sheet URL
const SHEET_TAB = 'Datos_IoT_UNMSM_G1';            // tab/pestaña name

function doPost(e) {
  try {
    const data  = JSON.parse(e.postData.contents);
    const sheet = SpreadsheetApp.openById(SHEET_ID).getSheetByName(SHEET_TAB);
    const timestamp = new Date().toISOString();
    sheet.appendRow([
      timestamp,
      data.dispositivo || 'unknown',
      data.temperatura !== undefined ? Number(data.temperatura) : '',
      data.humedad     !== undefined ? Number(data.humedad)     : '',
      data.presion_hpa !== undefined ? Number(data.presion_hpa) : '',
      data.gas_raw     !== undefined ? Number(data.gas_raw)     : '',
      data.alerta_gas  !== undefined ? Boolean(data.alerta_gas) : false,
      data.rssi        !== undefined ? Number(data.rssi)        : ''
    ]);
    const lastRow = sheet.getLastRow();
    sheet.getRange(lastRow, 1).setNumberFormat('yyyy-mm-dd hh:mm:ss');
    return ContentService
      .createTextOutput(JSON.stringify({ status: 'ok', fila: lastRow, timestamp }))
      .setMimeType(ContentService.MimeType.JSON);
  } catch (err) {
    return ContentService
      .createTextOutput(JSON.stringify({ status: 'error', mensaje: err.message }))
      .setMimeType(ContentService.MimeType.JSON);
  }
}
```

### 5.2 Mapeo de columnas

Cada fila persistida en la pestaña `Datos_IoT_UNMSM_G1` tiene **8 columnas**:

| # | Columna | Origen |
|---|---------|--------|
| 1 | Timestamp (ISO 8601) | generado por Apps Script |
| 2 | Dispositivo | `dispositivo` (`esp32-g1-ecc`) |
| 3 | Temperatura (°C) | `temperatura` |
| 4 | Humedad (%) | `humedad` |
| 5 | Presión (hPa) | `presion_hpa` |
| 6 | Gas (RAW, 0–4095) | `gas_raw` |
| 7 | Alerta de gas | `alerta_gas` |
| 8 | RSSI (dBm) | `rssi` |

![Figura 9 — Hoja de cálculo con lecturas reales](docs/img/09-google-sheets-filas.png)
> Captura requerida: la pestaña `Datos_IoT_UNMSM_G1` de Google Sheets con **al menos 25 filas**
> de lecturas reales, mostrando las 8 columnas pobladas y marcas de tiempo consecutivas
> separadas ~10 s.

![Figura 10 — Respuesta del Apps Script confirmando la inserción](docs/img/evidencia-apps-script-respuesta.png)
*Figura 10. Respuesta `{"status":"ok","fila":N, ...}` del web app de Apps Script ante el POST de
Node-RED, confirmando que cada lectura descifrada se persiste como una **nueva fila** en la hoja
`Datos_IoT_UNMSM_G1`.*

## 6. Cuestionario

### MQTT

**1. Diferencia entre publicador y suscriptor. ¿Un mismo ESP32-S3 puede cumplir ambos roles?**

El **publicador** envía mensajes a un tópico sin conocer quién los consumirá; el **suscriptor**
declara interés en uno o más tópicos y recibe los mensajes que se publiquen en ellos. Ambos son
clientes del broker y el patrón los **desacopla** en el tiempo y el espacio. **Sí**, un mismo
ESP32-S3 puede cumplir ambos roles simultáneamente con un único cliente MQTT. Ejemplo concreto
de este laboratorio: un nodo podría **publicar** sus lecturas de gas en
`unmsm/iot2025/lab-a/g1/alarmas/gas` y, a la vez, **suscribirse** a
`unmsm/iot2025/lab-a/g1/actuadores/relay` para accionar un relé local cuando Node-RED ordene la
actuación; así el mismo dispositivo emite telemetría hacia arriba y recibe comandos hacia abajo,
cerrando el lazo de control.

**2. Niveles QoS 0/1/2. ¿Cuál usarías para las alarmas del MQ-2?**

- **QoS 0 (*at most once*):** sin confirmación; puede perderse. Overhead mínimo.
- **QoS 1 (*at least once*):** confirmación con `PUBACK`; garantiza la llegada, pero puede
  duplicar.
- **QoS 2 (*exactly once*):** *handshake* de 4 fases; entrega única garantizada, máximo overhead
  y latencia.

Para las **alarmas del MQ-2** en un sistema de detección de incendios usaría **QoS 1**: en
seguridad de vida es inaceptable **perder** una alarma, y el posible **duplicado** de QoS 1 es
inofensivo (una alarma repetida no causa daño, mientras que una alarma perdida sí). QoS 2
eliminaría duplicados a costa de mayor latencia y tráfico, lo que en una alarma crítica no
compensa frente a la fiabilidad ya lograda con QoS 1. *Observación de implementación:* la
librería `PubSubClient` usada en el firmware publica siempre en **QoS 0**; un sistema de
producción debería migrar a un cliente con soporte de QoS 1 (p. ej. la librería MQTT
asíncrona) para las alarmas.

**3. Función del LWT en un sistema con cifrado ECC. ¿Cifrado o en claro?**

El **LWT** permite al broker anunciar la **caída inesperada** de un nodo: el cliente registra al
conectarse un mensaje (`offline` en `.../status`) que el broker publica si pierde la conexión
sin un *disconnect* limpio. En este proyecto el nodo además publica `online` (retenido) al
conectarse. El mensaje LWT **debe ir en claro**: (a) transporta **metadato operativo de
presencia**, no datos sensibles del sensor; (b) debe ser interpretable por el broker y por
herramientas de monitoreo que **no poseen** la clave privada ECC; y (c) cifrarlo impediría su
propósito de supervisión y añadiría complejidad sin beneficio de confidencialidad. Solo si la
mera presencia/ausencia del nodo fuese información confidencial se justificaría protegerlo.

### Seguridad ECC

**4. Perfect Forward Secrecy (PFS) y por qué ECIES con pares efímeros lo aporta.**

**PFS** es la propiedad por la cual el compromiso del material de clave de **un** mensaje (o de
una clave de largo plazo en el futuro) **no** permite descifrar los mensajes **anteriores**. En
el esquema implementado, **cada mensaje genera un par efímero `(r, R)`** y deriva una clave AES
**única** a partir del secreto ECDH de ese par; la clave efímera privada `r` se **descarta**
inmediatamente tras cifrar. Por tanto, comprometer la clave de sesión de un mensaje **no**
revela las de los demás: cada uno es criptográficamente independiente. En contraste, un esquema
con una **clave AES compartida y fija** carece de PFS: si esa única clave se filtra, **todo** el
histórico —pasado y futuro— queda expuesto de inmediato, porque todos los mensajes se cifraron
con ella. *Consideración rigurosa:* la PFS frente a las **claves efímeras** es completa; además,
debe protegerse la clave privada de **largo plazo** del receptor (ver P5), pues su custodia es
la última línea de defensa del histórico.

**5. ¿Dónde reside la clave privada ECC? ¿Puede un atacante que intercepta MQTT descifrar?**

La **clave privada ECC (`d`)** reside **exclusivamente en el receptor**: el archivo
`ecc_privada.pem`, montado dentro del contenedor de Node-RED en `/data/ecc_privada.pem` sobre la
Raspberry Pi. **Nunca** se transmite ni se almacena en el ESP32, que solo embebe la clave
**pública** `Q`. Un atacante que intercepte el tráfico MQTT entre el ESP32 y el broker observa
únicamente `R.x ‖ R.y ‖ nonce ‖ ciphertext`; para reconstruir el secreto compartido `S = d·R`
necesitaría `d`, cuya obtención a partir de `Q` o de `R` equivale a resolver el **ECDLP**,
computacionalmente inviable en P-256. En consecuencia, **sin acceso a la Raspberry Pi el
atacante no puede descifrar** los mensajes: la confidencialidad se mantiene aunque el broker y la
red estén comprometidos.

**6. (Numérica) Razón de expansión del payload y origen de los bytes extra.**

Se parte de un JSON en claro de tamaño `P` y se calcula el tamaño en el cable según el
**formato real** del firmware:

- **Cabecera ECC (sin Base64):** clave pública efímera **sin comprimir** `R.x ‖ R.y` = **64 B**
  + *nonce* = **16 B** ⇒ **80 B**.
- **Texto cifrado:** AES-128-**CTR** no usa relleno ⇒ `ciphertext = P` bytes.
- **Total binario:** `80 + P`.
- **Base64:** multiplica por **4/3** (≈ +33 %): `⌈(80 + P)/3⌉ · 4`.

Para un JSON de **P = 200 B**: binario `= 80 + 200 = 280 B`; en Base64
`= ⌈280/3⌉·4 = 94·4 = 376 B`. **Razón de expansión ≈ 376 / 200 ≈ 1,9×**. Los bytes adicionales
provienen de: (i) la **cabecera ECC** de 80 B (clave efímera + *nonce*), y (ii) el **+33 % de
Base64**.

*Diferencia teoría vs. implementación (a destacar):* la guía teórica suele asumir el punto
elíptico **comprimido** (33 B). La implementación real transmite el punto **sin comprimir**
(64 B), es decir **31 B extra** por mensaje; usar compresión de punto reduciría la cabecera de
80 B a 49 B. Asimismo, el valor de ~700 B citado en la guía corresponde al **tamaño del búfer de
salida** reservado en el firmware (`char b64[700]`), un límite superior seguro, no al tamaño real
del mensaje, que para estos JSON ronda los ~370–400 B.

### Google Sheets

**7. (Numérica) ¿En cuántos días se alcanza el límite de 10 millones de celdas?**

- Celdas por fila = **8 columnas** ⇒ filas máximas `= 10 000 000 / 8 = 1 250 000 filas`.
- Frecuencia = **1 publicación / 10 s** ⇒ `86 400 / 10 = 8 640 filas/día`.
- **Días hasta el límite** `= 1 250 000 / 8 640 ≈ 144,7 días ≈ 145 días` (≈ 4,8 meses).

**Estrategia de rotación:** crear **una pestaña por mes** (o por semana) y, al cerrar el período,
**archivar** los datos antiguos exportándolos a CSV o a **BigQuery** y vaciar la hoja activa;
adicionalmente, aplicar **submuestreo/agregación** (p. ej. guardar promedios cada 1–5 min en
lugar de cada lectura) reduce drásticamente el crecimiento. Una rutina programada de Apps Script
(*time-driven trigger*) puede automatizar la rotación y el archivado.

**8. (Numérica) Impacto de los límites de Apps Script con 10 ESP32 cada 5 s, y batching.**

- Peticiones diarias `= 10 dispositivos · (86 400 / 5) = 10 · 17 280 = 172 800 ejecuciones/día`.
- Cada POST invoca una **ejecución** de `doPost`. Con ~1–2 s por ejecución, el tiempo agregado
  (~3 000–6 000 min/día) **excede por mucho** la cuota de **90 min/día**, y el número de
  ejecuciones supera también el límite diario de invocaciones de las cuentas gratuitas
  (del orden de 20 000/día). El sistema, tal cual, es **inviable**.
- **Estrategia de batching en Node-RED:** usar un nodo *batch*/*join* para **acumular** las
  lecturas en una ventana temporal (p. ej. 1 min) y/o por dispositivo, y enviar **un solo POST**
  con un **arreglo** de lecturas. `doPost` insertaría el bloque con `getRange().setValues()` en
  vez de múltiples `appendRow`. Agrupando 1 envío/min se pasa de 172 800 a **1 440
  ejecuciones/día**, dentro de la cuota, reduciendo además la latencia de red y el consumo de
  tiempo de ejecución.

### Diseño integral (grupal)

**9. Arquitectura IoT segura para 5 aulas de la FISI-UNMSM.**

Se propone la arquitectura de la Figura 11, con los siguientes componentes:

- **(a) Tópicos MQTT con cifrado diferenciado por aula:** espacio de nombres
  `unmsm/iot2025/fisi/aula-{1..5}/sensores/json_enc`. Cada aula publica su telemetría **cifrada**
  con la **clave pública propia** de esa sala, de modo que el compromiso de una no afecta a las
  demás.
- **(b) Gestión de claves ECC por sala:** un **par ECC P-256 por aula** (`Q_i` pública en el
  ESP32 de la sala, `d_i` privada en el servidor). Las privadas se custodian en un *keystore* del
  servidor con permisos restringidos y se aplica **rotación periódica** de claves.
- **(c) Broker Mosquitto con autenticación:** habilitar `password_file` (usuario/contraseña por
  dispositivo) y **ACL** que restrinjan a cada ESP32 a publicar **solo** en el tópico de su aula,
  más TLS en el puerto 8883 para proteger las credenciales.
- **(d) Flujo Node-RED con descifrado y validación:** un subflujo por aula que selecciona la
  clave `d_i` correspondiente, **descifra**, **valida** el rango y la integridad de los campos y
  descarta mensajes mal formados.
- **(e) Google Sheets con una pestaña por aula:** la hoja contiene las pestañas
  `Aula_1 … Aula_5`; `doPost` enruta cada lectura a la pestaña según el campo de aula.
- **(f) Umbrales de alerta con notificación por correo:** cuando una lectura supera un umbral
  (p. ej. CO₂/gas), Apps Script envía un correo con **`MailApp.sendEmail`** al responsable del
  aula.

![Figura 11 — Arquitectura propuesta para 5 aulas](docs/img/diseno-5-aulas.png)
*Figura 11. Diseño seguro multi-aula: cifrado ECC diferenciado por sala, broker autenticado con
ACL, descifrado y validación en Node-RED, persistencia por pestaña y alertas por correo.*

## 7. Conclusiones

1. **Confidencialidad de extremo a extremo viable en hardware embebido.** Se demostró que un
   ESP32-S3 puede ejecutar ECIES (ECDH P-256 + SHA-256 + AES-128-CTR) y publicar telemetría
   cifrada en tiempo real, con el broker actuando como mero reenviador de **ciphertext**: la
   seguridad no depende de confiar en la infraestructura intermedia.

2. **Perfect Forward Secrecy gracias a las claves efímeras.** El uso de un par efímero por
   mensaje hace que cada lectura tenga una clave de sesión independiente; el compromiso de una
   no expone a las demás, ventaja decisiva frente a un esquema de clave AES fija compartida.

3. **El overhead del cifrado es moderado y predecible.** La cabecera ECC (80 B: clave efímera
   sin comprimir + *nonce*) más el +33 % de Base64 elevan un JSON de ~200 B a ~370–400 B en el
   cable (≈1,9×); el modo CTR evita relleno. La compresión de punto elíptico permitiría reducir
   31 B por mensaje si el ancho de banda fuese crítico.

4. **La elección de QoS debe alinearse con la criticidad del dato.** Para telemetría rutinaria
   QoS 0 es suficiente, pero las **alarmas** exigen al menos QoS 1; el informe evidencia la
   limitación de `PubSubClient` (solo QoS 0) como punto de mejora para escenarios de seguridad de
   vida.

5. **Google Sheets es un almacén válido para prototipos, con límites claros.** Es gratuito y
   accesible, pero su techo de 10 millones de celdas (~145 días a una muestra cada 10 s) y las
   cuotas de Apps Script obligan a estrategias de **rotación** y **batching** en cuanto crece el
   número de nodos o la frecuencia de muestreo.

6. **La separación de claves sustenta la postura de seguridad.** Mantener la clave privada
   **solo** en el receptor (la Raspberry Pi) y publicar únicamente la pública en el dispositivo
   reduce la superficie de ataque: interceptar la red no basta para descifrar.

## 8. Referencias

OASIS. (2019). *MQTT Version 5.0. OASIS Standard*. https://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html

National Institute of Standards and Technology. (2023). *Digital Signature Standard (DSS)* (FIPS PUB 186-5). U.S. Department of Commerce. https://doi.org/10.6028/NIST.FIPS.186-5

Hankerson, D., Menezes, A., & Vanstone, S. (2004). *Guide to Elliptic Curve Cryptography*. Springer.

HiveMQ. (2015). *MQTT Security Fundamentals*. HiveMQ GmbH. https://www.hivemq.com/mqtt-security-fundamentals/

Google. (2024). *Spreadsheet Service | Apps Script*. Google Developers. https://developers.google.com/apps-script/reference/spreadsheet

OpenJS Foundation. (2024). *Node-RED Documentation*. https://nodered.org/docs/

Eclipse Foundation. (2024). *Eclipse Mosquitto: An open source MQTT broker*. https://mosquitto.org/

OpenJS Foundation. (2024). *Crypto | Node.js v20 Documentation*. https://nodejs.org/api/crypto.html

MacKay, K. (2020). *micro-ecc: ECDH and ECDSA for 8-bit, 32-bit, and 64-bit processors* [Software]. GitHub. https://github.com/kmackay/micro-ecc
