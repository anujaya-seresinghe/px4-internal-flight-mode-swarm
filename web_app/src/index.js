import React from 'react';
import ReactDOM from 'react-dom/client';
import App from './App';

// Global CSS Reset
const styleTag = document.createElement('style');
styleTag.innerHTML = `* { margin: 0; padding: 0; overflow: hidden; box-sizing: border-box; }`;
document.head.appendChild(styleTag);

const root = ReactDOM.createRoot(document.getElementById('root'));
root.render(<App />);