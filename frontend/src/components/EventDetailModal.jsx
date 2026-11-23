import React from 'react';
import { X, FileJson, ArrowRight } from 'lucide-react';

export default function EventDetailModal({ event, onClose }) {
  if (!event) return null;

  return (
    <div className="fixed inset-0 z-50 bg-black/75 backdrop-blur-sm flex items-center justify-center p-4">
      <div className="glass-card bg-[#0f172a] rounded-2xl max-w-2xl w-full border border-slate-700 overflow-hidden shadow-2xl">
        <div className="flex items-center justify-between p-4 border-b border-slate-800">
          <div className="flex items-center space-x-2">
            <FileJson className="w-5 h-5 text-emerald-400" />
            <h3 className="text-sm font-semibold text-white">Event Audit &amp; Payload Inspector</h3>
          </div>
          <button onClick={onClose} className="text-slate-400 hover:text-white p-1">
            <X className="w-5 h-5" />
          </button>
        </div>

        <div className="p-5 space-y-4 max-h-[75vh] overflow-y-auto text-xs font-mono">
          <div>
            <span className="text-slate-400 block mb-1 text-[11px] uppercase tracking-wider font-sans">
              Event Metadata
            </span>
            <div className="p-3 rounded-lg bg-slate-900 border border-slate-800 space-y-1.5 text-slate-300">
              <div><strong className="text-slate-400">Event ID:</strong> {event.id}</div>
              <div><strong className="text-slate-400">Idempotency Key:</strong> {event.idempotency_key}</div>
              <div><strong className="text-slate-400">Event Type:</strong> {event.event_type} ({event.priority})</div>
              <div><strong className="text-slate-400">Current Status:</strong> <span className="text-emerald-400">{event.status}</span></div>
              <div><strong className="text-slate-400">Attempt Count:</strong> {event.attempt_count} / {event.max_retries || 3}</div>
              {event.last_error && (
                <div className="text-rose-400"><strong className="text-slate-400">Last Error:</strong> {event.last_error}</div>
              )}
            </div>
          </div>

          <div>
            <span className="text-slate-400 block mb-1 text-[11px] uppercase tracking-wider font-sans">
              Payload JSON
            </span>
            <pre className="p-3 rounded-lg bg-slate-950 border border-slate-800 text-emerald-400 text-[11px] overflow-x-auto">
              {typeof event.payload === 'string'
                ? event.payload
                : JSON.stringify(event.payload, null, 2)}
            </pre>
          </div>

          <div>
            <span className="text-slate-400 block mb-1 text-[11px] uppercase tracking-wider font-sans">
              WAL Audit Transition Timeline
            </span>
            <div className="space-y-1.5">
              {(event.audit && event.audit.length > 0) ? (
                event.audit.map((a, idx) => (
                  <div key={idx} className="p-2 rounded bg-slate-900 border border-slate-800 flex items-center justify-between">
                    <span className="text-cyan-400 font-semibold flex items-center gap-1">
                      <ArrowRight className="w-3.5 h-3.5" /> {a.to}
                    </span>
                    <span className="text-slate-400 text-[10px]">{a.reason}</span>
                    <span className="text-slate-500 text-[10px]">{a.time}</span>
                  </div>
                ))
              ) : (
                <div className="p-2 text-slate-500 italic text-[11px]">
                  No transition records available.
                </div>
              )}
            </div>
          </div>
        </div>

        <div className="p-4 border-t border-slate-800 bg-slate-900/60 flex justify-end">
          <button
            onClick={onClose}
            className="px-4 py-1.5 rounded-lg bg-slate-800 hover:bg-slate-700 text-xs font-semibold text-white border border-slate-700"
          >
            Close
          </button>
        </div>
      </div>
    </div>
  );
}
