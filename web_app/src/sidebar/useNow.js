import { useEffect, useState } from 'react';

/** Re-render periodically so "time since" values stay current. */
export const useNow = (intervalMs = 1000) => {
  const [now, setNow] = useState(Date.now());
  useEffect(() => {
    const t = setInterval(() => setNow(Date.now()), intervalMs);
    return () => clearInterval(t);
  }, [intervalMs]);
  return now;
};
