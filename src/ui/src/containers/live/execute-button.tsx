import PlayIcon from 'components/icons/play';
import * as React from 'react';

import IconButton from '@material-ui/core/IconButton';

import {LiveContext} from './context';

const ExecuteScriptButton = () => {
  const { vizierReady, executeScript, executeScriptOld } = React.useContext(LiveContext);
  const { oldLiveViewMode } = React.useContext(LiveContext);

  return (
    <IconButton disabled={!vizierReady} onClick={() => (oldLiveViewMode ? executeScriptOld() : executeScript())}>
      <PlayIcon />
    </IconButton>
  );
};

export default ExecuteScriptButton;
