import React, { useEffect, useMemo, useRef } from 'react';
import * as THREE from 'three';
import { Canvas, useFrame, useThree } from '@react-three/fiber';
import { OrbitControls, Grid, Html } from '@react-three/drei';
import { useShallow } from 'zustand/react/shallow';
import { useStore, isLinkLost } from '../store';
import { COLORS, vehicleRoles, formationTargets } from '../lib/theme';
import { isVtol } from '../lib/px4';
import { sendCommand } from '../lib/commands';

// Local NED (north, east, down) -> three.js (x = east, y = up, z = -north)
const toThree = (n, e, d, out = new THREE.Vector3()) => out.set(e, -d, -n);

const ARM_ANGLES = [Math.PI / 4, (3 * Math.PI) / 4, (5 * Math.PI) / 4, (7 * Math.PI) / 4];

const colorFor = (v, role, now) => {
  if (isLinkLost(v, now)) return COLORS.danger;
  if (role?.isLeader) return COLORS.leader;
  return role?.color || COLORS.vehicle;
};

const NoseMarker = ({ z }) => (
  <mesh position={[0, 0.02, z]} rotation={[-Math.PI / 2, 0, 0]}>
    <coneGeometry args={[0.08, 0.2, 12]} />
    <meshStandardMaterial color={COLORS.danger} emissive={COLORS.danger} emissiveIntensity={0.4} />
  </mesh>
);

/** Quadcopter (x500) from primitives. Nose points to -Z. */
const QuadModel = ({ bodyMat, rotors }) => (
  <>
    <mesh castShadow>
      <boxGeometry args={[0.35, 0.14, 0.5]} />
      <meshStandardMaterial ref={bodyMat} metalness={0.3} roughness={0.5} />
    </mesh>
    <NoseMarker z={-0.3} />
    {ARM_ANGLES.map((a, i) => {
      const x = Math.sin(a) * 0.55;
      const z = Math.cos(a) * 0.55;
      return (
        <group key={i}>
          <mesh position={[x / 2, 0, z / 2]} rotation={[0, a, 0]}>
            <boxGeometry args={[0.05, 0.04, 0.78]} />
            <meshStandardMaterial color="#334155" />
          </mesh>
          <mesh position={[x, 0.06, z]}>
            <cylinderGeometry args={[0.04, 0.05, 0.1, 10]} />
            <meshStandardMaterial color="#1e293b" />
          </mesh>
          <mesh ref={(el) => (rotors.current[i] = el)} position={[x, 0.12, z]}>
            <cylinderGeometry args={[0.24, 0.24, 0.01, 24]} />
            <meshStandardMaterial color="#cbd5e1" transparent opacity={0.28} />
          </mesh>
        </group>
      );
    })}
  </>
);

// Standard VTOL (gz standard_vtol): fuselage, main wing, tail, two booms with four lift
// rotors and a pusher propeller. Nose points to -Z, span ~2 m.
const VTOL_LIFT_ROTORS = [[-0.42, -0.42], [0.42, -0.42], [-0.42, 0.42], [0.42, 0.42]];

