# A Unix shell for testing the console's netcat flow and the dialer through a shell: socat gives each
# connection a login shell (Debian's bash 5.2, TERM=vt100) on a pty, as a login over a serial line would,
# with nc (OpenBSD's) to reach the parlor and python3 for ParlorNet's tools/ATZ, which test/README.md
# installs as /usr/local/bin/ATZ (python3, not python3-minimal, which has no termios or tty for it).
# test/README.md has the steps.
FROM debian:bookworm-slim
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
	socat netcat-openbsd python3 && rm -rf /var/lib/apt/lists/*
RUN useradd -m -s /bin/bash player && echo "PS1='player@shell:~\$ '" >> /home/player/.bashrc
USER player
WORKDIR /home/player
CMD ["socat", "TCP-LISTEN:2323,reuseaddr,fork", "EXEC:env TERM=vt100 bash -il,pty,stderr,setsid,sigint,sane,ctty"]
