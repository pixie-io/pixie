import * as React from 'react';

const useIsMounted: () => React.MutableRefObject<boolean> = () => {
  const mountedRef = React.useRef<boolean>(false);
  React.useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
    };
  });
  return mountedRef;
};

export default useIsMounted;
