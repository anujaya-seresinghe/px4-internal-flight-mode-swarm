import React, { useEffect } from 'react';
import { useStore } from './store';
import { connect } from './link';
import { TopBar } from './TopBar';
import { Map2D } from './views/Map2D';
import { Scene3D } from './views/Scene3D';
import { Sidebar } from './sidebar/Sidebar';

const App = () => {
  const view = useStore((s) => s.view);

  useEffect(() => {
    // ?sim forces the built-in simulator (handy for demos)
    if (new URLSearchParams(window.location.search).has('sim')) useStore.getState().setLink({ source: 'sim' });
    connect();
  }, []);

  return (
    <div className="app">
      <TopBar />
      <main className="workspace">
        <div className={`viewports viewports-${view}`}>
          {view !== '3d' && (
            <div className="viewport">
              <span className="viewport-tag">2D · Local NED</span>
              <Map2D />
            </div>
          )}
          {view !== '2d' && (
            <div className="viewport">
              <span className="viewport-tag">3D</span>
              <Scene3D />
            </div>
          )}
        </div>
        <Sidebar />
      </main>
    </div>
  );
};

export default App;
