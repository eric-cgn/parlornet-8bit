# MAME (Debian's package) for running the client in tests: test/lab/run.sh builds and runs it. The
# Apple IIe's ROMs are not part of this repository or of the image; the player supplies them (A2ROMS).
FROM debian:trixie-slim@sha256:a72b4433de1cf7d6bae0c4cee74d0547f58601a643b3375f070c51600d63b6b2
RUN apt-get update && apt-get install -y --no-install-recommends mame python3 ca-certificates \
 && rm -rf /var/lib/apt/lists/*
