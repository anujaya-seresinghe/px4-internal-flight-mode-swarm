import React, { useEffect, useRef, useState, useCallback } from 'react';

export const UavCanvas = ({ uavs, disabled, onSelectionComplete }) => {
  const canvasRef = useRef(null);

  // Viewport State (Zoom & Pan)
  const [scale, setScale] = useState(10);
  const [panOffset, setPanOffset] = useState({ x: 0, y: 0 });

  // Mouse Interaction States
  const [isPanning, setIsPanning] = useState(false);
  const [panStart, setPanStart] = useState({ x: 0, y: 0 });

  const [isSelecting, setIsSelecting] = useState(false);
  const [selectionStart, setSelectionStart] = useState(null);
  const [selectionEnd, setSelectionEnd] = useState(null);
  const [selectedBox, setSelectedBox] = useState(null);

  // Dynamic Window Resize
  useEffect(() => {
    const handleResize = () => {
      if (canvasRef.current) {
        canvasRef.current.width = window.innerWidth;
        canvasRef.current.height = window.innerHeight;
      }
    };
    handleResize();
    window.addEventListener('resize', handleResize);
    return () => window.removeEventListener('resize', handleResize);
  }, []);

  // Convert World Coordinates to Screen Pixel Coordinates
  const worldToScreen = useCallback(
    (uavX, uavY) => {
      const canvas = canvasRef.current;
      if (!canvas) return { px: 0, py: 0 };
      const centerX = canvas.width / 2;
      const centerY = canvas.height / 2;

      return {
        px: centerX + uavY * scale + panOffset.x,
        py: centerY - uavX * scale + panOffset.y,
      };
    },
    [scale, panOffset]
  );

  // Handle Zooming via Mouse Wheel
  const handleWheel = (e) => {
    e.preventDefault();
    if (disabled) return;

    const zoomFactor = 1.1;
    let newScale = e.deltaY < 0 ? scale * zoomFactor : scale / zoomFactor;
    newScale = Math.max(1, Math.min(100, newScale));
    setScale(newScale);
  };

  // Mouse Down: Left Click (Select) / Right or Middle Click (Pan)
  const handleMouseDown = (e) => {
    if (disabled) return;

    if (e.button === 1 || e.button === 2) {
      setIsPanning(true);
      setPanStart({ x: e.clientX - panOffset.x, y: e.clientY - panOffset.y });
      return;
    }

    if (e.button === 0) {
      setIsSelecting(true);
      setSelectionStart({ x: e.clientX, y: e.clientY });
      setSelectionEnd({ x: e.clientX, y: e.clientY });
      setSelectedBox(null);
      onSelectionComplete(null, []);
    }
  };

  const handleMouseMove = (e) => {
    if (isPanning) {
      setPanOffset({
        x: e.clientX - panStart.x,
        y: e.clientY - panStart.y,
      });
      return;
    }

    if (isSelecting) {
      setSelectionEnd({ x: e.clientX, y: e.clientY });
    }
  };

  const handleMouseUp = () => {
    if (isPanning) {
      setIsPanning(false);
      return;
    }

    if (!isSelecting || !selectionStart || !selectionEnd) return;
    setIsSelecting(false);

    const left = Math.min(selectionStart.x, selectionEnd.x);
    const top = Math.min(selectionStart.y, selectionEnd.y);
    const width = Math.abs(selectionEnd.x - selectionStart.x);
    const height = Math.abs(selectionEnd.y - selectionStart.y);

    if (width > 5 && height > 5) {
      const box = { left, top, width, height };
      setSelectedBox(box);

      const capturedIds = [];
      Object.entries(uavs).forEach(([sysId, uav]) => {
        const { px, py } = worldToScreen(uav.x, uav.y);
        if (
          px >= box.left &&
          px <= box.left + box.width &&
          py >= box.top &&
          py <= box.top + box.height
        ) {
          capturedIds.push(Number(sysId));
        }
      });

      onSelectionComplete(box, capturedIds);
    } else {
      setSelectedBox(null);
      onSelectionComplete(null, []);
    }
  };

  const handleContextMenu = (e) => {
    e.preventDefault();
  };

  // Canvas Render Loop
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');

    // 1. Clear Screen
    ctx.fillStyle = '#ffffff';
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    // 2. Draw Active UAVs
    Object.entries(uavs).forEach(([sysId, uav]) => {
      const { px: posX, py: posY } = worldToScreen(uav.x, uav.y);

      ctx.save();
      ctx.translate(posX, posY);
      ctx.rotate(uav.yaw);

      ctx.fillStyle = '#000000';
      ctx.beginPath();
      ctx.moveTo(0, -18);
      ctx.lineTo(12, 12);
      ctx.lineTo(-12, 12);
      ctx.closePath();
      ctx.fill();

      ctx.restore();

      ctx.fillStyle = '#000000';
      ctx.font = 'bold 12px sans-serif';
      ctx.fillText(`UAV ${sysId}`, posX + 16, posY + 4);
    });

    // 3. Draw Active Selection Box
    const boxToDraw = isSelecting && selectionStart && selectionEnd
      ? {
          left: Math.min(selectionStart.x, selectionEnd.x),
          top: Math.min(selectionStart.y, selectionEnd.y),
          width: Math.abs(selectionEnd.x - selectionStart.x),
          height: Math.abs(selectionEnd.y - selectionStart.y),
        }
      : selectedBox;

    if (boxToDraw) {
      ctx.fillStyle = 'rgba(0, 122, 255, 0.15)';
      ctx.fillRect(boxToDraw.left, boxToDraw.top, boxToDraw.width, boxToDraw.height);

      ctx.strokeStyle = '#007aff';
      ctx.lineWidth = 2;
      ctx.setLineDash([6, 4]);
      ctx.strokeRect(boxToDraw.left, boxToDraw.top, boxToDraw.width, boxToDraw.height);
      ctx.setLineDash([]);
    }
  }, [uavs, isSelecting, selectionStart, selectionEnd, selectedBox, worldToScreen]);

  return (
    <div style={{ position: 'relative', width: '100vw', height: '100vh' }}>
      <canvas
        ref={canvasRef}
        onWheel={handleWheel}
        onMouseDown={handleMouseDown}
        onMouseMove={handleMouseMove}
        onMouseUp={handleMouseUp}
        onContextMenu={handleContextMenu}
        style={{
          display: 'block',
          position: 'absolute',
          top: 0,
          left: 0,
          width: '100vw',
          height: '100vh',
          backgroundColor: '#ffffff',
          cursor: isPanning ? 'grabbing' : isSelecting ? 'crosshair' : 'grab',
        }}
      />

      {/* Floating Reset View Button */}
      <button
        onClick={() => {
          setPanOffset({ x: 0, y: 0 });
          setScale(10);
        }}
        title="Reset View"
        style={{
          position: 'absolute',
          bottom: '24px',
          right: '24px',
          width: '40px',
          height: '40px',
          borderRadius: '50%',
          backgroundColor: '#ffffff',
          border: '1px solid #ccc',
          boxShadow: '0 2px 8px rgba(0,0,0,0.15)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          cursor: 'pointer',
          outline: 'none',
        }}
      >
        <svg
          width="18"
          height="18"
          viewBox="0 0 24 24"
          fill="none"
          stroke="#333333"
          strokeWidth="2"
          strokeLinecap="round"
          strokeLinejoin="round"
        >
          <path d="M3 12a9 9 0 1 0 9-9 9.75 9.75 0 0 0-6.74 2.74L3 8" />
          <path d="M3 3v5h5" />
        </svg>
      </button>
    </div>
  );
};