import ClientContext from 'common/vizier-grpc-client-context';
import PlayIcon from 'components/icons/play';
import * as React from 'react';

import IconButton from '@material-ui/core/IconButton';
import Tooltip from '@material-ui/core/Tooltip';

import { ExecuteContext } from '../../context/execute-context';
import { ResultsContext } from '../../context/results-context';

const ExecuteScriptButton = () => {
  const { healthy } = React.useContext(ClientContext);
  const { execute } = React.useContext(ExecuteContext);
  const { loading } = React.useContext(ResultsContext);

  return (
    <Tooltip title={loading ? 'Executing' : !healthy ? 'Cluster Disconnected' : 'Execute script'}>
      <div>
        <IconButton disabled={!healthy || loading} onClick={() => execute()}>
          <PlayIcon />
        </IconButton>
      </div>
    </Tooltip>
  );
};

export default ExecuteScriptButton;
