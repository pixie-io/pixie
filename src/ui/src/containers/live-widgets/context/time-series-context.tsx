import * as React from 'react';

type Domain = [number, number];
type DomainFn = ((domain: Domain) => Domain);

interface TimeSeriesContextProps {
  hoverTime: number | null;
  setHoverTime: (time: number) => void;
  setTimeseriesDomain: (domain: Domain | DomainFn) => void;
  timeseriesDomain: Domain | null;
}

export const TimeSeriesContext = React.createContext<TimeSeriesContextProps>(null);

export const TimeSeriesContextProvider = (props) => {
  const [hoverTime, setHoverTime] = React.useState<number | null>(null);
  const [timeseriesDomain, setTimeseriesDomain] = React.useState<Domain | null>(null);

  const context = React.useMemo(() => ({
    hoverTime,
    setHoverTime,
    timeseriesDomain,
    setTimeseriesDomain,
  }), [hoverTime, setHoverTime, timeseriesDomain, setTimeseriesDomain]);

  return (
    <TimeSeriesContext.Provider value={context}>
      {props.children}
    </TimeSeriesContext.Provider>
  );
};

export function withTimeSeriesContextProvider<P>(Component: React.ComponentType<P>) {
  return function TimeSeriesContextProviderHOC(props: P) {
    return (
      <TimeSeriesContextProvider>
        <Component {...props} />
      </TimeSeriesContextProvider>
    );
  };
}
