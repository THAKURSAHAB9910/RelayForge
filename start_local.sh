#!/usr/bin/env bash
# RelayForge — Local Development Unified Startup Script (Bash)
# Starts C++ Engine Core (:8080), Java Spring Boot Gateway (:8000), and React + Vite (:5173)

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd "$DIR"

GATEWAY="${1:-java}"

echo "================================================================"
echo "   ⚡ RELAYFORGE — FAULT-TOLERANT EVENT PROCESSING ENGINE ⚡   "
echo "   Hybrid Architecture: C++20 Core + Java 21 / Spring Boot 3    "
echo "================================================================"
echo " Starting local development tiers (Gateway: $GATEWAY)..."

# Build C++ if not built
if [ ! -f "cpp-engine/build/relayforge_server" ]; then
    echo "[!] Compiling C++ Core Engine..."
    cmake -B cpp-engine/build -S cpp-engine -DCMAKE_BUILD_TYPE=Release
    cmake --build cpp-engine/build --target relayforge_server --parallel
fi

# Build Java JAR if not packaged
if [ ! -f "java-api/target/relayforge-java-api-1.0.0.jar" ] && [ "$GATEWAY" = "java" ]; then
    echo "[!] Packaging Java Spring Boot Service..."
    cd java-api && mvn clean package -DskipTests && cd ..
fi

cleanup() {
    echo ""
    echo "[RelayForge] Shutting down all background processes..."
    kill $(jobs -p) 2>/dev/null || true
    pkill -f relayforge_server 2>/dev/null || true
    pkill -f relayforge-java-api 2>/dev/null || true
    echo "[RelayForge] Clean shutdown complete."
}
trap cleanup EXIT INT TERM

# 1. Start C++ Core
echo "[1/3] Starting RelayForge C++ Core Engine on :8080..."
./cpp-engine/build/relayforge_server --port 8080 --db relayforge.db --workers 4 &
ENGINE_PID=$!
sleep 2

# 2. Start Application Gateway
if [ "$GATEWAY" = "java" ]; then
    echo "[2/3] Starting Java 21 Spring Boot Gateway on :8000..."
    java -jar java-api/target/relayforge-java-api-1.0.0.jar &
    API_PID=$!
else
    echo "[2/3] Starting FastAPI Gateway on :8000..."
    export RELAYFORGE_BACKEND_URL="http://127.0.0.1:8080"
    python3 -m uvicorn api.fast_api_gateway:app --port 8000 --host 0.0.0.0 &
    API_PID=$!
fi
sleep 3

# 3. Start React + Vite Frontend
echo "[3/3] Starting React + Vite Dev Server on :5173..."
cd frontend
npm run dev &
FRONTEND_PID=$!
cd ..
sleep 2

echo ""
echo "================================================================"
echo "  ✅ ALL SERVICES SUCCESSFULLY RUNNING LOCALLY!                "
echo "================================================================"
echo "  🌐 Frontend React App:       http://localhost:5173"
echo "  ☕ Java Spring Boot Gateway:  http://localhost:8000"
echo "  📖 Swagger Documentation:    http://localhost:8000/swagger-ui.html"
echo "  📋 OpenAPI Specification:    http://localhost:8000/v3/api-docs"
echo "  ⚡ C++ Core Engine REST:     http://localhost:8080"
echo "  📊 Prometheus Metrics:       http://localhost:8080/metrics"
echo "================================================================"
echo " Press [Ctrl+C] to stop all services..."

wait
