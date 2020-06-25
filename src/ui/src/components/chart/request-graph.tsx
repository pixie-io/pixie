import * as React from 'react';
import { WidgetDisplay } from 'containers/live/vis';

export interface RequestGraphDisplay extends WidgetDisplay {
  readonly requestorPodColumn: string;
  readonly responderPodColumn: string;
  readonly requestorServiceColumn: string;
  readonly responderServiceColumn: string;
  readonly p50Column: string;
  readonly p90Column: string;
  readonly p99Column: string;
  readonly errorRateColumn: string;
  readonly requestsPerSecondColumn: string;
  readonly inboundBytesPerSecondColumn: string;
  readonly outboundBytesPerSecondColumn: string;
  readonly totalRequestCountColumn: string;
}

interface RequestGraphProps {
  display: RequestGraphDisplay;
  data: any[];
}

export const RequestGraphWidget = (props: RequestGraphProps) => {
  return <div>{JSON.stringify(props.display)}</div>;
}
