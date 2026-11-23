import React from 'react';
import { ArrowDownToDot, CheckCircle2, Cpu, RefreshCw, Skull, Gauge } from 'lucide-react';

export default function MetricsCards({ stats, isConnected }) {
  const successRate = stats.total_ingested > 0
    ? ((stats.total_completed / Math.max(1, stats.total_completed + stats.dead_letter_count)) * 100).toFixed(1)
    : '100.0';

  const workerUtil = Math.min(100, Math.round((stats.in_flight_count / 4) * 100));

  return (
    <div className="space-y-2">
      {/* Real vs Sample Data Badge */}
      <div className="flex items-center justify-between text-xs px-1">
        <span className="text-slate-400 font-mono text-[11px] uppercase tracking-wider">Engine Performance Telemetry</span>
        {isConnected ? (
          <span className="inline-flex items-center gap-1.5 px-2 py-0.5 rounded-full bg-emerald-500/10 text-emerald-400 border border-emerald-500/30 text-[10px] font-mono">
            <span className="w-1.5 h-1.5 rounded-full bg-emerald-400"></span>
            ACTUAL SYSTEM-GENERATED RESULTS
          </span>
        ) : (
          <span className="inline-flex items-center gap-1.5 px-2 py-0.5 rounded-full bg-indigo-500/10 text-indigo-300 border border-indigo-500/30 text-[10px] font-mono">
            <span className="w-1.5 h-1.5 rounded-full bg-indigo-400 animate-pulse"></span>
            SAMPLE / SIMULATED TELEMETRY
          </span>
        )}
      </div>

      <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-6 gap-3.5">
        {/* 1. Total Ingested */}
        <div className="glass-card rounded-xl p-4 flex flex-col justify-between">
          <div className="flex items-center justify-between text-slate-400 mb-1">
            <span className="text-xs uppercase font-medium tracking-wider">Total Ingested</span>
            <ArrowDownToDot className="w-4 h-4 text-emerald-400" />
          </div>
          <div>
            <div className="text-2xl font-bold font-mono text-white">
              {stats.total_ingested.toLocaleString()}
            </div>
            <div className="text-[11px] text-emerald-400 flex items-center gap-1 mt-0.5">
              <span className="font-mono font-medium">{stats.throughput_eps.toFixed(1)}</span> eps
            </div>
          </div>
        </div>

        {/* 2. Completed */}
        <div className="glass-card rounded-xl p-4 flex flex-col justify-between">
          <div className="flex items-center justify-between text-slate-400 mb-1">
            <span className="text-xs uppercase font-medium tracking-wider">Completed</span>
            <CheckCircle2 className="w-4 h-4 text-emerald-400" />
          </div>
          <div>
            <div className="text-2xl font-bold font-mono text-emerald-400">
              {stats.total_completed.toLocaleString()}
            </div>
            <div className="text-[11px] text-slate-400 mt-0.5">
              <span className="font-mono font-medium text-emerald-400">{successRate}%</span> success rate
            </div>
          </div>
        </div>

        {/* 3. In-Flight Workers */}
        <div className="glass-card rounded-xl p-4 flex flex-col justify-between">
          <div className="flex items-center justify-between text-slate-400 mb-1">
            <span className="text-xs uppercase font-medium tracking-wider">In Flight</span>
            <Cpu className="w-4 h-4 text-cyan-400" />
          </div>
          <div>
            <div className="text-2xl font-bold font-mono text-cyan-400">
              {stats.in_flight_count}
            </div>
            <div className="text-[11px] text-slate-400 mt-0.5">
              <span className="font-mono text-cyan-300">{workerUtil}%</span> pool busy
            </div>
          </div>
        </div>

        {/* 4. Retries Scheduled */}
        <div className="glass-card rounded-xl p-4 flex flex-col justify-between">
          <div className="flex items-center justify-between text-slate-400 mb-1">
            <span className="text-xs uppercase font-medium tracking-wider">Retries Scheduled</span>
            <RefreshCw className="w-4 h-4 text-amber-400" />
          </div>
          <div>
            <div className="text-2xl font-bold font-mono text-amber-400">
              {stats.retries_scheduled.toLocaleString()}
            </div>
            <div className="text-[11px] text-amber-400/90 mt-0.5 flex items-center gap-1">
              <span className="w-1.5 h-1.5 rounded-full bg-amber-400 animate-ping"></span>
              Full Jitter Backoff
            </div>
          </div>
        </div>

        {/* 5. Dead Letter Queue */}
        <div className="glass-card rounded-xl p-4 flex flex-col justify-between border-rose-900/30">
          <div className="flex items-center justify-between text-slate-400 mb-1">
            <span className="text-xs uppercase font-medium tracking-wider">Dead Letter (DLQ)</span>
            <Skull className="w-4 h-4 text-rose-400" />
          </div>
          <div>
            <div className="text-2xl font-bold font-mono text-rose-400">
              {stats.dead_letter_count.toLocaleString()}
            </div>
            <div className="text-[11px] text-rose-400/80 mt-0.5">
              Quarantined Poison Pills
            </div>
          </div>
        </div>

        {/* 6. Latency Percentiles */}
        <div className="glass-card rounded-xl p-4 flex flex-col justify-between">
          <div className="flex items-center justify-between text-slate-400 mb-1">
            <span className="text-xs uppercase font-medium tracking-wider">p50 / p99 Latency</span>
            <Gauge className="w-4 h-4 text-purple-400" />
          </div>
          <div>
            <div className="text-xl font-bold font-mono text-purple-300">
              {stats.p50_latency_ms.toFixed(2)}<span className="text-xs text-slate-400">ms</span> / {stats.p99_latency_ms.toFixed(2)}<span className="text-xs text-slate-400">ms</span>
            </div>
            <div className="text-[11px] text-slate-400 mt-0.5">
              Algorithm R Reservoir
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
