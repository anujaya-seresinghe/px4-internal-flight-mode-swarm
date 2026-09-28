import React, { useRef, useState } from 'react';
import { APF_RADIUS, tightPairs } from '../lib/formations';

const W = 332;
const H = 240;
const PAD = 34;

/** North-up plot of formation offsets; followers can be dragged to build a custom formation. */
export const FormationEditor = ({ offsets, leaderId, color, onMove }) => {
  const svgRef = useRef(null);
  const [dragId, setDragId] = useState(null);
  const view = useRef(null);

  const ids = Object.keys(offsets).map(Number);
  // Freeze the scale while dragging so the plot doesn't rescale under the cursor
  if (!dragId || !view.current) {
    const xs = ids.map((id) => offsets[id].x);
    const ys = ids.map((id) => offsets[id].y);
    const minN = Math.min(...xs, 0) - 4;
    const maxN = Math.max(...xs, 0) + 4;
    const minE = Math.min(...ys, 0) - 4;
    const maxE = Math.max(...ys, 0) + 4;
    const scale = Math.min((W - PAD * 2) / (maxE - minE), (H - PAD * 2) / (maxN - minN));
    view.current = { scale, cN: (maxN + minN) / 2, cE: (maxE + minE) / 2 };
  }
  const { scale, cN, cE } = view.current;
  const sx = (e) => W / 2 + (e - cE) * scale;
  const sy = (n) => H / 2 - (n - cN) * scale;

  const toWorld = (ev) => {
    const r = svgRef.current.getBoundingClientRect();
    const px = ((ev.clientX - r.left) / r.width) * W;
    const py = ((ev.clientY - r.top) / r.height) * H;
    const snap = (v) => Math.round(v * 2) / 2;
    return { x: snap(cN - (py - H / 2) / scale), y: snap(cE + (px - W / 2) / scale) };
  };

  const tight = tightPairs(offsets);
  const gridStep = scale > 14 ? 2 : scale > 6 ? 5 : 10;
  const gridLines = [];
  for (let e = Math.ceil((cE - W / 2 / scale) / gridStep) * gridStep; e <= cE + W / 2 / scale; e += gridStep) {
    gridLines.push(<line key={`e${e}`} x1={sx(e)} x2={sx(e)} y1={0} y2={H} className={e === 0 ? 'fe-axis' : 'fe-grid'} />);
  }
  for (let n = Math.ceil((cN - H / 2 / scale) / gridStep) * gridStep; n <= cN + H / 2 / scale; n += gridStep) {
    gridLines.push(<line key={`n${n}`} x1={0} x2={W} y1={sy(n)} y2={sy(n)} className={n === 0 ? 'fe-axis' : 'fe-grid'} />);
  }

  return (
    <div className="formation-editor">
      <svg
        ref={svgRef}
        viewBox={`0 0 ${W} ${H}`}
        onPointerMove={(ev) => dragId !== null && onMove(dragId, toWorld(ev))}
        onPointerUp={() => setDragId(null)}
        onPointerLeave={() => setDragId(null)}
      >
        {gridLines}
        {ids.map((id) => (
          <circle key={`apf${id}`} cx={sx(offsets[id].y)} cy={sy(offsets[id].x)} r={(APF_RADIUS / 2) * scale} className="fe-apf" />
        ))}
        {tight.map((p) => (
          <line
            key={`${p.a}-${p.b}`}
            x1={sx(offsets[p.a].y)} y1={sy(offsets[p.a].x)}
            x2={sx(offsets[p.b].y)} y2={sy(offsets[p.b].x)}
            className="fe-tight"
          />
        ))}
        {ids.map((id) => {
          const o = offsets[id];
          const isLeader = id === leaderId;
          return (
            <g
              key={id}
              transform={`translate(${sx(o.y)} ${sy(o.x)})`}
              className={`fe-node ${isLeader ? 'leader' : 'follower'} ${dragId === id ? 'dragging' : ''}`}
              onPointerDown={(ev) => {
                if (isLeader) return;
                ev.currentTarget.ownerSVGElement.setPointerCapture(ev.pointerId);
                setDragId(id);
              }}
            >
              <circle r={isLeader ? 12 : 10} style={{ fill: isLeader ? 'var(--leader)' : color }} />
              <text textAnchor="middle" dominantBaseline="central">{id}</text>
              {!isLeader && (
                <text className="fe-coord" y={20} textAnchor="middle">
                  {o.x.toFixed(1)}, {o.y.toFixed(1)}
                </text>
              )}
            </g>
          );
        })}
        <g transform="translate(18 22)" className="fe-north">
          <path d="M 0 -9 L 5 4 L 0 1 L -5 4 Z" />
          <text y={15} textAnchor="middle">N</text>
        </g>
        <text x={W - 8} y={H - 8} textAnchor="end" className="fe-scale">grid {gridStep} m</text>
      </svg>
      <div className="fe-caption">Offsets from leader in metres (north, east). Drag followers to customise.</div>
    </div>
  );
};
