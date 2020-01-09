import {SCRIPT_HISTORY} from 'common/local-gql';
import {Accordion} from 'components/accordion';
import * as React from 'react';
import * as toml from 'toml';

import {useQuery} from '@apollo/react-hooks';

// @ts-ignore : TS does not seem to like this import.
import * as PresetQueriesTOML from '../vizier/preset-queries.toml';
import HistoryEntry from './script-history';

interface EditorDrawerMenuProps {
  onSelect?: (script: Script) => void;
}

interface Script {
  id?: string;
  name: string;
  code: string;
}

const PRESET_QUERIES: Script[] = toml.parse(PresetQueriesTOML).queries.map(
  (query) => ({ name: query[0], code: query[1] }));

const EditorDrawerMenu = (props: EditorDrawerMenuProps) => {
  const presetQueries = React.useMemo(() =>
    PRESET_QUERIES.map((s) => ({
      title: s.name,
      onClick: () => {
        if (props.onSelect) {
          props.onSelect(s);
        }
      },
    })), []);

  const { data: historyData } = useQuery(SCRIPT_HISTORY);
  const accordionItem = React.useMemo(() => {
    const historyEntries = historyData && historyData.scriptHistory || [];
    const historyMenuItems = historyEntries.map((history) => ({
      title: (<HistoryEntry name={history.title} time={history.time} />),
      onClick: () => {
        if (props.onSelect) {
          props.onSelect({
            id: history.id,
            name: history.title,
            code: history.code,
          });
        }
      },
    }));
    return [
      {
        title: 'Example Scripts',
        key: 'example',
        children: presetQueries,
      },
      {
        title: 'Script History',
        key: 'history',
        children: historyMenuItems,
      },
    ];
  }, [historyData]);

  return (<Accordion items={accordionItem} />);
};

export default EditorDrawerMenu;
