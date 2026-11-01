#!/usr/bin/env bash
set -euo pipefail
# First argument is the staged Linux server executable; second is the UDP port.
server_binary="${1:?Usage: bash StartDedicatedServer.sh /path/to/NEXTGENONEServer [7777]}"
server_port="${2:-7777}"
if [[ ! "$server_port" =~ ^[0-9]+$ ]] || (( server_port < 1 || server_port > 65535 )); then
  echo "Invalid UDP port" >&2
  exit 2
fi
exec "$server_binary" /Game/Maps/Gameplay -port="$server_port" -unattended -nosound -log
