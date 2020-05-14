import ClientContext from 'common/vizier-grpc-client-context';
import PlayIcon from 'components/icons/play';
import * as React from 'react';

import IconButton from '@material-ui/core/IconButton';
import Tooltip from '@material-ui/core/Tooltip';

import { ExecuteContext } from './context/execute-context';

const ExecuteScriptButton = () => {
  const client = React.useContext(ClientContext);
  const { execute } = React.useContext(ExecuteContext);
  return (
    <Tooltip title='Execute script'>
      <IconButton disabled={!client} onClick={() => execute()}>
        <PlayIcon />
      </IconButton>
    </Tooltip >
  );
};

export default ExecuteScriptButton;
