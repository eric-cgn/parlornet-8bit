# The IIgs lab's network: slirp, a SLIP endpoint in user space (no privileges), behind a TCP port that
# MAME's modem line connects to (test/lab/gs.sh). slirp-fullbolt, since Debian's plain slirp exits at once
# on 64-bit machines (its select() timeout is out of range).
FROM debian:trixie-slim@sha256:a72b4433de1cf7d6bae0c4cee74d0547f58601a643b3375f070c51600d63b6b2
RUN apt-get update && apt-get install -y --no-install-recommends slirp socat \
 && rm -rf /var/lib/apt/lists/*
CMD ["socat", "-d", "TCP-LISTEN:2323,reuseaddr,fork", "EXEC:slirp-fullbolt \"mtu 1500\",pty,raw,echo=0"]
