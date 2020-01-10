import './script-history.scss';

import * as React from 'react';
import {relativeTime} from 'utils/time';

interface HistoryEntryProps {
  name: string;
  time: Date;
}

const HistoryEntry = (props: HistoryEntryProps) => {
  const time = React.useMemo(() => relativeTime(props.time), [props.time]);
  return (
    <div className='pixie-history-entry'>
      <span className='pixie-history-title'>{props.name}</span>
      <span className='pixie-history-time'>({time})</span>
    </div>
  );
};

export default HistoryEntry;
