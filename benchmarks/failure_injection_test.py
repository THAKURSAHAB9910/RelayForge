import subprocess
import time
import requests
import os
import signal
import glob

def run_failure_injection_test():
    print("=================================================================")
    print("      RelayForge Failure-Injection & Crash-Recovery Test         ")
    print("=================================================================")

    db_path = "/tmp/rf_failure_injection.db"
    for f in glob.glob(f"{db_path}*"):
        try: os.remove(f)
        except OSError: pass

    port = 8099
    base_url = f"http://127.0.0.1:{port}"

    # 1. Start Engine Server
    print("Starting RelayForge server process...")
    p1 = subprocess.Popen(
        ["./relayforge_server", "--port", str(port), "--db", db_path, "--workers", "4"],
        cwd="/mnt/c/Users/Vishe/Downloads/FORGE/build",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    time.sleep(1.5)

    # Verify health
    r = requests.get(f"{base_url}/health", timeout=3.0)
    assert r.status_code == 200, "Server failed to start"
    print("Server online and healthy.")

    # 2. Ingest 300 events with simulated latency so many are in-flight
    total_events = 300
    print(f"Submitting {total_events} events with 100ms simulated latency...")
    submitted_ids = []
    for i in range(total_events):
        payload = {
            "event_type": "order.created",
            "idempotency_key": f"fail-inj-{i}",
            "priority": "NORMAL",
            "payload": {
                "order_id": f"ord_fail_{i}",
                "amount": 50.0 + i,
                "simulate_latency_ms": 100
            }
        }
        res = requests.post(f"{base_url}/events", json=payload, timeout=5.0)
        assert res.status_code == 202
        submitted_ids.append(res.json()["event_id"])

    # Let workers pick up some events into IN_FLIGHT
    time.sleep(0.3)

    # 3. Simulate Hard Crash (SIGKILL - 9)
    print("Simulating HARD CRASH via SIGKILL (kill -9) while events are IN-FLIGHT...")
    os.kill(p1.pid, signal.SIGKILL)
    p1.wait()
    print("Server violently killed. Process terminated.")

    # Verify server is truly dead
    try:
        requests.get(f"{base_url}/health", timeout=1.0)
        assert False, "Server should be dead!"
    except requests.RequestException:
        print("Confirmed: Server is unreachable.")

    # 4. Restart Server on Same Database
    print("\nRestarting RelayForge on same SQLite WAL database...")
    p2 = subprocess.Popen(
        ["./relayforge_server", "--port", str(port), "--db", db_path, "--workers", "4"],
        cwd="/mnt/c/Users/Vishe/Downloads/FORGE/build",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    time.sleep(1.5)

    r = requests.get(f"{base_url}/health", timeout=3.0)
    assert r.status_code == 200, "Restarted server failed to start"
    print("Restarted server online. Recovery manager reconciled state.")

    # 5. Wait for all recovered events to be processed to COMPLETED
    print("Waiting for all recovered events to be processed to COMPLETED...")
    max_wait = 15.0
    t0 = time.time()
    all_done = False
    completed_total = 0

    while time.time() - t0 < max_wait:
        completed_total = 0
        for eid in submitted_ids:
            try:
                ev = requests.get(f"{base_url}/events/{eid}", timeout=1.0).json()
                if ev.get("status") == "COMPLETED":
                    completed_total += 1
            except Exception:
                pass
        if completed_total >= total_events:
            all_done = True
            break
        time.sleep(0.3)

    stats = requests.get(f"{base_url}/stats", timeout=3.0).json()
    p2_completed = stats.get("events_success_total", 0)
    recovered_count = stats.get("recovery", {}).get("recovered_events_count", 0)

    print(f"Server P1 completed before crash: {total_events - recovered_count}")
    print(f"Server P2 recovered from crash:   {recovered_count}")
    print(f"Server P2 completed post-crash:   {p2_completed}")
    print(f"Total events in COMPLETED status: {completed_total}/{total_events}")

    assert completed_total == total_events, f"Expected {total_events} completed, got {completed_total}"

    # 6. Check a sample of events to confirm valid results and audit trail
    for sample_id in [submitted_ids[0], submitted_ids[len(submitted_ids)//2], submitted_ids[-1]]:
        ev = requests.get(f"{base_url}/events/{sample_id}").json()
        assert ev["status"] == "COMPLETED"
        assert len(ev["audit_trail"]) >= 2
        print(f"Sample Event {sample_id[:8]}... status={ev['status']}, attempts={ev['attempt_count']}, audit_entries={len(ev['audit_trail'])}")

    # 7. Test Idempotency After Recovery
    dup_res = requests.post(f"{base_url}/events", json={
        "event_type": "order.created",
        "idempotency_key": "fail-inj-0",
        "payload": {"order_id": "ord_fail_0", "amount": 50.0}
    })
    assert dup_res.status_code == 200, "Duplicate re-submission should return HTTP 200"
    print("Idempotency deduplication verified post-recovery.")

    # Clean shutdown
    p2.terminate()
    try: p2.wait(timeout=3)
    except subprocess.TimeoutExpired:
        p2.kill()
        p2.wait()

    print("\n=================================================================")
    print("      CRASH RECOVERY & ZERO EVENT LOSS VERIFIED SUCCESSFULLY!    ")
    print("=================================================================")

if __name__ == "__main__":
    run_failure_injection_test()
