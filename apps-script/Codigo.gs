// Google Apps Script — HTTP endpoint that appends decrypted IoT readings from
// Node-RED into a Google Sheet. Deploy: Extensiones > Apps Script, paste this,
// then Implementar > Nueva implementación > Aplicación web
//   (Ejecutar como: Yo | Quién accede: Cualquiera). Copy the /exec URL into .env
//   as APPS_SCRIPT_URL.
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

// Run from the Apps Script editor to test without the ESP32.
function testDoPost() {
  const e = { postData: { contents: JSON.stringify({
    dispositivo: 'esp32-g1-test', temperatura: 24.5, humedad: 62.3,
    presion_hpa: 1012.7, gas_raw: 342, alerta_gas: false, rssi: -65
  })}};
  Logger.log(doPost(e).getContent());
}
