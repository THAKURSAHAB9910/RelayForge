import React from 'react';
import { Play, ShoppingCart, CreditCard, Package, Bell, AlertTriangle, Skull, Activity, Code, RotateCcw } from 'lucide-react';

export default function ControlDeck({ onDispatch, onBurst, onOpenCustomModal, onResetData }) {
  return (
    <section className="glass-card rounded-2xl p-5 border border-slate-800">
      <div className="flex flex-col md:flex-row md:items-center justify-between gap-4 mb-4">
        <div>
          <h2 className="text-sm font-semibold tracking-wide uppercase text-slate-300 flex items-center gap-2">
            <Play className="w-4 h-4 text-emerald-400 fill-emerald-400" />
            Interactive Event Dispatcher &amp; Resilience Controls
          </h2>
          <p className="text-xs text-slate-400 mt-0.5">
            Dispatch real domain events, induce transient network failures, or inject poison pills to observe WAL and DLQ mechanisms.
          </p>
        </div>
        <div className="flex items-center gap-2">
          <button
            onClick={onOpenCustomModal}
            className="px-3 py-1.5 rounded-lg bg-slate-800 hover:bg-slate-700 text-xs font-medium text-slate-200 border border-slate-700 flex items-center gap-1.5 transition"
          >
            <Code className="w-3.5 h-3.5" />
            <span>Custom JSON</span>
          </button>
          <button
            onClick={onResetData}
            className="px-3 py-1.5 rounded-lg bg-slate-800 hover:bg-rose-950/40 text-xs font-medium text-slate-400 hover:text-rose-400 border border-slate-700 hover:border-rose-800 flex items-center gap-1 transition"
          >
            <RotateCcw className="w-3.5 h-3.5" />
            <span>Reset Feed</span>
          </button>
        </div>
      </div>

      {/* Preset Action Buttons Grid */}
      <div className="grid grid-cols-2 sm:grid-cols-3 lg:grid-cols-6 gap-2.5">
        <button
          onClick={() => onDispatch('order')}
          className="flex flex-col items-center justify-center p-3 rounded-xl bg-slate-800/80 hover:bg-emerald-950/40 border border-slate-700 hover:border-emerald-600/50 text-center transition group"
        >
          <ShoppingCart className="w-5 h-5 text-emerald-400 group-hover:scale-110 transition mb-1.5" />
          <span className="text-xs font-semibold text-white">Order Event</span>
          <span className="text-[10px] text-emerald-400/80 font-mono">HIGH Priority</span>
        </button>

        <button
          onClick={() => onDispatch('payment')}
          className="flex flex-col items-center justify-center p-3 rounded-xl bg-slate-800/80 hover:bg-purple-950/40 border border-slate-700 hover:border-purple-600/50 text-center transition group"
        >
          <CreditCard className="w-5 h-5 text-purple-400 group-hover:scale-110 transition mb-1.5" />
          <span className="text-xs font-semibold text-white">Payment Event</span>
          <span className="text-[10px] text-purple-400/80 font-mono">CRITICAL Priority</span>
        </button>

        <button
          onClick={() => onDispatch('inventory')}
          className="flex flex-col items-center justify-center p-3 rounded-xl bg-slate-800/80 hover:bg-blue-950/40 border border-slate-700 hover:border-blue-600/50 text-center transition group"
        >
          <Package className="w-5 h-5 text-blue-400 group-hover:scale-110 transition mb-1.5" />
          <span className="text-xs font-semibold text-white">Inventory Reserve</span>
          <span className="text-[10px] text-blue-400/80 font-mono">NORMAL Priority</span>
        </button>

        <button
          onClick={() => onDispatch('notification')}
          className="flex flex-col items-center justify-center p-3 rounded-xl bg-slate-800/80 hover:bg-slate-700/60 border border-slate-700 hover:border-slate-500 text-center transition group"
        >
          <Bell className="w-5 h-5 text-slate-300 group-hover:scale-110 transition mb-1.5" />
          <span className="text-xs font-semibold text-white">Notification</span>
          <span className="text-[10px] text-slate-400 font-mono">LOW Priority</span>
        </button>

        <button
          onClick={() => onDispatch('failure')}
          className="flex flex-col items-center justify-center p-3 rounded-xl bg-slate-800/80 hover:bg-amber-950/40 border border-slate-700 hover:border-amber-600/50 text-center transition group"
        >
          <AlertTriangle className="w-5 h-5 text-amber-400 group-hover:scale-110 transition mb-1.5" />
          <span className="text-xs font-semibold text-amber-200">Simulate Failure</span>
          <span className="text-[10px] text-amber-400/80 font-mono">Test Full Jitter</span>
        </button>

        <button
          onClick={() => onDispatch('poison')}
          className="flex flex-col items-center justify-center p-3 rounded-xl bg-slate-800/80 hover:bg-rose-950/40 border border-slate-700 hover:border-rose-600/50 text-center transition group"
        >
          <Skull className="w-5 h-5 text-rose-400 group-hover:scale-110 transition mb-1.5" />
          <span className="text-xs font-semibold text-rose-200">Poison Pill</span>
          <span className="text-[10px] text-rose-400/80 font-mono">Max Retries &rarr; DLQ</span>
        </button>
      </div>

      {/* Traffic Surge Bursts */}
      <div className="mt-4 pt-3.5 border-t border-slate-800 flex flex-wrap items-center justify-between gap-3">
        <div className="flex items-center space-x-2 text-xs text-slate-300">
          <Activity className="w-4 h-4 text-emerald-400" />
          <span><strong>Traffic Surge Generator:</strong> Inject concurrent burst load:</span>
        </div>
        <div className="flex items-center gap-2">
          <button
            onClick={() => onBurst(25)}
            className="px-3 py-1 rounded bg-slate-800 hover:bg-slate-700 text-xs font-mono text-emerald-400 border border-emerald-500/20 transition"
          >
            +25 Burst
          </button>
          <button
            onClick={() => onBurst(100)}
            className="px-3 py-1 rounded bg-slate-800 hover:bg-slate-700 text-xs font-mono text-emerald-400 border border-emerald-500/20 transition"
          >
            +100 High Load
          </button>
        </div>
      </div>
    </section>
  );
}
