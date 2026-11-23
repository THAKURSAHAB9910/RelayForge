# Stage 1: Build environment
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libsqlite3-dev \
    libcpp-httplib-dev \
    nlohmann-json3-dev \
    libgtest-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY CMakeLists.txt ./
COPY include/ ./include/
COPY src/ ./src/
COPY tests/ ./tests/
COPY benchmarks/ ./benchmarks/

RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc) relayforge_server relayforge_benchmark

# Stage 2: Minimal runtime environment
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    libsqlite3-0 \
    libcpp-httplib0.14t64 \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/relayforge

COPY --from=builder /app/build/relayforge_server /usr/local/bin/relayforge_server
COPY --from=builder /app/build/relayforge_benchmark /usr/local/bin/relayforge_benchmark

# Data directory for persistent SQLite WAL storage
RUN mkdir -p /data
VOLUME ["/data"]

EXPOSE 8080

HEALTHCHECK --interval=5s --timeout=3s --retries=3 \
    CMD curl -f http://127.0.0.1:8080/health || exit 1

ENTRYPOINT ["relayforge_server"]
CMD ["--port", "8080", "--db", "/data/relayforge.db", "--workers", "4"]
