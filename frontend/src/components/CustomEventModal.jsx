import React, { useState } from 'react';
import { X, Code } from 'lucide-react';

export default function CustomEventModal({ isOpen, onClose, onSubmit }) {
  const [eventType, setEventType] = useState('order.created');
  const [priority, setPriority] = useState('HIGH');
  const [idempKey, setIdempKey] = useState(`tx_custom_${Date.now().toString().slice(-6)}`);
  const [maxRetries, setMaxRetries] = useState(3);
  const [payloadText, setPayloadText] = useState(JSON.stringify({
    order_id: 'ORD-CUSTOM-99',
    amount: 199.95,
    customer: 'Vishesh',
    timestamp: new Date().toISOString()
  }, null, 2));

  if (!isOpen) return null;

  const handleSubmit = (e) => {
    e.preventDefault();
    try {
      const parsed = JSON.parse(payloadText);
      onSubmit({
        event_type: eventType,
        priority,
        idempotency_key: idempKey,
        max_retries: parseInt(maxRetries, 10),
        payload: parsed
      });
      onClose();
    } catch (err) {
      alert(`Invalid JSON Payload: ${err.message}`);
    }
  };

  return (
    <div className="fixed inset-0 z-50 bg-black/75 backdrop-blur-sm flex items-center justify-center p-4">
      <div className="glass-card bg-[#0f172a] rounded-2xl max-w-lg w-full border border-slate-700 p-5 shadow-2xl">
        <div className="flex items-center justify-between pb-3 border-b border-slate-800 mb-4">
          <h3 className="text-sm font-semibold text-white flex items-center gap-2">
            <Code className="w-4 h-4 text-emerald-400" />
            Dispatch Custom JSON Event
          </h3>
          <button onClick={onClose} className="text-slate-400 hover:text-white">
            <X className="w-4 h-4" />
          </button>
        </div>

        <form onSubmit={handleSubmit} className="space-y-3 text-xs">
          <div className="grid grid-cols-2 gap-3">
            <div>
              <label className="block text-slate-400 mb-1">Event Type</label>
              <input
                type="text"
                value={eventType}
                onChange={(e) => setEventType(e.target.value)}
                required
                className="w-full bg-slate-900 border border-slate-700 rounded-lg p-2 text-white font-mono text-xs focus:border-emerald-500 focus:outline-none"
              />
            </div>
            <div>
              <label className="block text-slate-400 mb-1">Priority</label>
              <select
                value={priority}
                onChange={(e) => setPriority(e.target.value)}
                className="w-full bg-slate-900 border border-slate-700 rounded-lg p-2 text-white font-mono text-xs focus:border-emerald-500 focus:outline-none"
              >
                <option value="CRITICAL">CRITICAL (0)</option>
                <option value="HIGH">HIGH (1)</option>
                <option value="NORMAL">NORMAL (2)</option>
                <option value="LOW">LOW (3)</option>
              </select>
            </div>
          </div>

          <div>
            <label className="block text-slate-400 mb-1">Idempotency Key</label>
            <input
              type="text"
              value={idempKey}
              onChange={(e) => setIdempKey(e.target.value)}
              required
              className="w-full bg-slate-900 border border-slate-700 rounded-lg p-2 text-white font-mono text-xs focus:border-emerald-500 focus:outline-none"
            />
          </div>

          <div>
            <label className="block text-slate-400 mb-1">Max Retries</label>
            <input
              type="number"
              min="0"
              max="20"
              value={maxRetries}
              onChange={(e) => setMaxRetries(e.target.value)}
              className="w-full bg-slate-900 border border-slate-700 rounded-lg p-2 text-white font-mono text-xs focus:border-emerald-500 focus:outline-none"
            />
          </div>

          <div>
            <label className="block text-slate-400 mb-1">Payload JSON</label>
            <textarea
              rows="5"
              value={payloadText}
              onChange={(e) => setPayloadText(e.target.value)}
              required
              className="w-full bg-slate-900 border border-slate-700 rounded-lg p-2 text-emerald-400 font-mono text-xs focus:border-emerald-500 focus:outline-none"
            ></textarea>
          </div>

          <div className="mt-5 flex justify-end gap-2 pt-2 border-t border-slate-800">
            <button
              type="button"
              onClick={onClose}
              className="px-3 py-1.5 rounded-lg bg-slate-800 text-xs text-slate-300"
            >
              Cancel
            </button>
            <button
              type="submit"
              className="px-4 py-1.5 rounded-lg bg-emerald-500 hover:bg-emerald-600 text-black font-semibold text-xs transition"
            >
              Ingest Event
            </button>
          </div>
        </form>
      </div>
    </div>
  );
}
