import React, { useState } from 'react';
import { Search } from 'lucide-react';

export default function EventTable({ events, onSelectEvent }) {
  const [searchTerm, setSearchTerm] = useState('');
  const [statusFilter, setStatusFilter] = useState('ALL');

  const filtered = events.filter(ev => {
    const q = searchTerm.toLowerCase();
    const matchesSearch = !q ||
      (ev.id && ev.id.toLowerCase().includes(q)) ||
      (ev.event_type && ev.event_type.toLowerCase().includes(q)) ||
      (ev.idempotency_key && ev.idempotency_key.toLowerCase().includes(q));

    const matchesStatus = statusFilter === 'ALL' || ev.status === statusFilter;
    return matchesSearch && matchesStatus;
  });

  return (
    <div className="space-y-3">
      {/* Search & Filter Bar */}
      <div className="flex flex-wrap items-center justify-between gap-3 text-xs">
        <div className="flex items-center gap-2">
          <div className="relative">
            <input
              type="text"
              value={searchTerm}
              onChange={(e) => setSearchTerm(e.target.value)}
              placeholder="Search Event ID, Type, Key..."
              className="bg-slate-900 border border-slate-700 rounded-lg pl-8 pr-3 py-1.5 text-xs text-white placeholder-slate-500 focus:outline-none focus:border-emerald-500 w-64"
            />
            <Search className="w-3.5 h-3.5 text-slate-500 absolute left-2.5 top-2.5" />
          </div>

          <select
            value={statusFilter}
            onChange={(e) => setStatusFilter(e.target.value)}
            className="bg-slate-900 border border-slate-700 rounded-lg px-2.5 py-1.5 text-xs text-slate-300 focus:outline-none"
          >
            <option value="ALL">All Statuses</option>
            <option value="COMPLETED">Completed</option>
            <option value="IN_FLIGHT">In Flight</option>
            <option value="RETRY_SCHEDULED">Retry Scheduled</option>
            <option value="DEAD_LETTER">Dead Letter</option>
            <option value="PENDING">Pending</option>
          </select>
        </div>

        <div className="text-[11px] text-slate-400 font-mono">
          Showing {Math.min(filtered.length, 30)} of {events.length} events
        </div>
      </div>

      {/* Events Table */}
      <div className="overflow-x-auto rounded-xl border border-slate-800">
        <table className="w-full text-left text-xs">
          <thead className="bg-slate-900/80 text-slate-400 border-b border-slate-800 uppercase font-mono text-[10px]">
            <tr>
              <th className="py-2.5 px-3">Time</th>
              <th className="py-2.5 px-3">Event ID</th>
              <th className="py-2.5 px-3">Idempotency Key</th>
              <th className="py-2.5 px-3">Event Type</th>
              <th className="py-2.5 px-3">Priority</th>
              <th className="py-2.5 px-3">Status</th>
              <th className="py-2.5 px-3 text-center">Attempt</th>
              <th className="py-2.5 px-3 text-right">Duration</th>
              <th className="py-2.5 px-3 text-center">Action</th>
            </tr>
          </thead>
          <tbody className="divide-y divide-slate-800/60 font-mono text-slate-300">
            {filtered.length === 0 ? (
              <tr>
                <td colSpan="9" className="text-center py-8 text-slate-500 font-sans">
                  No events match filter criteria. Dispatch events using the buttons above.
                </td>
              </tr>
            ) : (
              filtered.slice(0, 30).map((ev) => (
                <tr
                  key={ev.id}
                  onClick={() => onSelectEvent(ev)}
                  className="hover:bg-slate-800/40 transition cursor-pointer"
                >
                  <td className="py-2.5 px-3 text-slate-400 text-[11px]">{ev.created_at || 'Just now'}</td>
                  <td className="py-2.5 px-3 text-cyan-300 font-medium">{(ev.id || '').slice(0, 8)}...</td>
                  <td className="py-2.5 px-3 text-slate-400 text-[11px]">{ev.idempotency_key}</td>
                  <td className="py-2.5 px-3 font-semibold text-slate-200">{ev.event_type}</td>
                  <td className="py-2.5 px-3">{renderPriorityBadge(ev.priority)}</td>
                  <td className="py-2.5 px-3">{renderStatusBadge(ev.status)}</td>
                  <td className="py-2.5 px-3 text-center text-slate-400">{ev.attempt_count}/{ev.max_retries || 3}</td>
                  <td className="py-2.5 px-3 text-right font-medium text-emerald-400">
                    {ev.duration_ms ? `${ev.duration_ms} ms` : '--'}
                  </td>
                  <td className="py-2.5 px-3 text-center">
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        onSelectEvent(ev);
                      }}
                      className="px-2 py-0.5 rounded bg-slate-800 hover:bg-slate-700 text-slate-300 text-[11px]"
                    >
                      Inspect
                    </button>
                  </td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
}

function renderPriorityBadge(prio) {
  if (prio === 'CRITICAL' || prio === 0) {
    return <span className="px-1.5 py-0.5 rounded text-[10px] font-bold bg-purple-500/20 text-purple-300 border border-purple-500/30">CRITICAL</span>;
  }
  if (prio === 'HIGH' || prio === 1) {
    return <span className="px-1.5 py-0.5 rounded text-[10px] font-bold bg-amber-500/20 text-amber-300 border border-amber-500/30">HIGH</span>;
  }
  if (prio === 'NORMAL' || prio === 2) {
    return <span className="px-1.5 py-0.5 rounded text-[10px] font-bold bg-blue-500/20 text-blue-300 border border-blue-500/30">NORMAL</span>;
  }
  return <span className="px-1.5 py-0.5 rounded text-[10px] font-bold bg-slate-700 text-slate-300">LOW</span>;
}

function renderStatusBadge(status) {
  if (status === 'COMPLETED') {
    return (
      <span className="px-1.5 py-0.5 rounded text-[10px] font-bold bg-emerald-500/20 text-emerald-300 border border-emerald-500/30 flex items-center gap-1 w-fit">
        <span className="w-1.5 h-1.5 rounded-full bg-emerald-400"></span>COMPLETED
      </span>
    );
  }
  if (status === 'IN_FLIGHT') {
    return (
      <span className="px-1.5 py-0.5 rounded text-[10px] font-bold bg-cyan-500/20 text-cyan-300 border border-cyan-500/30 flex items-center gap-1 w-fit animate-pulse">
        <span className="w-1.5 h-1.5 rounded-full bg-cyan-400"></span>IN_FLIGHT
      </span>
    );
  }
  if (status === 'RETRY_SCHEDULED') {
    return (
      <span className="px-1.5 py-0.5 rounded text-[10px] font-bold bg-amber-500/20 text-amber-300 border border-amber-500/30 flex items-center gap-1 w-fit">
        <span className="w-1.5 h-1.5 rounded-full bg-amber-400 animate-ping"></span>RETRYING
      </span>
    );
  }
  if (status === 'DEAD_LETTER') {
    return (
      <span className="px-1.5 py-0.5 rounded text-[10px] font-bold bg-rose-500/20 text-rose-300 border border-rose-500/30 flex items-center gap-1 w-fit">
        <span className="w-1.5 h-1.5 rounded-full bg-rose-400"></span>DEAD_LETTER
      </span>
    );
  }
  return (
    <span className="px-1.5 py-0.5 rounded text-[10px] font-bold bg-slate-800 text-slate-300 border border-slate-700">
      PENDING
    </span>
  );
}
