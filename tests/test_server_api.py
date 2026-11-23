import subprocess
import time
import requests
import sys

def main():
    import glob
    for f in glob.glob("/tmp/test_api_server.db*"):
        try: os.remove(f)
        except OSError: pass

    print("Starting relayforge_server...")
    proc = subprocess.Popen(
        ["./relayforge_server", "--port", "8086", "--db", "/tmp/test_api_server.db"],
        cwd="/mnt/c/Users/Vishe/Downloads/FORGE/build",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    time.sleep(1.5)

    base = "http://127.0.0.1:8086"
    try:
        # 1. Health
        r = requests.get(f"{base}/health")
        print("Health status:", r.status_code, r.json())
        assert r.status_code == 200

        # 2. Ingest Event
        payload = {
            "event_type": "order.created",
            "idempotency_key": "idemp-http-123",
            "priority": "HIGH",
            "payload": {"order_id": "ord_888", "amount": 149.99}
        }
        r = requests.post(f"{base}/events", json=payload)
        print("Ingest status:", r.status_code, r.json())
        assert r.status_code == 202
        event_id = r.json()["event_id"]

        # Wait for worker execution
        time.sleep(0.5)

        # 3. Query Event
        r = requests.get(f"{base}/events/{event_id}")
        print("Query event status:", r.status_code, r.json()["status"], "result:", r.json()["execution_result"])
        assert r.status_code == 200
        assert r.json()["status"] == "COMPLETED"

        # 4. Duplicate Submission (Idempotency)
        r = requests.post(f"{base}/events", json=payload)
        print("Duplicate ingest status:", r.status_code, r.json()["message"])
        assert r.status_code == 200

        # 5. Stats
        r = requests.get(f"{base}/stats")
        print("Stats:", r.status_code, r.json())
        assert r.status_code == 200

        # 6. Metrics (Prometheus)
        r = requests.get(f"{base}/metrics")
        print("Metrics line count:", len(r.text.splitlines()))
        assert "relayforge_events_received_total" in r.text
        assert "relayforge_events_success_total" in r.text

        print("All server HTTP endpoint tests PASSED!")
    finally:
        print("Terminating server...")
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
        print("Server shutdown completed.")

if __name__ == "__main__":
    main()
