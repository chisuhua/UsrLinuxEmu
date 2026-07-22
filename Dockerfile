FROM ubuntu:22.04

LABEL org.opencontainers.image.title="UsrLinuxEmu"
LABEL org.opencontainers.image.description="User-space Linux kernel emulation for portable GPU driver development"
LABEL org.opencontainers.image.version="1.0.0"
LABEL org.opencontainers.image.source="https://github.com/chisuhua/UsrLinuxEmu"

RUN apt-get update -qq && \
    apt-get install -y -qq --no-install-recommends \
        build-essential \
        cmake \
        g++ \
        gcc \
        git \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

COPY . .

RUN mkdir -p build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc)

WORKDIR /workspace/build

CMD ["ctest", "--output-on-failure"]
