import { VizierErrorDetails } from 'common/errors';
import { ResultsContext } from 'context/results-context';
import * as React from 'react';

const ErrorPanel = () => {
  const { error } = React.useContext(ResultsContext);
  if (!error) {
    return (
      <div style={{
        display: 'flex',
        flexDirection: 'row',
        height: '100%',
        alignItems: 'center',
        justifyContent: 'center',
      }}
      >
        Everything is fine! No errors.
      </div>
    );
  }
  return <VizierErrorDetails error={error} />;
};

export default ErrorPanel;
