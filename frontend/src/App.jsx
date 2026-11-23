import React, { useState, useEffect, useRef } from 'react';
import Header from './components/Header';
import MetricsCards from './components/MetricsCards';
import ControlDeck from './components/ControlDeck';
import ChartsPanel from './components/ChartsPanel';
import TopologyPanel from './components/TopologyPanel';
import EventTable from './components/EventTable';
import DLQTable from './components/DLQTable';
import PrometheusView from './components/PrometheusView';
import EventDetailModal from './components/EventDetailModal';
import CustomEventModal from './components/CustomEventModal';
import {
  checkHealth,
  fetchStats,
  ingestEvent,
  fetchDLQ,
  replayDLQEvent,
  fetchPrometheusMetrics
} from './api';
import { List, Skull, Terminal } from 'lucide-react';

export default function App() {
  const [isConnected, setIsConnected] = useState(false);
  const [backendSource, setBackendSource] = useState('Checking...');
  const [activeTab, setActiveTab] = useState('stream'); // 'stream' | 'dlq' | 'prom'

  // Telemetry Counters
  const [stats, setStats] = useState({
    total_ingested: 0,
    total_completed: 0,
    in_flight_count: 0,
    retries_scheduled: 0,
    dead_letter_count: 0,
    queue_depth: 0,
    delayed_queue_depth: 0,
    p50_latency_ms: 0.05,
    p95_latency_ms: 0.15,
    p99_latency_ms: 1.25,
    throughput_eps: 0.0,
  });

  // Chart History
  const [throughputHistory, setThroughputHistory] = useState(Array(30).fill(0));
  const [latencyHistory, setLatencyHistory] = useState({
    p50: Array(30).fill(0.05),
    p95: Array(30).fill(0.18),
    p99: Array(30).fill(1.2)
  });

  // Events & DLQ lists
  const [events, setEvents] = useState([]);
  const [dlqEvents, setDlqEvents] = useState([]);
  const [metricsText, setMetricsText] = useState('');

  // Modals
  const [selectedEvent, setSelectedEvent] = useState(null);
  const [isCustomModalOpen, setIsCustomModalOpen] = useState(false);

  const prevCompletedRef = useRef(0);
  const prevTimeRef = useRef(Date.now());

  // Initial seed if empty
  useEffect(() => {
    seedInitialDemoData();
  }, []);

  // Polling loop
  useEffect(() => {
    const poll = async () => {
      // 1. Health check
      const health = await checkHealth();
      if (health.ok) {
        setIsConnected(true);
        setBackendSource(health.data.gateway ? 'FastAPI Gateway (:8000)' : 'C++ Core Engine (:8080)');

        // 2. Real stats
        const realStats = await fetchStats();
        if (realStats) {
          const now = Date.now();
          const elapsedSec = (now - prevTimeRef.current) / 1000;
          const completedCount = realStats.events_success_total ?? realStats.events_processed_total ?? 0;
          const receivedCount = realStats.events_received_total ?? 0;
          const deltaCompleted = completedCount - prevCompletedRef.current;
          const currentThroughput = realStats.throughput_eps ?? (elapsedSec > 0 && deltaCompleted >= 0 ? deltaCompleted / elapsedSec : 0);

          prevCompletedRef.current = completedCount;
          prevTimeRef.current = now;

          const p50 = realStats.latency_ms?.p50 ?? realStats.latency_p50_ms ?? 0.05;
          const p95 = realStats.latency_ms?.p95 ?? realStats.latency_p95_ms ?? 0.15;
          const p99 = realStats.latency_ms?.p99 ?? realStats.latency_p99_ms ?? 1.25;

          const updated = {
            total_ingested: receivedCount,
            total_completed: completedCount,
            in_flight_count: realStats.in_flight_count || 0,
            retries_scheduled: realStats.events_retried_total || 0,
            dead_letter_count: realStats.events_dlq_total ?? realStats.events_dead_letter_total ?? 0,
            queue_depth: realStats.queue_depth || 0,
            delayed_queue_depth: realStats.delayed_queue_depth || 0,
            p50_latency_ms: p50,
            p95_latency_ms: p95,
            p99_latency_ms: p99,
            throughput_eps: currentThroughput
          };

          setStats(updated);
          updateChartHistory(currentThroughput, updated.p50_latency_ms, updated.p95_latency_ms, updated.p99_latency_ms);
        }

        // 3. Real DLQ
        const dlqData = await fetchDLQ();
        if (dlqData && dlqData.events) {
          setDlqEvents(dlqData.events);
        }

        // 4. Prometheus metrics text
        const promText = await fetchPrometheusMetrics();
        if (promText) {
          setMetricsText(promText);
        }
      } else {
        setIsConnected(false);
        setBackendSource('Offline');
        runSimulationStep();
      }
    };

    poll();
    const interval = setInterval(poll, 1000);
    return () => clearInterval(interval);
  }, []);

  const updateChartHistory = (tp, p50, p95, p99) => {
    setThroughputHistory(prev => [...prev.slice(1), tp]);
    setLatencyHistory(prev => ({
      p50: [...prev.p50.slice(1), p50],
      p95: [...prev.p95.slice(1), p95],
      p99: [...prev.p99.slice(1), p99],
    }));
  };

  const seedInitialDemoData = () => {
    const initial = [
      {
        id: generateUUID(),
        idempotency_key: 'ord_init_101',
        event_type: 'order.created',
        priority: 'HIGH',
        status: 'COMPLETED',
        attempt_count: 1,
        max_retries: 3,
        created_at: new Date(Date.now() - 15000).toLocaleTimeString(),
        duration_ms: '0.048',
        payload: { order_id: 'ORD-9001', amount: 99.95, items: [{ sku: 'PRO-10' }] },
        audit: [
          { to: 'PENDING', time: '10:00:01', reason: 'Persisted to SQLite WAL' },
          { to: 'IN_FLIGHT', time: '10:00:01', reason: 'Claimed by Worker #1' },
          { to: 'COMPLETED', time: '10:00:01', reason: 'Order validated and reserved' }
        ]
      },
      {
        id: generateUUID(),
        idempotency_key: 'pay_init_802',
        event_type: 'payment.process',
        priority: 'CRITICAL',
        status: 'COMPLETED',
        attempt_count: 1,
        max_retries: 4,
        created_at: new Date(Date.now() - 10000).toLocaleTimeString(),
        duration_ms: '0.054',
        payload: { txn_id: 'TX-7788', amount: 350.00, gateway: 'Stripe' },
        audit: [
          { to: 'PENDING', time: '10:00:05', reason: 'Persisted to SQLite WAL' },
          { to: 'IN_FLIGHT', time: '10:00:05', reason: 'Claimed by Worker #2' },
          { to: 'COMPLETED', time: '10:00:05', reason: 'Payment captured' }
        ]
      }
    ];

    setEvents(initial);
    setStats(prev => ({
      ...prev,
      total_ingested: 2,
      total_completed: 2
    }));
  };

  const runSimulationStep = () => {
    setEvents(prev => {
      let completedInc = 0;
      const updated = prev.map(ev => {
        if (ev.status === 'IN_FLIGHT') {
          completedInc++;
          return {
            ...ev,
            status: 'COMPLETED',
            duration_ms: (Math.random() * 0.12 + 0.04).toFixed(3),
            audit: [...(ev.audit || []), { to: 'COMPLETED', time: new Date().toLocaleTimeString(), reason: 'Completed by virtual worker' }]
          };
        }
        if (ev.status === 'PENDING') {
          return {
            ...ev,
            status: 'IN_FLIGHT',
            attempt_count: (ev.attempt_count || 0) + 1,
            audit: [...(ev.audit || []), { to: 'IN_FLIGHT', time: new Date().toLocaleTimeString(), reason: 'Claimed by virtual worker' }]
          };
        }
        if (ev.status === 'RETRY_SCHEDULED') {
          const rem = (ev.backoff_rem || 2) - 1;
          if (rem <= 0) {
            return {
              ...ev,
              status: 'PENDING',
              backoff_rem: 0,
              audit: [...(ev.audit || []), { to: 'PENDING', time: new Date().toLocaleTimeString(), reason: 'Retry timer expired' }]
            };
          }
          return { ...ev, backoff_rem: rem };
        }
        return ev;
      });

      if (completedInc > 0) {
        setStats(s => {
          const newCompleted = s.total_completed + completedInc;
          const newInFlight = Math.max(0, s.in_flight_count - completedInc);
          return {
            ...s,
            total_completed: newCompleted,
            in_flight_count: newInFlight
          };
        });
      }
      return updated;
    });
  };

  const handleDispatchPreset = async (type) => {
    const id = generateUUID();
    let evObj = null;

    if (type === 'order') {
      evObj = {
        event_type: 'order.created',
        priority: 'HIGH',
        idempotency_key: `ord_tx_${Date.now().toString().slice(-6)}`,
        max_retries: 3,
        payload: { order_id: `ORD-${Date.now().toString().slice(-4)}`, amount: 149.95 }
      };
    } else if (type === 'payment') {
      evObj = {
        event_type: 'payment.process',
        priority: 'CRITICAL',
        idempotency_key: `pay_tx_${Date.now().toString().slice(-6)}`,
        max_retries: 4,
        payload: { transaction_id: `TX-${Date.now().toString().slice(-4)}`, amount: 450.00 }
      };
    } else if (type === 'inventory') {
      evObj = {
        event_type: 'inventory.reserve',
        priority: 'NORMAL',
        idempotency_key: `inv_tx_${Date.now().toString().slice(-6)}`,
        max_retries: 3,
        payload: { sku: 'SKU-FORGE-88', qty: 10 }
      };
    } else if (type === 'notification') {
      evObj = {
        event_type: 'notification.send',
        priority: 'LOW',
        idempotency_key: `notif_tx_${Date.now().toString().slice(-6)}`,
        max_retries: 2,
        payload: { channel: 'EMAIL', recipient: 'dev@relayforge.io' }
      };
    } else if (type === 'failure') {
      evObj = {
        event_type: 'payment.process',
        priority: 'HIGH',
        idempotency_key: `fail_tx_${Date.now().toString().slice(-6)}`,
        max_retries: 3,
        payload: { simulate_failure: true, reason: 'Downstream HTTP 504 Timeout' }
      };
    } else if (type === 'poison') {
      evObj = {
        event_type: 'order.created',
        priority: 'CRITICAL',
        idempotency_key: `poison_tx_${Date.now().toString().slice(-6)}`,
        max_retries: 2,
        payload: { poison_pill: true, reason: 'Irrecoverable schema corruption invariant' }
      };
    }

    // Call API if connected
    if (isConnected) {
      await ingestEvent(evObj);
    }

    // Add to local state
    addEventToFeed(evObj, type === 'failure', type === 'poison');
  };

  const addEventToFeed = (evObj, isFailure, isPoison) => {
    const timeStr = new Date().toLocaleTimeString();
    const id = generateUUID();

    const record = {
      id,
      ...evObj,
      created_at: timeStr,
      attempt_count: 0,
      duration_ms: '--',
      audit: [{ to: 'PENDING', time: timeStr, reason: 'Accepted & persisted to SQLite WAL' }]
    };

    if (isPoison) {
      record.status = 'DEAD_LETTER';
      record.attempt_count = record.max_retries;
      record.last_error = evObj.payload.reason || 'Fatal poison pill';
      record.audit.push({ to: 'DEAD_LETTER', time: timeStr, reason: 'Exhausted retries into DLQ' });

      setDlqEvents(prev => [
        {
          event_id: id,
          event_type: record.event_type,
          last_error: record.last_error,
          attempt_count: record.attempt_count,
          created_at: timeStr,
          payload: record.payload
        },
        ...prev
      ]);

      setStats(s => ({
        ...s,
        total_ingested: s.total_ingested + 1,
        dead_letter_count: s.dead_letter_count + 1
      }));
    } else if (isFailure) {
      record.status = 'RETRY_SCHEDULED';
      record.attempt_count = 1;
      record.backoff_rem = 3;
      record.last_error = 'Transient HTTP 504 Gateway Timeout';
      record.audit.push({ to: 'RETRY_SCHEDULED', time: timeStr, reason: 'Transient error caught: Full Jitter backoff' });

      setStats(s => ({
        ...s,
        total_ingested: s.total_ingested + 1,
        retries_scheduled: s.retries_scheduled + 1,
        delayed_queue_depth: (s.delayed_queue_depth || 0) + 1
      }));
    } else {
      record.status = 'PENDING';
      setStats(s => ({
        ...s,
        total_ingested: s.total_ingested + 1,
        queue_depth: s.queue_depth + 1
      }));
    }

    setEvents(prev => [record, ...prev.slice(0, 99)]);
  };

  const handleBurst = (count) => {
    const types = ['order', 'payment', 'inventory', 'notification'];
    for (let i = 0; i < count; i++) {
      setTimeout(() => {
        const t = types[Math.floor(Math.random() * types.length)];
        handleDispatchPreset(t);
      }, i * 20);
    }
  };

  const handleReplayDLQ = async (eventId) => {
    if (isConnected) {
      await replayDLQEvent(eventId);
    }

    setDlqEvents(prev => prev.filter(e => (e.event_id || e.id) !== eventId));
    setEvents(prev => prev.map(e => {
      if (e.id === eventId) {
        return {
          ...e,
          status: 'PENDING',
          attempt_count: 0,
          audit: [...(e.audit || []), { to: 'PENDING', time: new Date().toLocaleTimeString(), reason: 'Replayed via Admin DLQ API' }]
        };
      }
      return e;
    }));

    setStats(s => ({
      ...s,
      dead_letter_count: Math.max(0, s.dead_letter_count - 1),
      queue_depth: s.queue_depth + 1
    }));
  };

  const handleReplayAllDLQ = () => {
    dlqEvents.forEach(e => handleReplayDLQ(e.event_id || e.id));
  };

  const handleResetData = () => {
    setEvents([]);
    setDlqEvents([]);
    setStats({
      total_ingested: 0,
      total_completed: 0,
      in_flight_count: 0,
      retries_scheduled: 0,
      dead_letter_count: 0,
      queue_depth: 0,
      delayed_queue_depth: 0,
      p50_latency_ms: 0.05,
      p95_latency_ms: 0.15,
      p99_latency_ms: 1.25,
      throughput_eps: 0.0,
    });
  };

  return (
    <div className="min-h-screen bg-[#090d16] text-slate-100 flex flex-col font-sans">
      <Header
        isConnected={isConnected}
        backendSource={backendSource}
        onRefresh={() => checkHealth()}
      />

      <main className="flex-1 max-w-7xl w-full mx-auto px-4 lg:px-8 py-6 space-y-6">
        {/* Metric KPI Cards */}
        <MetricsCards stats={stats} isConnected={isConnected} />

        {/* Interactive Controls */}
        <ControlDeck
          onDispatch={handleDispatchPreset}
          onBurst={handleBurst}
          onOpenCustomModal={() => setIsCustomModalOpen(true)}
          onResetData={handleResetData}
        />

        {/* Real-time Charts */}
        <ChartsPanel
          throughputHistory={throughputHistory}
          latencyHistory={latencyHistory}
        />

        {/* Subsystem Topology Matrix */}
        <TopologyPanel stats={stats} isConnected={isConnected} />

        {/* Tabs: Stream, DLQ, Prometheus */}
        <section className="glass-card rounded-2xl border border-slate-800 overflow-hidden">
          <div className="flex items-center border-b border-slate-800 px-5 pt-3 gap-6 text-xs font-semibold">
            <button
              onClick={() => setActiveTab('stream')}
              className={`pb-3 flex items-center gap-1.5 transition ${
                activeTab === 'stream'
                  ? 'border-b-2 border-emerald-400 text-white'
                  : 'text-slate-400 hover:text-white'
              }`}
            >
              <List className="w-4 h-4 text-emerald-400" />
              <span>Live Event Stream</span>
              <span className="ml-1 px-1.5 py-0.2 rounded-full bg-slate-800 text-[10px] text-slate-300 font-mono">
                {events.length}
              </span>
            </button>

            <button
              onClick={() => setActiveTab('dlq')}
              className={`pb-3 flex items-center gap-1.5 transition ${
                activeTab === 'dlq'
                  ? 'border-b-2 border-rose-400 text-white'
                  : 'text-slate-400 hover:text-white'
              }`}
            >
              <Skull className="w-4 h-4 text-rose-400" />
              <span>Dead Letter Queue (DLQ)</span>
              <span className="ml-1 px-1.5 py-0.2 rounded-full bg-rose-950/80 border border-rose-800 text-[10px] text-rose-300 font-mono">
                {dlqEvents.length}
              </span>
            </button>

            <button
              onClick={() => setActiveTab('prom')}
              className={`pb-3 flex items-center gap-1.5 transition ${
                activeTab === 'prom'
                  ? 'border-b-2 border-purple-400 text-white'
                  : 'text-slate-400 hover:text-white'
              }`}
            >
              <Terminal className="w-4 h-4 text-purple-400" />
              <span>Prometheus Metrics (/metrics)</span>
            </button>
          </div>

          <div className="p-4">
            {activeTab === 'stream' && (
              <EventTable events={events} onSelectEvent={setSelectedEvent} />
            )}
            {activeTab === 'dlq' && (
              <DLQTable
                dlqEvents={dlqEvents}
                onReplay={handleReplayDLQ}
                onReplayAll={handleReplayAllDLQ}
              />
            )}
            {activeTab === 'prom' && (
              <PrometheusView metricsText={metricsText} />
            )}
          </div>
        </section>
      </main>

      {/* Modals */}
      <EventDetailModal
        event={selectedEvent}
        onClose={() => setSelectedEvent(null)}
      />

      <CustomEventModal
        isOpen={isCustomModalOpen}
        onClose={() => setIsCustomModalOpen(false)}
        onSubmit={(ev) => handleDispatchPreset('custom', ev)}
      />
    </div>
  );
}

function generateUUID() {
  return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function (c) {
    const r = (Math.random() * 16) | 0;
    const v = c === 'x' ? r : (r & 0x3) | 0x8;
    return v.toString(16);
  });
}
