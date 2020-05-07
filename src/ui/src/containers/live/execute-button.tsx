import PlayIcon from 'components/icons/play';
import * as React from 'react';

import IconButton from '@material-ui/core/IconButton';
import Tooltip from '@material-ui/core/Tooltip';

import { LiveContext } from './context';

const ExecuteScriptButton = () => {
  const { vizierReady, executeScript } = React.useContext(LiveContext);
  return (
    <Tooltip title='Execute script'>
      <IconButton disabled={!vizierReady} onClick={() => executeScript()}>
        <PlayIcon />
      </IconButton>
    </Tooltip >
  );
};

export default ExecuteScriptButton;
