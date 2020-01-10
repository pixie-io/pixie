import './script-history.scss';

import {AccordionList} from 'components/accordion';
// @ts-ignore : TS does not like image files.
import * as clearIcon from 'images/icons/cross.svg';
import * as React from 'react';
import {FormControl} from 'react-bootstrap';
import {relativeTime} from 'utils/time';

export interface ScriptHistory {
  id: string;
  title: string;
  code: string;
  time: Date;
}

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

interface HistoryListProps {
  history: ScriptHistory[];
  onClick: (history: ScriptHistory) => void;
}

export const HistoryList = React.memo((props: HistoryListProps) => {
  const [value, setValue] = React.useState<string>('');
  const items = React.useMemo(() => {
    const history = !value ?
      props.history :
      props.history.filter((h) => new RegExp(value, 'i').test(h.title));

    return history.map((h) => ({
      title: (<HistoryEntry name={h.title} time={h.time} />),
      onClick: () => {
        props.onClick(h);
      },
    }));
  }, [props.history, value]);

  const valueChange = React.useCallback((e) => {
    setValue(e.target.value);
  }, []);

  const clearInput = React.useCallback(() => setValue(''), []);

  return (
    <>
      <div className='pixie-history-list-controls'>
        <div className='pixie-history-list-search-wrapper'>
          <FormControl
            placeholder='search'
            className='pixie-history-list-search-input'
            onChange={valueChange}
            size='sm'
            value={value}
          />
          <img
            className={`pixie-history-list-search-clear${!value ? '-hidden' : ''}`}
            src={clearIcon}
            onClick={clearInput}
          />
        </div>
      </div>
      <AccordionList items={items} />
    </>
  );
});
