import time
import requests
import concurrent.futures
import statistics
import argparse
import sys

def send_event(base_url, event_index, simulate_failure=False):
    t0 = time.perf_counter()
    payload = {
        "event_type": "order.created" if event_index % 2 == 0 else "payment.process",
        "idempotency_key": f"load-idemp-{event_index}",
        "priority": "HIGH" if event_index % 5 == 0 else "NORMAL",
        "payload": {
            "order_id": f"ord_{event_index}",
            "amount": 10.0 + (event_index % 100),
            "payment_id": f"pay_{event_index}",
            "currency": "USD"
        }
    }
    if simulate_failure and event_index % 10 == 0:
        payload["payload"]["fail_until_attempt"] = 1

    try:
        r = requests.post(f"{base_url}/events", json=payload, timeout=10.0)
        elapsed_ms = (time.perf_counter() - t0) * 1000.0
        return r.status_code, elapsed_ms
    except Exception as e:
        elapsed_ms = (time.perf_counter() - t0) * 1000.0
        return 0, elapsed_ms

def run_load_test(base_url, total_events, concurrency):
    print(f"=== Starting Load Generator ===")
    print(f" Target URL:    {base_url}")
    print(f" Total Events:  {total_events}")
    print(f" Concurrency:   {concurrency}")

    latencies = []
    status_counts = {}

    start_wall = time.perf_counter()

    with concurrent.futures.ThreadPoolExecutor(max_workers=concurrency) as executor:
        futures = [
            executor.submit(send_event, base_url, i)
            for i in range(total_events)
        ]
        for f in concurrent.futures.as_completed(futures):
            status_code, lat_ms = f.result()
            latencies.append(lat_ms)
            status_counts[status_code] = status_counts.get(status_code, 0) + 1

    total_time = time.perf_counter() - start_wall
    throughput = total_events / total_time

    latencies.sort()
    p50 = statistics.median(latencies)
    p95 = latencies[int(len(latencies) * 0.95)]
    p99 = latencies[int(len(latencies) * 0.99)]

    print(f"\n=== Load Generator Results ===")
    print(f" Duration:          {total_time:.2f} s")
    print(f" Ingestion Rate:    {throughput:.2f} events/sec")
    print(f" Status Codes:      {status_counts}")
    print(f" Latency p50:       {p50:.2f} ms")
    print(f" Latency p95:       {p95:.2f} ms")
    print(f" Latency p99:       {p99:.2f} ms")

    # Fetch server stats
    try:
        stats = requests.get(f"{base_url}/stats", timeout=5.0).json()
        print(f"\n=== Server Engine Stats ===")
        print(f" Processed Success: {stats.get('events_success_total')}")
        print(f" Retried:           {stats.get('events_retried_total')}")
        print(f" DLQ:               {stats.get('events_dlq_total')}")
        print(f" Engine p50:        {stats.get('latency_ms', {}).get('p50'):.2f} ms")
        print(f" Engine p95:        {stats.get('latency_ms', {}).get('p95'):.2f} ms")
        print(f" Engine p99:        {stats.get('latency_ms', {}).get('p99'):.2f} ms")
        print(f" Worker Util:       {stats.get('worker_utilization', 0.0) * 100:.1f} %")
    except Exception as e:
        print(f"Could not fetch server stats: {e}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--url", default="http://127.0.0.1:8080")
    parser.add_argument("--events", type=int, default=1000)
    parser.add_argument("--concurrency", type=int, default=16)
    args = parser.parse_args()

    run_load_test(args.url, args.events, args.concurrency)
