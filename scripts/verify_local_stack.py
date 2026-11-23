import subprocess
import time
import urllib.request
import json
import sys
import os

print("[+] Starting RelayForge C++ Engine Core on port 8080...")
cmd = ["./build/relayforge_server", "--port", "8080", "--db", "/tmp/rf_verify.db", "--workers", "4"]
if sys.platform == "win32":
    cmd = ["wsl", "bash", "-c", "cd /mnt/c/Users/Vishe/Downloads/FORGE && ./build/relayforge_server --port 8080 --db /tmp/rf_verify.db --workers 4"]

engine_proc = subprocess.Popen(cmd)
time.sleep(2)

print("[+] Starting FastAPI Gateway on port 8000...")
api_cmd = [sys.executable, "-m", "uvicorn", "api.fast_api_gateway:app", "--port", "8000", "--host", "127.0.0.1"]
api_proc = subprocess.Popen(api_cmd)
time.sleep(2)

def http_get(url):
    req = urllib.request.Request(url)
    with urllib.request.urlopen(req, timeout=5) as resp:
        return resp.status, json.loads(resp.read().decode('utf-8'))

def http_post(url, data):
    body = json.dumps(data).encode('utf-8')
    req = urllib.request.Request(url, data=body, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=5) as resp:
        return resp.status, json.loads(resp.read().decode('utf-8'))

try:
    # 1. Test C++ engine health
    status, d1 = http_get("http://127.0.0.1:8080/health")
    print(f"[1] C++ Core Health: {status} -> {d1}")
    assert status == 200

    # 2. Test FastAPI health
    status, d2 = http_get("http://127.0.0.1:8000/health")
    print(f"[2] FastAPI Gateway Health: {status} -> {d2}")
    assert status == 200

    # 3. Ingest real event via FastAPI
    test_event = {
        "event_type": "order.created",
        "priority": "HIGH",
        "idempotency_key": f"verify_order_{int(time.time()*1000)}",
        "payload": {"order_id": "ORD-VERIFY", "amount": 99.99}
    }
    status, d3 = http_post("http://127.0.0.1:8000/api/v1/events", test_event)
    print(f"[3] FastAPI Event Ingest: {status} -> {d3}")
    assert status in [200, 202]

    time.sleep(1)

    # 4. Fetch real stats via FastAPI
    status, d4 = http_get("http://127.0.0.1:8000/api/v1/stats")
    print(f"[4] FastAPI Stats: {status} -> {d4}")
    processed = d4.get("events_success_total") or d4.get("events_processed_total", 0)
    assert processed >= 1

    print("\n========================================================")
    print("  [SUCCESS] All backend tiers operational and verified! ")
    print("========================================================")
finally:
    print("[+] Stopping background test processes...")
    engine_proc.terminate()
    api_proc.terminate()
    try:
        engine_proc.wait(timeout=2)
        api_proc.wait(timeout=2)
    except Exception:
        engine_proc.kill()
        api_proc.kill()
