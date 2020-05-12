import * as React from 'react';

type Domain = [number, number];
type DomainFn = ((domain: Domain) => Domain);

interface VegaContextProps {
  hoverTime: number | null;
  setHoverTime: (time: number) => void;
  setTimeseriesDomain: (domain: Domain | DomainFn) => void;
  timeseriesDomain: Domain | null;
}

export const VegaContext = React.createContext<VegaContextProps>(null);

export const VegaContextProvider = (props) => {
  const [hoverTime, setHoverTime] = React.useState<number | null>(null);
  const [timeseriesDomain, setTimeseriesDomain] = React.useState<Domain | null>(null);

  return (
    <VegaContext.Provider value={{
      hoverTime,
      setHoverTime,
      timeseriesDomain,
      setTimeseriesDomain,
    }}>
      {props.children}
    </VegaContext.Provider>
  );
};

export function withVegaContextProvider<P>(Component: React.ComponentType<P>) {
  return function VegaContextProviderHOC(props: P) {
    return (
      <VegaContextProvider>
        <Component {...props} />
      </VegaContextProvider>
    );
  };
}
