import ClientContext from 'common/vizier-grpc-client-context';
import PlayIcon from 'components/icons/play';
import * as React from 'react';

import IconButton from '@material-ui/core/IconButton';
import Tooltip from '@material-ui/core/Tooltip';

import { ResultsContext } from 'context/results-context';
import { ScriptContext } from 'context/script-context';

const ExecuteScriptButton = ({ className }) => {
  const { healthy } = React.useContext(ClientContext);
  const { loading } = React.useContext(ResultsContext);
  const { saveEditorAndExecute } = React.useContext(ScriptContext);

  return (
    <Tooltip title={loading ? 'Executing' : !healthy ? 'Cluster Disconnected' : 'Execute script'}>
      <div>
        <IconButton disabled={!healthy || loading} onClick={saveEditorAndExecute}>
          <PlayIcon className={className} />
        </IconButton>
      </div>
    </Tooltip>
  );
};

export default ExecuteScriptButton;
