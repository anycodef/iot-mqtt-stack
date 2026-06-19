#!/usr/bin/env bash
# =============================================================================
# ecc-keygen.sh — genera el par de claves P-256 (NIST secp256r1) para el lab ECIES.
#
# Salidas:
#   crypto/ecc_privada.pem  -> clave privada 'd'. La lee Node-RED (suscriptor).
#                              NUNCA se commitea — está en .gitignore.
#   crypto/ecc_publica.pem  -> clave pública 'Q = d*G'. Distribuible.
#   firmware/publisher-ecc/ecc_public_key.h -> header C con Q (X||Y) para el ESP32.
#
# Uso:   ./scripts/ecc-keygen.sh
# Corre UNA sola vez antes del laboratorio. Si regeneras: re-flashear ESP32 y
# reiniciar Node-RED.
# =============================================================================
set -euo pipefail

OUT_DIR="${1:-./crypto}"
HDR_DIR="${2:-./firmware/publisher-ecc}"
mkdir -p "$OUT_DIR" "$HDR_DIR"
PRIV="$OUT_DIR/ecc_privada.pem"
PUB="$OUT_DIR/ecc_publica.pem"
HEADER="$HDR_DIR/ecc_public_key.h"

if [[ -f "$PRIV" ]]; then
  echo "AVISO: $PRIV ya existe. Bórralo a mano si de verdad quieres regenerar." >&2
  exit 1
fi

openssl ecparam -name prime256v1 -genkey -noout -out "$PRIV"
chmod 600 "$PRIV"
openssl ec -in "$PRIV" -pubout -out "$PUB" 2>/dev/null

# Punto sin comprimir = últimos 64 bytes del DER SPKI = X(32) || Y(32).
HEX=$(openssl ec -in "$PRIV" -pubout -outform DER 2>/dev/null \
        | od -An -v -tx1 | tr -d ' \n' | tail -c 128)
X_HEX="${HEX:0:64}"
Y_HEX="${HEX:64:64}"

fmt() { echo "$1" | sed 's/../0x&, /g' | fold -sw 48 | sed 's/^/    /; s/ *$//'; }

cat > "$HEADER" <<EOF
// ecc_public_key.h — AUTO-GENERADO por scripts/ecc-keygen.sh. No editar a mano.
// Clave publica Q (P-256, X||Y sin comprimir) del suscriptor Node-RED.
// Seguro de commitear: es la mitad PUBLICA. La privada nunca sale de la Pi.
#ifndef ECC_PUBLIC_KEY_H
#define ECC_PUBLIC_KEY_H

static const uint8_t ecc_pub_x[32] = {
$(fmt "$X_HEX")
};

static const uint8_t ecc_pub_y[32] = {
$(fmt "$Y_HEX")
};

#endif // ECC_PUBLIC_KEY_H
EOF

echo "OK. Generado:"
echo "  $PRIV   (privada 'd' — solo Node-RED, chmod 600, NO commitear)"
echo "  $PUB   (publica 'Q')"
echo "  $HEADER (header firmware — se #incluye en el .ino)"
