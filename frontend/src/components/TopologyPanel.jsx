import React from 'react';
import { Layers, Database, ListOrdered, Sliders, ShieldCheck } from 'lucide-react';

export default function TopologyPanel({ stats, isConnected }) {
  const util = Math.min(100, Math.round((stats.in_flight_count / 4) * 100));

  return (
    <section className="glass-card rounded-2xl p-5 border border-slate-800">
      <div className="flex items-center justify-between mb-3">
        <h3 className="text-xs font-semibold uppercase tracking-wider text-slate-300 flex items-center gap-2">
          <Layers className="w-4 h-4 text-cyan-400" />
          Subsystem Telemetry &amp; Architecture Topology
        </h3>
        <span className="text-[11px] font-mono text-emerald-400 flex items-center gap-1">
          <span className="w-2 h-2 rounded-full bg-emerald-400 animate-pulse"></span>
          {isConnected ? 'ALL C++ ENGINE SUBSYSTEMS UP' : 'SIMULATION SUBSYSTEMS ACTIVE'}
        </span>
      </div>

      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4 text-xs">
        {/* WAL Storage */}
        <div className="bg-slate-900/60 rounded-xl p-3.5 border border-slate-800">
          <div className="flex items-center justify-between text-slate-400 mb-1">
            <span className="font-medium">SQLite WAL Storage</span>
            <Database className="w-3.5 h-3.5 text-emerald-400" />
          </div>
          <div className="font-mono text-white text-sm font-semibold">journal_mode = WAL</div>
          <div className="text-[11px] text-slate-400 mt-1">synchronous = NORMAL</div>
          <div className="mt-2 text-[11px] text-emerald-400 font-mono">0 corruptions &bull; Crash-Safe</div>
        </div>

        {/* Priority Scheduler */}
        <div className="bg-slate-900/60 rounded-xl p-3.5 border border-slate-800">
          <div className="flex items-center justify-between text-slate-400 mb-1">
            <span className="font-medium">Dual-Queue Scheduler</span>
            <ListOrdered className="w-3.5 h-3.5 text-cyan-400" />
          </div>
          <div className="flex items-center justify-between font-mono text-white text-sm font-semibold">
            <span>Ready: <span className="text-cyan-300">{stats.queue_depth}</span></span>
            <span className="text-amber-400">Delayed: <span>{stats.delayed_queue_depth || 0}</span></span>
          </div>
          <div className="text-[11px] text-slate-400 mt-1">Capacity Bound: 50,000</div>
          <div className="mt-2 text-[11px] text-cyan-400 font-mono">Backpressure Shield Active</div>
        </div>

        {/* Worker Pool */}
        <div className="bg-slate-900/60 rounded-xl p-3.5 border border-slate-800">
          <div className="flex items-center justify-between text-slate-400 mb-1">
            <span className="font-medium">Worker Thread Pool</span>
            <Sliders className="w-3.5 h-3.5 text-purple-400" />
          </div>
          <div className="font-mono text-white text-sm font-semibold">
            <span>{stats.in_flight_count}</span> / 4 Threads Active
          </div>
          <div className="w-full bg-slate-800 rounded-full h-1.5 mt-2 overflow-hidden">
            <div
              className="bg-gradient-to-r from-cyan-500 to-purple-500 h-1.5 rounded-full transition-all duration-300"
              style={{ width: `${util}%` }}
            ></div>
          </div>
          <div className="mt-1 text-[11px] text-purple-300 font-mono">Exception Shields Shielded</div>
        </div>

        {/* Rate Limiter */}
        <div className="bg-slate-900/60 rounded-xl p-3.5 border border-slate-800">
          <div className="flex items-center justify-between text-slate-400 mb-1">
            <span className="font-medium">Token Bucket Limiter</span>
            <ShieldCheck className="w-3.5 h-3.5 text-emerald-400" />
          </div>
          <div className="font-mono text-white text-sm font-semibold">Rate: 50,000 EPS</div>
          <div className="text-[11px] text-slate-400 mt-1">Burst Cap: 100,000 tokens</div>
          <div className="mt-2 text-[11px] text-emerald-400 font-mono">Anti-Spike Protection</div>
        </div>
      </div>
    </section>
  );
}