const VtolModel = ({ bodyMat, rotors, pusher }) => (
  <>
    {/* fuselage */}
    <mesh rotation={[Math.PI / 2, 0, 0]} castShadow>
      <cylinderGeometry args={[0.08, 0.06, 1.3, 16]} />
      <meshStandardMaterial ref={bodyMat} metalness={0.3} roughness={0.5} />
    </mesh>
    <mesh position={[0, 0, -0.65]} rotation={[-Math.PI / 2, 0, 0]}>
      <coneGeometry args={[0.08, 0.2, 16]} />
      <meshStandardMaterial color="#e2e8f0" metalness={0.2} roughness={0.5} />
    </mesh>
    <NoseMarker z={-0.8} />
    {/* main wing */}
    <mesh position={[0, 0.03, -0.05]} castShadow>
      <boxGeometry args={[2.0, 0.025, 0.28]} />
      <meshStandardMaterial color="#cbd5e1" metalness={0.2} roughness={0.6} />
    </mesh>
    {/* tail: horizontal stabilizer + vertical fin */}
    <mesh position={[0, 0.02, 0.58]}>
      <boxGeometry args={[0.62, 0.02, 0.16]} />
      <meshStandardMaterial color="#cbd5e1" metalness={0.2} roughness={0.6} />
    </mesh>
    <mesh position={[0, 0.14, 0.58]}>
      <boxGeometry args={[0.02, 0.24, 0.18]} />
      <meshStandardMaterial color="#cbd5e1" metalness={0.2} roughness={0.6} />
    </mesh>
    {/* booms and lift rotors */}
    {[-0.42, 0.42].map((x) => (
      <mesh key={x} position={[x, 0, 0]}>
        <boxGeometry args={[0.04, 0.04, 1.0]} />
        <meshStandardMaterial color="#334155" />
      </mesh>
    ))}
    {VTOL_LIFT_ROTORS.map(([x, z], i) => (
      <group key={i}>
        <mesh position={[x, 0.05, z]}>
          <cylinderGeometry args={[0.035, 0.04, 0.08, 10]} />
          <meshStandardMaterial color="#1e293b" />
        </mesh>
        <mesh ref={(el) => (rotors.current[i] = el)} position={[x, 0.1, z]}>
          <cylinderGeometry args={[0.2, 0.2, 0.01, 24]} />
          <meshStandardMaterial color="#cbd5e1" transparent opacity={0.28} />
        </mesh>
      </group>
    ))}
    {/* pusher propeller: disc in the XY plane at the tail, spins about Z */}
    <group ref={pusher} position={[0, 0, 0.7]}>
      <mesh rotation={[Math.PI / 2, 0, 0]}>
        <cylinderGeometry args={[0.18, 0.18, 0.01, 24]} />
        <meshStandardMaterial color="#cbd5e1" transparent opacity={0.3} />
      </mesh>
      <mesh rotation={[0, 0, 0]}>
        <boxGeometry args={[0.34, 0.03, 0.01]} />
        <meshStandardMaterial color="#475569" />
      </mesh>
    </group>
  </>
);

