import os
import sys
import httpx
from typing import Optional, Dict, Any, List
from fastapi import FastAPI, HTTPException, Query, status
from fastapi.responses import PlainTextResponse, JSONResponse, FileResponse
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field

BACKEND_URL = os.getenv("RELAYFORGE_BACKEND_URL", "http://127.0.0.1:8080")

app = FastAPI(
    title="RelayForge API Gateway",
    description="Production-grade fault-tolerant event processing engine REST gateway",
    version="1.0.0"
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

@app.get("/", summary="Live Dashboard", include_in_schema=False)
@app.get("/dashboard", summary="Live Dashboard", include_in_schema=False)
async def serve_dashboard():
    dashboard_path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "dashboard", "index.html")
    if os.path.exists(dashboard_path):
        return FileResponse(dashboard_path)
    return PlainTextResponse("RelayForge Live Dashboard not found at dashboard/index.html")


class EventIngestRequest(BaseModel):
    event_type: str = Field(..., example="order.created")
    payload: Dict[str, Any] = Field(..., example={"order_id": "ord_1001", "amount": 99.50})
    priority: Optional[str] = Field("NORMAL", example="HIGH")
    idempotency_key: Optional[str] = Field(None, example="order-ord_1001-v1")
    max_retries: Optional[int] = Field(3, ge=0, le=20)

class IngestResponse(BaseModel):
    status: int
    event_id: str
    message: str
    http_status_code: int
    event: Optional[Dict[str, Any]] = None

@app.post(
    "/api/v1/events",
    response_model=IngestResponse,
    status_code=status.HTTP_202_ACCEPTED,
    summary="Ingest an event into RelayForge",
    responses={
        200: {"description": "Duplicate request deduplicated via idempotency key"},
        202: {"description": "Event accepted and durably persisted to WAL"},
        400: {"description": "Validation error in event payload"},
        429: {"description": "Rate limit or Backpressure capacity exceeded"}
    }
)
async def ingest_event(event_req: EventIngestRequest):
    async with httpx.AsyncClient(timeout=10.0) as client:
        try:
            resp = await client.post(f"{BACKEND_URL}/events", json=event_req.model_dump())
            return JSONResponse(status_code=resp.status_code, content=resp.json())
        except httpx.ConnectError:
            raise HTTPException(
                status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                detail="RelayForge C++ core engine is unavailable or not running"
            )

@app.get("/api/v1/events/{event_id}", summary="Get event details and audit trail")
async def get_event(event_id: str):
    async with httpx.AsyncClient(timeout=10.0) as client:
        try:
            resp = await client.get(f"{BACKEND_URL}/events/{event_id}")
            if resp.status_code == 404:
                raise HTTPException(status_code=404, detail="Event not found")
            return resp.json()
        except httpx.ConnectError:
            raise HTTPException(status_code=503, detail="RelayForge C++ core engine unavailable")

@app.get("/api/v1/dlq", summary="List dead letter queue events")
async def get_dlq(limit: int = Query(50, ge=1, le=1000), offset: int = Query(0, ge=0)):
    async with httpx.AsyncClient(timeout=10.0) as client:
        try:
            resp = await client.get(f"{BACKEND_URL}/dlq", params={"limit": limit, "offset": offset})
            return resp.json()
        except httpx.ConnectError:
            raise HTTPException(status_code=503, detail="RelayForge C++ core engine unavailable")

@app.post("/api/v1/dlq/{event_id}/replay", summary="Replay a dead letter event")
async def replay_dlq(event_id: str):
    async with httpx.AsyncClient(timeout=10.0) as client:
        try:
            resp = await client.post(f"{BACKEND_URL}/dlq/{event_id}/replay")
            if resp.status_code == 404:
                raise HTTPException(status_code=404, detail="Event not found in DLQ")
            return resp.json()
        except httpx.ConnectError:
            raise HTTPException(status_code=503, detail="RelayForge C++ core engine unavailable")

@app.get("/api/v1/metrics", response_class=PlainTextResponse, summary="Prometheus metrics")
async def get_metrics():
    async with httpx.AsyncClient(timeout=10.0) as client:
        try:
            resp = await client.get(f"{BACKEND_URL}/metrics")
            return PlainTextResponse(resp.text)
        except httpx.ConnectError:
            raise HTTPException(status_code=503, detail="RelayForge C++ core engine unavailable")

@app.get("/api/v1/stats", summary="Runtime statistics (throughput, latencies, worker utilization)")
async def get_stats():
    async with httpx.AsyncClient(timeout=10.0) as client:
        try:
            resp = await client.get(f"{BACKEND_URL}/stats")
            return resp.json()
        except httpx.ConnectError:
            raise HTTPException(status_code=503, detail="RelayForge C++ core engine unavailable")

@app.get("/health", summary="Health check probe")
async def health():
    backend_status = "DOWN"
    backend_info = {}
    try:
        async with httpx.AsyncClient(timeout=3.0) as client:
            resp = await client.get(f"{BACKEND_URL}/health")
            if resp.status_code == 200:
                backend_status = "UP"
                backend_info = resp.json()
    except Exception:
        backend_status = "UNREACHABLE"

    return {
        "gateway": "UP",
        "backend_core": backend_status,
        "backend_url": BACKEND_URL,
        "details": backend_info
    }

if __name__ == "__main__":
    import uvicorn
    port = int(os.getenv("PORT", 8000))
    uvicorn.run("fast_api_gateway:app", host="0.0.0.0", port=port, reload=False)
