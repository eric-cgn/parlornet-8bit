# VICE 3.10 for running the C64 program in tests (test/vice/run.sh), built from VICE's upstream source
# tarball, which you download yourself from https://vice-emu.sourceforge.io/ and put beside this file as
# vice-3.10.tar.gz (SHA-256 8e5bac18cbcb9f192380ad3ef881f8790f5b75c41d7b3da65d831985d864d6d1). VICE's own
# source has the C64's and the 1541's ROMs (in its data/ directory); nothing of VICE's is part of this
# repository. The SDL2 interface runs on Xvfb, and the tests drive VICE through its binary monitor.
#   docker build -t parlornet-8bit-vice test/vice
FROM debian:bookworm@sha256:268fb24c39bca92d326dd83ea3abebe4a3f7751c804ffef23bb6d857dd343ff8
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential pkg-config flex bison xa65 dos2unix file \
    libsdl2-dev libsdl2-image-dev libpng-dev zlib1g-dev libcurl4-openssl-dev \
    xvfb xdotool python3 procps \
    && rm -rf /var/lib/apt/lists/*
COPY vice-3.10.tar.gz /usr/src/
RUN cd /usr/src && echo "8e5bac18cbcb9f192380ad3ef881f8790f5b75c41d7b3da65d831985d864d6d1  vice-3.10.tar.gz" | sha256sum -c \
    && tar xzf vice-3.10.tar.gz && cd vice-3.10 \
    && ./configure --enable-sdl2ui --disable-pdf-docs --disable-html-docs --without-pulse --without-alsa \
       --without-oss --disable-ffmpeg --prefix=/usr/local > /tmp/configure.log 2>&1 \
    && make -j8 > /tmp/make.log 2>&1 && make install > /tmp/install.log 2>&1 \
    && cd / && rm -rf /usr/src/vice-3.10 /usr/src/vice-3.10.tar.gz
RUN useradd -m -u 1000 cbm
USER cbm
