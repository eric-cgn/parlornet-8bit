#!/bin/sh
# Starts a lab parlor for the tests from a ParlorNet checkout (PARLORNET), with an empty database, the
# bot, and what MAME's serial line reaches: a bridge on port 16601, relayed to the parlor's port 16501,
# and a WiFi-modem stand-in on port 16600 that answers AT commands and connects on ATDT.
# With MOTD=file the parlor shows that message of the day (a ParlorNet checkout's deploy/motd.txt is the
# live one). The bot moves at once; THINK=least,most makes it wait that many seconds first, as the deployed
# bot does (0.5,2). State and logs go to test/work/srv. Stop them with: pkill -f "parlord --port 16501"; pkill -f bridge.py
set -eu
: "${PARLORNET:?set PARLORNET to a ParlorNet checkout}"
LAB=$(cd "$(dirname "$0")" && pwd)
SRV=$(dirname "$LAB")/work/srv
mkdir -p "$SRV"
pkill -f "parlord --port 16501" 2>/dev/null || true
pkill -f "bot --core 127.0.0.1:16590" 2>/dev/null || true
pkill -f "$LAB/bridge.py" 2>/dev/null || true
sleep 1
rm -f "$SRV/lab.db" "$SRV/lab.db-wal" "$SRV/lab.db-shm"
cd "$PARLORNET"
# The bot is a server extension: its key in the core's key directory (kept from one run to the next).
python3 -m parlord.frontends --keys "$SRV/keys" add bot > /dev/null
nohup python3 -m parlord --port 16501 --name labparlor --auth password --signup password \
	--frontend-port 16590 --keys "$SRV/keys" --db "$SRV/lab.db" --admin-telnet 0 --rate 1000 --burst 1000 \
	${MOTD:+--motd "$MOTD"} > "$SRV/core.log" 2>&1 &
sleep 2
nohup python3 -m bot --core 127.0.0.1:16590 --key "$SRV/keys/bot.key" --think ${THINK:-0,0} --pace 0 --poll 0.3 \
	> "$SRV/bot.log" 2>&1 &
nohup python3 "$LAB/bridge.py" 16601 127.0.0.1 16501 bridge "$SRV/bridge.log" > /dev/null 2>&1 &
nohup python3 "$LAB/bridge.py" 16600 127.0.0.1 16501 modem "$SRV/modem.log" > /dev/null 2>&1 &
sleep 1
echo "lab parlor on 16501; a bridge on 16601 and a modem (ATDT) on 16600"
