import React from 'react';
import { RotateCw, Skull } from 'lucide-react';

export default function DLQTable({ dlqEvents, onReplay, onReplayAll }) {
  return (
    <div className="space-y-3">
      <div className="flex items-center justify-between text-xs text-slate-400">
        <p>
          Dead Letter Queue contains permanently poisoned or repeatedly failing events. Replaying resets attempts and restores status to <code className="text-emerald-400 font-mono">PENDING</code>.
        </p>
        {dlqEvents.length > 0 && (
          <button
            onClick={onReplayAll}
            className="px-3 py-1.5 rounded-lg bg-emerald-500/20 hover:bg-emerald-500/30 text-emerald-400 border border-emerald-500/40 text-xs font-semibold flex items-center gap-1.5 transition"
          >
            <RotateCw className="w-3.5 h-3.5" />
            <span>Replay All DLQ ({dlqEvents.length})</span>
          </button>
        )}
      </div>

      <div className="overflow-x-auto rounded-xl border border-slate-800">
        <table className="w-full text-left text-xs font-mono">
          <thead className="bg-rose-950/30 text-rose-300/80 border-b border-slate-800 uppercase text-[10px]">
            <tr>
              <th className="py-2.5 px-3">Event ID</th>
              <th className="py-2.5 px-3">Type</th>
              <th className="py-2.5 px-3">Failure Reason</th>
              <th className="py-2.5 px-3 text-center">Attempts</th>
              <th className="py-2.5 px-3">Quarantined At</th>
              <th className="py-2.5 px-3 text-center">Action</th>
            </tr>
          </thead>
          <tbody className="divide-y divide-slate-800/60 text-slate-300">
            {dlqEvents.length === 0 ? (
              <tr>
                <td colSpan="6" className="text-center py-8 text-slate-500 font-sans">
                  Dead Letter Queue is clear. (Click "Poison Pill" above to simulate a quarantine).
                </td>
              </tr>
            ) : (
              dlqEvents.map((ev) => {
                const id = ev.event_id || ev.id;
                return (
                  <tr key={id} className="hover:bg-rose-950/20 transition">
                    <td className="py-2.5 px-3 text-rose-300 font-medium">{id.slice(0, 13)}...</td>
                    <td className="py-2.5 px-3 text-slate-300">{ev.event_type}</td>
                    <td className="py-2.5 px-3 text-rose-400 text-[11px]">{ev.last_error || 'Max retries exhausted'}</td>
                    <td className="py-2.5 px-3 text-center text-slate-400">{ev.attempt_count}</td>
                    <td className="py-2.5 px-3 text-slate-400 text-[11px]">{ev.created_at || 'Just now'}</td>
                    <td className="py-2.5 px-3 text-center">
                      <button
                        onClick={() => onReplay(id)}
                        className="px-2.5 py-1 rounded bg-emerald-500/20 hover:bg-emerald-500/30 text-emerald-400 border border-emerald-500/40 text-[11px] font-semibold transition"
                      >
                        Replay
                      </button>
                    </td>
                  </tr>
                );
              })
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
}
