import React, { useState } from 'react';
import { Copy, Check } from 'lucide-react';

export default function PrometheusView({ metricsText }) {
  const [copied, setCopied] = useState(false);

  const handleCopy = () => {
    navigator.clipboard.writeText(metricsText);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <div className="space-y-3">
      <div className="flex items-center justify-between text-xs text-slate-400">
        <span>Live Prometheus Text Exposition Format (`GET /metrics`):</span>
        <button
          onClick={handleCopy}
          className="px-2.5 py-1 rounded bg-slate-800 hover:bg-slate-700 text-xs text-slate-300 border border-slate-700 flex items-center gap-1 transition"
        >
          {copied ? <Check className="w-3.5 h-3.5 text-emerald-400" /> : <Copy className="w-3.5 h-3.5" />}
          <span>{copied ? 'Copied!' : 'Copy Metrics'}</span>
        </button>
      </div>
      <pre className="p-4 rounded-xl bg-slate-950 border border-slate-800 text-[11px] font-mono text-emerald-400/90 overflow-x-auto max-h-96">
        {metricsText || '# Loading metrics...'}
      </pre>
    </div>
  );
}
