import React from 'react';
import { Zap, RefreshCw, Server, AlertCircle, CheckCircle, Github } from 'lucide-react';

export default function Header({ isConnected, backendSource, onRefresh, backendUrl }) {
  return (
    <header className="border-b border-slate-800 bg-[#0f172a]/90 backdrop-blur sticky top-0 z-40 px-4 lg:px-8 py-3.5">
      <div className="max-w-7xl mx-auto flex flex-wrap items-center justify-between gap-4">
        {/* Brand */}
        <div className="flex items-center space-x-3">
          <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-emerald-500 to-cyan-500 flex items-center justify-center font-bold text-black shadow-lg shadow-emerald-500/20">
            <Zap className="w-6 h-6 stroke-[2.5]" />
          </div>
          <div>
            <div className="flex items-center space-x-2">
              <h1 className="text-lg font-bold tracking-tight text-white">RelayForge</h1>
              <span className="text-[11px] px-2 py-0.5 rounded-full bg-emerald-500/10 text-emerald-400 border border-emerald-500/20 font-mono font-medium">
                REACT + VITE (:5173)
              </span>
            </div>
            <p className="text-xs text-slate-400">Fault-Tolerant Distributed Event Processing Engine</p>
          </div>
        </div>

        {/* Live Status Indicators */}
        <div className="flex items-center space-x-3">
          {isConnected ? (
            <div className="flex items-center space-x-2 px-3 py-1.5 rounded-full bg-emerald-950/80 border border-emerald-500/40 text-xs font-mono text-emerald-300">
              <span className="w-2.5 h-2.5 rounded-full bg-emerald-400 animate-pulse"></span>
              <span>LIVE CORE: {backendSource}</span>
            </div>
          ) : (
            <div className="flex items-center space-x-2 px-3 py-1.5 rounded-full bg-amber-950/80 border border-amber-500/40 text-xs font-mono text-amber-300">
              <span className="w-2.5 h-2.5 rounded-full bg-amber-400 animate-ping"></span>
              <span>SIMULATION MODE (Core Offline)</span>
            </div>
          )}

          <button
            onClick={onRefresh}
            className="p-2 rounded-lg bg-slate-800 hover:bg-slate-700 border border-slate-700 text-slate-300 transition"
            title="Poll Engine Now"
          >
            <RefreshCw className="w-4 h-4" />
          </button>

          <a
            href="https://github.com/THAKURSAHAB9910/RelayForge"
            target="_blank"
            rel="noreferrer"
            className="flex items-center space-x-1.5 px-3 py-1.5 rounded-lg bg-slate-800 hover:bg-slate-700 border border-slate-700 text-xs font-medium text-slate-200 transition"
          >
            <Github className="w-4 h-4" />
            <span>GitHub</span>
          </a>
        </div>
      </div>
    </header>
  );
}
