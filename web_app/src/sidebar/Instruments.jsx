import React from 'react';

const deg = (r) => (r * 180) / Math.PI;

export const AttitudeIndicator = ({ roll = 0, pitch = 0, size = 104 }) => {
  const r = size / 2;
  const pitchPx = Math.max(-40, Math.min(40, deg(pitch))) * 1.2;
  return (
    <svg width={size} height={size} viewBox={`${-r} ${-r} ${size} ${size}`} className="instrument" aria-label="Attitude">
      <defs>
        <clipPath id="ai-clip">
          <circle r={r - 2} />
        </clipPath>
      </defs>
      <g clipPath="url(#ai-clip)">
        <g transform={`rotate(${-deg(roll)}) translate(0 ${pitchPx})`}>
          <rect x={-size} y={-size * 2} width={size * 2} height={size * 2} fill="#1d4f7a" />
          <rect x={-size} y={0} width={size * 2} height={size * 2} fill="#5b3b1f" />
          <line x1={-size} x2={size} y1={0} y2={0} stroke="#e2e8f0" strokeWidth="1.2" />
          {[-20, -10, 10, 20].map((p) => (
            <line key={p} x1={-(p % 20 ? 8 : 14)} x2={p % 20 ? 8 : 14} y1={-p * 1.2} y2={-p * 1.2} stroke="#e2e8f0" strokeWidth="1" opacity="0.7" />
          ))}
        </g>
      </g>
      <circle r={r - 2} fill="none" stroke="#2c4166" strokeWidth="2" />
      <path d={`M ${-r * 0.5} 0 H -8 L 0 6 L 8 0 H ${r * 0.5}`} fill="none" stroke="#fbbf24" strokeWidth="2.2" strokeLinejoin="round" />
      <circle r="2" fill="#fbbf24" />
    </svg>
  );
};

export const Compass = ({ yaw = 0, size = 104 }) => {
  const r = size / 2;
  const heading = ((deg(yaw) % 360) + 360) % 360;
  return (
    <svg width={size} height={size} viewBox={`${-r} ${-r} ${size} ${size}`} className="instrument" aria-label="Heading">
      <circle r={r - 2} fill="#0d1526" stroke="#2c4166" strokeWidth="2" />
      <g transform={`rotate(${-heading})`}>
        {Array.from({ length: 36 }, (_, i) => (
          <line
            key={i}
            y1={-(r - 4)}
            y2={-(r - (i % 9 === 0 ? 12 : 8))}
            stroke={i === 0 ? '#f87171' : '#6b7a93'}
            strokeWidth={i % 9 === 0 ? 2 : 1}
            transform={`rotate(${i * 10})`}
          />
        ))}
        {['N', 'E', 'S', 'W'].map((t, i) => (
          <text
            key={t}
            transform={`rotate(${i * 90}) translate(0 ${-(r - 22)}) rotate(${-i * 90 + heading})`}
            textAnchor="middle"
            dominantBaseline="central"
            fontSize="11"
            fontWeight="700"
            fill={t === 'N' ? '#f87171' : '#c9d4e5'}
          >
            {t}
          </text>
        ))}
      </g>
      <path d="M 0 -14 L 7 8 L 0 4 L -7 8 Z" fill="#38bdf8" />
      <text y={r - 16} textAnchor="middle" fontSize="10" fill="#c9d4e5" fontFamily="JetBrains Mono, monospace">
        {heading.toFixed(0).padStart(3, '0')}°
      </text>
    </svg>
  );
};

export const BatteryBar = ({ value }) => {
  if (value === null || value === undefined) return <span className="dim">—</span>;
  const level = value > 50 ? 'ok' : value > 20 ? 'warn' : 'bad';
  return (
    <span className={`battery battery-${level}`}>
      <span className="battery-fill" style={{ width: `${Math.max(4, value)}%` }} />
      <span className="battery-text mono">{value}%</span>
    </span>
  );
};