/** Quadcopter or VTOL, driven directly from the store every frame. */
const Drone = ({ id }) => {
  const vtol = useStore((s) => isVtol(s.vehicles[id]));
  const group = useRef();
  const body = useRef();
  const rotors = useRef([]);
  const pusher = useRef();
  const bodyMat = useRef();
  const ring = useRef();
  const drop = useRef();
  const label = useRef();
  const scaleRef = useRef(1);
  const { camera } = useThree();

  const dropGeom = useMemo(() => {
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.BufferAttribute(new Float32Array(6), 3));
    return g;
  }, []);

  const tmp = useMemo(() => new THREE.Vector3(), []);
  const euler = useMemo(() => new THREE.Euler(0, 0, 0, 'YXZ'), []);
  const quat = useMemo(() => new THREE.Quaternion(), []);

  useFrame((_, dt) => {
    const { vehicles, selected, selectedSwarm, swarms, overlays } = useStore.getState();
    const v = vehicles[id];
    if (!v || !group.current) return;
    const role = vehicleRoles(swarms)[id];
    const now = Date.now();
    const color = colorFor(v, role, now);

    toThree(v.x, v.y, v.z, tmp);
    group.current.position.lerp(tmp, 0.35);
    euler.set(v.pitch, -v.yaw, -v.roll);
    body.current.quaternion.slerp(quat.setFromEuler(euler), 0.35);

    // Keep drones readable when zoomed out
    const dist = camera.position.distanceTo(group.current.position);
    // (fixed-wing swarms spread over hundreds of metres, so allow a large scale-up)
    scaleRef.current = Math.min(500, Math.max(1.4, dist / 22)); // ~constant on-screen size far away
    body.current.scale.setScalar(scaleRef.current);

    bodyMat.current.color.set(color);
    // VTOL: lift rotors in multicopter phase and transitions, pusher in fixed-wing and transitions
    // (MAV_VTOL_STATE 1/2 = transitions, 3 = MC, 4 = FW)
    const vs = v.vtolState ?? 0;
    const liftOn = v.armed && (!vtol || vs !== 4);
    const pusherOn = v.armed && vtol && vs !== 3;
    rotors.current.forEach((r, i) => {
      if (r) r.rotation.y += liftOn ? dt * (i % 2 ? 40 : -40) : 0;
    });
    if (pusher.current && pusherOn) pusher.current.rotation.z += dt * 45;

    ring.current.visible = selected.includes(id) || !!swarms[selectedSwarm]?.members.includes(id);
    ring.current.scale.setScalar(scaleRef.current);
    ring.current.rotation.z += dt * 0.8;

    // Altitude drop line
    const dp = dropGeom.attributes.position.array;
    dp[0] = 0; dp[1] = 0; dp[2] = 0;
    dp[3] = 0; dp[4] = -group.current.position.y; dp[5] = 0;
    dropGeom.attributes.position.needsUpdate = true;
    drop.current.material.color.set(color);

    if (label.current) {
      label.current.style.display = overlays.labels ? '' : 'none';
      const lost = isLinkLost(v, now);
      label.current.dataset.state = lost ? 'lost' : role?.isLeader ? 'leader' : '';
      label.current.children[1].textContent = lost ? 'NO LINK' : `${(-v.z).toFixed(1)} m`;
    }
  });

  const onClick = (e) => {
    e.stopPropagation();
    const { select, pendingGoto } = useStore.getState();
    if (pendingGoto) return;
    select(id, e.nativeEvent.ctrlKey || e.nativeEvent.metaKey || e.nativeEvent.shiftKey ? 'toggle' : 'click');
  };

  return (
    <>
      <group ref={group}>
        <group ref={body} onClick={onClick} onDoubleClick={(e) => (e.stopPropagation(), useStore.getState().setFollow(id))}>
          {vtol ? (
            <VtolModel bodyMat={bodyMat} rotors={rotors} pusher={pusher} />
          ) : (
            <QuadModel bodyMat={bodyMat} rotors={rotors} />
          )}
        </group>
        <mesh ref={ring} rotation={[-Math.PI / 2, 0, 0]} visible={false}>
          <ringGeometry args={[0.95, 1.1, 40, 1, 0, Math.PI * 1.7]} />
          <meshBasicMaterial color={COLORS.select} side={THREE.DoubleSide} transparent opacity={0.9} />
        </mesh>
        <line ref={drop} geometry={dropGeom} frustumCulled={false}>
          <lineBasicMaterial transparent opacity={0.3} />
        </line>
        <Html position={[0, 1.6, 0]} center zIndexRange={[20, 0]} style={{ pointerEvents: 'none' }}>
          <div ref={label} className="label-3d">
            <span>UAV {id}</span>
            <span className="mono">—</span>
          </div>
        </Html>
      </group>
    </>
  );
};

