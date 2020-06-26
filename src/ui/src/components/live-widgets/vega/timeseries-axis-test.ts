import { formatTime, prepareLabels } from './timeseries-axis';

// Verify that time formatter works properly.
describe('timeFormat', () => {
  it('formats HH:MM:SS', () => {
    const tick = new Date(2020, 4, 1, 12, 21, 30);
    expect(formatTime(tick)).toEqual('12:21:30');
  });

  it('formats HH:MM:SS and AM/PM', () => {
    const tick = new Date(2020, 4, 1, 12, 21, 30);
    expect(formatTime(tick, /* showAmPM */ true)).toEqual('12:21:30 PM');
  });

  it('formats Date HH:MM:SS AM/PM ', () => {
    const tick = new Date(2020, 4, 1, 12, 21, 30);
    expect(formatTime(tick, /* showAmPM */ true, /* showDate */ true)).toEqual('May 01, 2020 12:21:30 PM');
  });
});

describe('prepareLabels', () => {
  it('simple hour data', () => {
    const start = new Date(2020, 4, 1, 12, 21, 30);
    const stop = new Date(2020, 4, 1, 13, 21, 30);
    const labels = prepareLabels(
      [start, stop],
      /* width */ 1300,
      /* numTicks */ Math.ceil(1300 / 20),
      /* overlapBuffer */ 100,
      'Roboto',
      /* fontSize */ 10,
    );

    const visibleLabels = [];
    labels.forEach((value) => {
      if (value.label) {
        visibleLabels.push(value.label);
      }
    });

    // There should be fewer visible labels than all labels.
    expect(visibleLabels.length).toBeLessThan(labels.length);

    // First label should have PM.
    expect(visibleLabels[0]).toEqual('12:22:00 PM');
    // Second label should not have AM/PM.
    expect(visibleLabels[1]).toEqual('12:30:00');
    // Last label should have PM.
    expect(visibleLabels[visibleLabels.length - 1]).toEqual('1:18:00 PM');
    // Second to last label should not have AM/PM.
    expect(visibleLabels[visibleLabels.length - 2]).toEqual('1:10:00');
  });
});
