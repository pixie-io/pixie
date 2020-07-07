const LATENCY_HIGH_THRESHOLD = 300;
const LATENCY_MEDIUM_THRESHOLD = 150;

export type GaugeLevel = 'low' | 'med' | 'high' | 'none';

export function getLatencyLevel(val: number): GaugeLevel {
  if (val < LATENCY_MEDIUM_THRESHOLD) {
    return 'low';
  }
  if (val < LATENCY_HIGH_THRESHOLD) {
    return 'med';
  }
  return 'high';
}
