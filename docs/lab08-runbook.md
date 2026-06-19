# Lab 08 runbook — ECC encryption + Google Sheets

These are the **manual runtime steps** Lab 08 needs that cannot be scripted (key
material, a third-party deployment, and flashing physical hardware). Do them once, in
order. Lab 07 is unaffected — this is purely additive.

## 1. Generate the ECC keypair (once)

From the repo root:

```bash
./scripts/ecc-keygen.sh
```

This creates:

- `crypto/ecc_privada.pem` — the private key `d` (Node-RED only, `chmod 600`, **git-ignored**).
- `crypto/ecc_publica.pem` — the public key `Q` (distributable).
- `firmware/publisher-ecc/ecc_public_key.h` — `Q` as a C header for the ESP32
  (**git-ignored** — keypair-specific, treat like `arduino_secrets.h`).

Regenerating later means re-doing steps 2 and 3.

## 2. Place the private key in the Node-RED data volume

Node-RED reads the key inside the container at `/data/ecc_privada.pem`. The
`${DATA_PATH}/node-red` host directory is bind-mounted to `/data`, so copy it there:

```bash
cp crypto/ecc_privada.pem "${DATA_PATH:-./data}/node-red/ecc_privada.pem"
```

(Run this on the host that runs the containers — the Raspberry Pi in the classroom.)

## 3. Flash the encrypted publisher

```bash
cp firmware/arduino_secrets.example.h firmware/publisher-ecc/arduino_secrets.h
# edit it: Wi-Fi SSID/pass and the Raspberry Pi broker IP
```

Open `firmware/publisher-ecc/publisher-ecc.ino` in the Arduino IDE, confirm the
**micro-ecc** library is installed (see `firmware/README.md`), and upload. The sketch
`#include`s the `ecc_public_key.h` generated in step 1.

## 4. Deploy the Google Apps Script and set the URL

Follow [`../apps-script/README.md`](../apps-script/README.md): create the sheet with the
`Datos_IoT_UNMSM_G1` tab + 8-column header, paste `Codigo.gs`, deploy as a web app
("Cualquiera"), and copy the `/exec` URL into `.env`:

```bash
APPS_SCRIPT_URL=https://script.google.com/macros/s/XXXXXXXX/exec
```

## 5. Bring the stack up

```bash
docker compose up -d
```

(If the stack was already running, `docker compose up -d nodered` is enough to pick up
`settings.js`, the mounted private key, and `APPS_SCRIPT_URL`.)

## Verify

- `docker compose logs -f nodered` — the **Descifrado ECC** node should not error.
- Watch the opaque ciphertext on the wire:

  ```bash
  mosquitto_sub -h <rpi-ip> -t unmsm/iot2025/lab-a/g1/sensores/json_enc -v
  ```

- A new row should appear in the Google Sheet roughly every 10 s while `publisher-ecc`
  runs. The Node-RED **Sheets response** debug node shows the `{ status: 'ok', fila: N }`
  reply.

## Troubleshooting

- **`ENOENT /data/ecc_privada.pem`** → step 2 was skipped or pointed at the wrong
  `DATA_PATH`. The key must land at `/data/ecc_privada.pem` *inside* the container.
- **`ECC decrypt failed`** in the catch debug → the firmware and Node-RED keys don't
  match. Re-run step 1, re-copy the key (step 2), and re-flash (step 3).
- **No rows in the sheet but decryption works** → check `APPS_SCRIPT_URL` in `.env` and
  that the web app is deployed with access "Cualquiera"; the `http request` node must
  follow redirects (it does by default).
