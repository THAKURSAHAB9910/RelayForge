import React from 'react';
import { TrendingUp, BarChart2 } from 'lucide-react';
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
  Filler
} from 'chart.js';
import { Line } from 'react-chartjs-2';

ChartJS.register(
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
  Filler
);

export default function ChartsPanel({ throughputHistory, latencyHistory }) {
  const throughputData = {
    labels: Array(throughputHistory.length).fill(''),
    datasets: [
      {
        label: 'Throughput (eps)',
        data: throughputHistory,
        borderColor: '#22c55e',
        backgroundColor: 'rgba(34, 197, 94, 0.1)',
        borderWidth: 2,
        tension: 0.35,
        fill: true,
        pointRadius: 0,
      },
    ],
  };

  const latencyData = {
    labels: Array(latencyHistory.p50.length).fill(''),
    datasets: [
      {
        label: 'p50',
        data: latencyHistory.p50,
        borderColor: '#22c55e',
        borderWidth: 1.5,
        tension: 0.3,
        pointRadius: 0,
      },
      {
        label: 'p95',
        data: latencyHistory.p95,
        borderColor: '#06b6d4',
        borderWidth: 1.5,
        tension: 0.3,
        pointRadius: 0,
      },
      {
        label: 'p99',
        data: latencyHistory.p99,
        borderColor: '#c084fc',
        borderWidth: 1.5,
        tension: 0.3,
        pointRadius: 0,
      },
    ],
  };

  const chartOptions = {
    responsive: true,
    maintainAspectRatio: false,
    plugins: {
      legend: { display: false },
      tooltip: {
        mode: 'index',
        intersect: false,
        backgroundColor: '#0f172a',
        borderColor: '#334155',
        borderWidth: 1,
        titleFont: { family: 'JetBrains Mono', size: 10 },
        bodyFont: { family: 'JetBrains Mono', size: 11 },
      },
    },
    scales: {
      x: { display: false },
      y: {
        grid: { color: 'rgba(51, 65, 85, 0.3)' },
        ticks: { color: '#94a3b8', font: { family: 'JetBrains Mono', size: 10 } },
      },
    },
  };

  return (
    <section className="grid grid-cols-1 lg:grid-cols-2 gap-4">
      {/* Throughput Chart */}
      <div className="glass-card rounded-2xl p-5 border border-slate-800 flex flex-col">
        <div className="flex items-center justify-between mb-3">
          <div className="flex items-center space-x-2">
            <TrendingUp className="w-4 h-4 text-emerald-400" />
            <h3 className="text-xs font-semibold uppercase tracking-wider text-slate-300">Real-Time Throughput (eps)</h3>
          </div>
          <span className="text-[11px] font-mono text-slate-400">Rolling 30s Window</span>
        </div>
        <div className="h-44 w-full relative">
          <Line data={throughputData} options={chartOptions} />
        </div>
      </div>

      {/* Latency Distribution Chart */}
      <div className="glass-card rounded-2xl p-5 border border-slate-800 flex flex-col">
        <div className="flex items-center justify-between mb-3">
          <div className="flex items-center space-x-2">
            <BarChart2 className="w-4 h-4 text-purple-400" />
            <h3 className="text-xs font-semibold uppercase tracking-wider text-slate-300">Latency Percentiles (ms)</h3>
          </div>
          <div className="flex items-center gap-2 text-[10px] font-mono">
            <span className="text-emerald-400">&bull; p50</span>
            <span className="text-cyan-400">&bull; p95</span>
            <span className="text-purple-400">&bull; p99</span>
          </div>
        </div>
        <div className="h-44 w-full relative">
          <Line data={latencyData} options={chartOptions} />
        </div>
      </div>
    </section>
  );
}
