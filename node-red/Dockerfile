# =============================================================================
# Custom Node-RED image — base 3.1 + node-red-contrib-influxdb (v1 nodes).
#
# Building the contrib node into the image (instead of installing it at runtime
# from the palette) is what makes the stack come up "ready to receive data" on
# the very first `docker compose up -d`, on both amd64 and arm64.
# =============================================================================
FROM nodered/node-red:3.1

# node-red-contrib-influxdb provides the "influxdb out" node used by flows.json.
# Pinned for reproducible builds across the Pi and the dev machine.
RUN npm install --no-audit --no-update-notifier --no-fund \
    node-red-contrib-influxdb@0.7.0
