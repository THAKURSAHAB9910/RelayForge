// Central API Client for RelayForge
// Interacts with FastAPI Gateway (:8000) or directly with C++ Core Engine (:8080)

const DEFAULT_API_BASE = '/api/v1';
const DEFAULT_ENGINE_BASE = '/engine';

export async function checkHealth(customUrl) {
  const url = customUrl || '';
  try {
    const res = await fetch(`${url}/health`, { signal: AbortSignal.timeout(2000) });
    if (res.ok) {
      return { ok: true, data: await res.json() };
    }
  } catch (err) {
    // Try engine direct health
    try {
      const res2 = await fetch(`${url}${DEFAULT_ENGINE_BASE}/health`, { signal: AbortSignal.timeout(2000) });
      if (res2.ok) {
        return { ok: true, data: await res2.json() };
      }
    } catch (e) {}
  }
  return { ok: false };
}

export async function fetchStats(customUrl) {
  const base = customUrl || DEFAULT_API_BASE;
  try {
    const res = await fetch(`${base}/stats`, { signal: AbortSignal.timeout(2000) });
    if (res.ok) return await res.json();
  } catch (err) {
    // fallback direct to engine
    try {
      const res = await fetch(`${DEFAULT_ENGINE_BASE}/stats`, { signal: AbortSignal.timeout(2000) });
      if (res.ok) return await res.json();
    } catch (e) {}
  }
  return null;
}

export async function ingestEvent(event, customUrl) {
  const base = customUrl || DEFAULT_API_BASE;
  try {
    const res = await fetch(`${base}/events`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(event),
      signal: AbortSignal.timeout(5000)
    });
    return { ok: res.ok, status: res.status, data: await res.json() };
  } catch (err) {
    // Fallback directly to engine core
    try {
      const res = await fetch(`${DEFAULT_ENGINE_BASE}/events`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(event),
        signal: AbortSignal.timeout(5000)
      });
      return { ok: res.ok, status: res.status, data: await res.json() };
    } catch (e) {
      return { ok: false, error: err.message };
    }
  }
}

export async function fetchDLQ(customUrl, limit = 50, offset = 0) {
  const base = customUrl || DEFAULT_API_BASE;
  try {
    const res = await fetch(`${base}/dlq?limit=${limit}&offset=${offset}`, { signal: AbortSignal.timeout(3000) });
    if (res.ok) return await res.json();
  } catch (err) {
    try {
      const res = await fetch(`${DEFAULT_ENGINE_BASE}/dlq?limit=${limit}&offset=${offset}`, { signal: AbortSignal.timeout(3000) });
      if (res.ok) return await res.json();
    } catch (e) {}
  }
  return { events: [], total_count: 0 };
}

export async function replayDLQEvent(eventId, customUrl) {
  const base = customUrl || DEFAULT_API_BASE;
  try {
    const res = await fetch(`${base}/dlq/${eventId}/replay`, { method: 'POST', signal: AbortSignal.timeout(3000) });
    return { ok: res.ok, status: res.status, data: await res.json() };
  } catch (err) {
    try {
      const res = await fetch(`${DEFAULT_ENGINE_BASE}/dlq/${eventId}/replay`, { method: 'POST', signal: AbortSignal.timeout(3000) });
      return { ok: res.ok, status: res.status, data: await res.json() };
    } catch (e) {
      return { ok: false, error: err.message };
    }
  }
}

export async function fetchPrometheusMetrics(customUrl) {
  const base = customUrl || DEFAULT_API_BASE;
  try {
    const res = await fetch(`${base}/metrics`, { signal: AbortSignal.timeout(2000) });
    if (res.ok) return await res.text();
  } catch (err) {
    try {
      const res = await fetch(`${DEFAULT_ENGINE_BASE}/metrics`, { signal: AbortSignal.timeout(2000) });
      if (res.ok) return await res.text();
    } catch (e) {}
  }
  return '';
}