/** Leader-to-member links and formation slots for every known swarm. */
const SwarmOverlay = () => {
  const linkGeom = useMemo(() => {
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.BufferAttribute(new Float32Array(256 * 6), 3));
    g.setAttribute('color', new THREE.BufferAttribute(new Float32Array(256 * 6), 3));
    g.setDrawRange(0, 0);
    return g;
  }, []);
  const slots = useRef();
  const dummy = useMemo(() => new THREE.Object3D(), []);
  const col = useMemo(() => new THREE.Color(), []);
  const linkRef = useRef();

  useFrame(() => {
    const { swarms, vehicles, overlays } = useStore.getState();
    const pos = linkGeom.attributes.position.array;
    const cols = linkGeom.attributes.color.array;
    let li = 0;
    let si = 0;
    const roles = vehicleRoles(swarms);
    Object.values(swarms).forEach((s) => {
      const leader = vehicles[s.leaderId];
      if (!leader?.hasPosition) return;
      col.set(roles[s.leaderId]?.color || COLORS.accent);
      if (overlays.links) {
        s.members.forEach((id) => {
          const m = vehicles[id];
          if (id === s.leaderId || !m?.hasPosition || li >= 256) return;
          pos.set([leader.y, -leader.z, -leader.x, m.y, -m.z, -m.x], li * 6);
          cols.set([col.r, col.g, col.b, col.r, col.g, col.b], li * 6);
          li++;
        });
      }
      if (overlays.targets) {
        formationTargets(s, vehicles).forEach((t) => {
          if (si >= 128) return;
          dummy.position.set(t.y, -t.z, -t.x);
          dummy.rotation.set(-Math.PI / 2, 0, 0);
          dummy.updateMatrix();
          slots.current.setMatrixAt(si, dummy.matrix);
          slots.current.setColorAt(si, col);
          si++;
        });
      }
    });
    linkGeom.setDrawRange(0, li * 2);
    linkGeom.attributes.position.needsUpdate = true;
    linkGeom.attributes.color.needsUpdate = true;
    slots.current.count = si;
    slots.current.instanceMatrix.needsUpdate = true;
    if (slots.current.instanceColor) slots.current.instanceColor.needsUpdate = true;
  });

  return (
    <>
      <lineSegments ref={linkRef} geometry={linkGeom} frustumCulled={false}>
        <lineBasicMaterial vertexColors transparent opacity={0.6} />
      </lineSegments>
      <instancedMesh ref={slots} args={[null, null, 128]} frustumCulled={false}>
        <ringGeometry args={[0.7, 0.85, 32]} />
        <meshBasicMaterial side={THREE.DoubleSide} transparent opacity={0.8} />
      </instancedMesh>
    </>
  );
};

/** Camera follow + fit-to-selection handling. */
const CameraRig = () => {
  const { camera, controls } = useThree();
  const focusSeq = useStore((s) => s.focus.seq);
  const tmp = useMemo(() => new THREE.Vector3(), []);
  const fitted = useRef(false);

  const fit = (ids) => {
    const { vehicles } = useStore.getState();
    const pts = (ids.length ? ids : Object.keys(vehicles).map(Number))
      .map((id) => vehicles[id])
      .filter((v) => v?.hasPosition);
    if (!pts.length || !controls) return false;
    const box = new THREE.Box3();
    pts.forEach((v) => box.expandByPoint(toThree(v.x, v.y, v.z)));
    const center = box.getCenter(new THREE.Vector3());
    const radius = Math.max(8, box.getSize(new THREE.Vector3()).length() * 0.8);
    const dir = camera.position.clone().sub(controls.target).normalize();
    controls.target.copy(center);
    camera.position.copy(center).addScaledVector(dir, radius * 2.4);
    controls.update();
    return true;
  };

  useEffect(() => {
    if (focusSeq) fit(useStore.getState().focus.ids);
  }, [focusSeq]); // eslint-disable-line react-hooks/exhaustive-deps

  useFrame(() => {
    const { follow, vehicles } = useStore.getState();
    if (!fitted.current && Object.keys(vehicles).length && controls) fitted.current = fit([]);
    if (!follow || !controls) return;
    const v = vehicles[follow];
    if (!v?.hasPosition) return;
    toThree(v.x, v.y, v.z, tmp);
    const delta = tmp.sub(controls.target).multiplyScalar(0.12);
    controls.target.add(delta);
    camera.position.add(delta);
    controls.update();
  });

  return null;
};

