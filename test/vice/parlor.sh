#!/bin/sh
# Starts the C64 lab's parlor: a container named c64-parlor on the Docker network c64lab, which has no way
# out (--internal), with parlord on port 16501 (on the lab network; an empty database), the bot, and the lab's orchestrator
# (test/lab/orch.py) answering the runner's `need` steps through test/work/c64xchg. ParlorNet comes from a
# checkout (PARLORNET), mounted read-only; MOTD=file sets the message of the day (a path in the checkout).
# The bot moves at once; THINK=least,most makes it wait that many seconds first (the deployed bot: 0.5,2).
# The container runs unprivileged, with every capability dropped.
set -eu
: "${PARLORNET:?set PARLORNET to a ParlorNet checkout}"
LAB=$(cd "$(dirname "$0")/../lab" && pwd)
WORK=$(cd "$(dirname "$0")/.." && pwd)/work
mkdir -p "$WORK/c64srv" "$WORK/c64xchg"
rm -f "$WORK/c64srv/lab.db" "$WORK/c64srv/lab.db-wal" "$WORK/c64srv/lab.db-shm" "$WORK/c64xchg/need" "$WORK/c64xchg/steps.txt"
docker network inspect c64lab >/dev/null 2>&1 || docker network create --internal c64lab >/dev/null
docker rm -f c64-parlor >/dev/null 2>&1 || true
docker run -d --name c64-parlor --network c64lab --cap-drop ALL --security-opt no-new-privileges \
	-u "$(id -u):$(id -g)" -e HOME=/tmp -e PYTHONDONTWRITEBYTECODE=1 \
	-v "$PARLORNET:/pn:ro" -v "$LAB:/lab:ro" -v "$WORK/c64srv:/srv" -v "$WORK/c64xchg:/xchg" \
	python:3.13-slim sh -c "cd /pn && python3 -m parlord.frontends --keys /srv/keys add bot > /dev/null 2>&1;
	  python3 -m parlord --host 0.0.0.0 --port 16501 --name labparlor --auth password --signup password --frontend-port 16590 \
	    --keys /srv/keys --db /srv/lab.db --admin-telnet 0 --rate 1000 --burst 1000 ${MOTD:+--motd /pn/$MOTD} \
	    > /srv/core.log 2>&1 &
	  sleep 2
	  python3 -m bot --core 127.0.0.1:16590 --key /srv/keys/bot.key --think ${THINK:-0,0} --pace 0 --poll 0.3 > /srv/bot.log 2>&1 &
	  python3 /lab/orch.py /xchg > /srv/orch.log 2>&1 &
	  wait" >/dev/null
sleep 4
echo "c64-parlor on c64lab: parlord on 16501"
