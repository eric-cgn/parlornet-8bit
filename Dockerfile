# The toolchain for ParlorNet's 8-bit programs: cc65 2.19 (Debian's package) and Python, on a Debian
# image pinned by digest. tools/build.sh builds it and runs build.sh in it.
FROM debian:trixie-slim@sha256:a72b4433de1cf7d6bae0c4cee74d0547f58601a643b3375f070c51600d63b6b2
RUN apt-get update \
 && apt-get install -y --no-install-recommends cc65=2.19-2 python3-minimal \
 && rm -rf /var/lib/apt/lists/*
