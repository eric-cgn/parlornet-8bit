# A Unix shell for testing the console's netcat flow: socat gives each connection a bash on a pty, as a
# login over a serial line would, with nc (OpenBSD's) to reach the parlor. test/README.md has the steps.
FROM alpine:3.22
RUN apk add --no-cache socat netcat-openbsd bash
RUN adduser -D player
USER player
WORKDIR /home/player
ENV PS1='player@shell:~$ '
CMD ["socat", "TCP-LISTEN:2323,reuseaddr,fork", "EXEC:bash --norc -i,pty,stderr,setsid,sigint,sane"]
