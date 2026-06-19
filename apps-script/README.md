# Apps Script — Google Sheets endpoint (Lab 08)

[`Codigo.gs`](./Codigo.gs) is a Google Apps Script **web app** that receives the
decrypted IoT readings from Node-RED (Flow 3) over HTTP `POST` and appends one row per
reading to a Google Sheet. The timestamp is generated **server-side** (`new Date()`), so
the ESP32 never needs a clock.

## Sheet schema

One tab named exactly **`Datos_IoT_UNMSM_G1`**, with this header row in row 1:

| A | B | C | D | E | F | G | H |
|---|---|---|---|---|---|---|---|
| Timestamp (ISO) | Dispositivo | Temperatura (°C) | Humedad (%) | Presión_hPa | Gas_RAW | Alerta_Gas | RSSI_dBm |

## Deploy (one time)

1. **Create the spreadsheet.** New Google Sheet → rename the tab to
   `Datos_IoT_UNMSM_G1` → add the 8-column header row above. Copy the **sheet ID** from
   the URL (`https://docs.google.com/spreadsheets/d/<SHEET_ID>/edit`).
2. **Open the script editor.** In the sheet: **Extensiones → Apps Script**. Delete the
   placeholder and paste the contents of [`Codigo.gs`](./Codigo.gs).
3. **Set the constants** at the top: paste your `SHEET_ID`; leave `SHEET_TAB` as
   `Datos_IoT_UNMSM_G1` (or match your tab name).
4. **(Optional) Test** without the ESP32: run the `testDoPost` function from the editor
   and check **Ver → Registros** — a row should appear in the sheet.
5. **Deploy as a web app.** **Implementar → Nueva implementación → Aplicación web**:
   - *Ejecutar como:* **Yo** (your account).
   - *Quién tiene acceso:* **Cualquiera** (so Node-RED can POST unauthenticated).
   - Deploy and authorize. Copy the **`/exec`** URL.
6. **Wire it into the stack.** Put the URL in your `.env` (git-ignored):

   ```bash
   APPS_SCRIPT_URL=https://script.google.com/macros/s/XXXXXXXX/exec
   ```

   then restart Node-RED so it picks up the env var:

   ```bash
   docker compose up -d nodered
   ```

> The web app redirects (HTTP 302) to `script.googleusercontent.com` before returning
> its JSON response — that's why the Node-RED `http request` node **follows redirects**.
