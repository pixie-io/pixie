import * as React from 'react';
import {AutoSizer} from 'react-virtualized';

interface AutoSizerProps {
  width: number;
  height: number;
}

export type WithAutoSizerProps<T> = T & AutoSizerProps;

export default function withAutoSizer<T>(WrappedComponent: React.ComponentType<T & AutoSizerProps>) {
  return (props: T) => (
    <AutoSizer>
      {({ height, width }) => (
        <WrappedComponent width={Math.max(width, 0)} height={Math.max(height, 0)} {...props} />
      )}
    </AutoSizer>
  );
}