const Ground = () => {
  const onClick = (e) => {
    const { pendingGoto, setPendingGoto, select } = useStore.getState();
    if (e.delta > 4) return; // was an orbit drag
    if (pendingGoto) {
      e.stopPropagation();
      sendCommand(pendingGoto.ids, 'goto', { north: -e.point.z, east: e.point.x });
      setPendingGoto(null);
      return;
    }
    select([]);
  };
  return (
    <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -0.01, 0]} onClick={onClick}>
      <planeGeometry args={[40000, 40000]} />
      <meshStandardMaterial color="#0b1322" />
    </mesh>
  );
};

const HomeMarker = () => (
  <group>
    <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.02, 0]}>
      <ringGeometry args={[0.9, 1.2, 40]} />
      <meshBasicMaterial color={COLORS.success} side={THREE.DoubleSide} />
    </mesh>
    <Html position={[0, 0.2, 0]} center zIndexRange={[10, 0]} style={{ pointerEvents: 'none' }}>
      <div className="home-3d">H</div>
    </Html>
  </group>
);

const AxisLabels = () => (
  <>
    {[
      ['N', [0, 0.1, -30]],
      ['E', [30, 0.1, 0]],
    ].map(([t, p]) => (
      <Html key={t} position={p} center zIndexRange={[10, 0]} style={{ pointerEvents: 'none' }}>
        <div className="axis-3d">{t}</div>
      </Html>
    ))}
  </>
);

export const Scene3D = () => {
  const ids = useStore(useShallow((s) => Object.values(s.vehicles).filter((v) => v.hasPosition).map((v) => v.id)));
  const pendingGoto = useStore((s) => s.pendingGoto);

  return (
    <div className={`view view-3d ${pendingGoto ? 'picking' : ''}`}>
      <Canvas
        camera={{ position: [-25, 22, 30], fov: 50, near: 0.1, far: 20000 }}
        dpr={[1, 2]}
        gl={{ antialias: true }}
        onCreated={({ scene }) => {
          scene.background = new THREE.Color(COLORS.bg);
          // Far enough for fixed-wing swarms (hundreds of metres), still fades the horizon
          scene.fog = new THREE.Fog(COLORS.bg, 4000, 20000);
        }}
      >
        <ambientLight intensity={0.55} />
        <directionalLight position={[30, 60, 20]} intensity={1.3} />
        <hemisphereLight args={['#7dd3fc', '#0f172a', 0.35]} />
        <Ground />
        <Grid
          infiniteGrid
          cellSize={1}
          sectionSize={10}
          cellColor={COLORS.grid}
          sectionColor={COLORS.gridMajor}
          cellThickness={0.6}
          sectionThickness={1}
          fadeDistance={260}
          fadeStrength={1.5}
          position={[0, 0.005, 0]}
        />
        {/* Coarse grid (50 m / 500 m) that stays visible when zoomed out on fixed-wing swarms */}
        <Grid
          infiniteGrid
          cellSize={50}
          sectionSize={500}
          cellColor={COLORS.grid}
          sectionColor={COLORS.gridMajor}
          cellThickness={0.5}
          sectionThickness={1}
          fadeDistance={6000}
          fadeStrength={2}
          position={[0, 0.004, 0]}
        />
        <HomeMarker />
        <AxisLabels />
        <SwarmOverlay />
        {ids.map((id) => (
          <Drone key={id} id={id} />
        ))}
        <OrbitControls makeDefault maxPolarAngle={Math.PI / 2 - 0.02} enableDamping dampingFactor={0.12} />
        <CameraRig />
      </Canvas>
      {pendingGoto && (
        <div className="view-banner">
          Click the ground to send UAV {pendingGoto.ids.join(', ')} there · <kbd>Esc</kbd> to cancel
        </div>
      )}
      <div className="view-hint">Drag to orbit · right-drag to pan · scroll to zoom · double-click a drone to follow</div>
    </div>
  );
};
