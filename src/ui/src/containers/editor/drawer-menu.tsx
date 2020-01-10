import {SCRIPT_HISTORY} from 'common/local-gql';
import {Accordion, AccordionList} from 'components/accordion';
import * as React from 'react';
import * as toml from 'toml';

import {useQuery} from '@apollo/react-hooks';

// @ts-ignore : TS does not seem to like this import.
import * as PresetQueriesTOML from '../vizier/preset-queries.toml';
import {HistoryList, ScriptHistory} from './script-history';

interface EditorDrawerMenuProps {
  onSelect: (script: Script) => void;
}

interface Script {
  id?: string;
  title: string;
  code: string;
}

const PRESET_QUERIES: Script[] = toml.parse(PresetQueriesTOML).queries.map(
  (query) => ({ title: query[0], code: query[1] }));

const EditorDrawerMenu = (props: EditorDrawerMenuProps) => {
  const { data: historyData } = useQuery<{ scriptHistory: ScriptHistory[] }>(SCRIPT_HISTORY);
  const accordionItem = React.useMemo(() => {
    const presetQueries = PRESET_QUERIES.map((s) => ({
      title: s.title,
      onClick: () => {
        if (props.onSelect) {
          props.onSelect(s);
        }
      },
    }));
    const historyEntries = historyData && historyData.scriptHistory || [];
    return [
      {
        title: 'Example Scripts',
        key: 'example',
        content: <AccordionList items={presetQueries} />,
      },
      {
        title: 'Script History',
        key: 'history',
        content: <HistoryList history={historyEntries} onClick={props.onSelect} />,
      },
    ];
  }, [historyData]);

  return (<Accordion items={accordionItem} />);
};

export default EditorDrawerMenu;
