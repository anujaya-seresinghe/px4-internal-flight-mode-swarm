import React from 'react';
import { SwarmWeight } from './SwarmWeight';

/** PX4 parameters that apply to the whole fleet */
export const ParamsPanel = () => (
  <div className="panel-stack">
    <section className="section">
      <div className="section-head">
        <h3>Consensus weight <span className="dim mono">SWARM_WEIGHT</span></h3>
      </div>
      <SwarmWeight />
    </section>
  </div>
);
