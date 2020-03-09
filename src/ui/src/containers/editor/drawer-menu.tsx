import {SCRIPT_HISTORY} from 'common/local-gql';
import {Accordion, AccordionList} from 'components/accordion';
import * as React from 'react';
import * as toml from 'toml';
import {GetPxScripts, Script} from 'utils/script-bundle';

import {useQuery} from '@apollo/react-hooks';

import {HistoryList, ScriptHistory} from './script-history';

interface EditorDrawerMenuProps {
  onSelect: (script: Script) => void;
}

export const EditorDrawerMenu = (props: EditorDrawerMenuProps) => {
  const [exampleScripts, setExampleScripts] = React.useState<Script[]>([]);

  React.useEffect(() => {
    GetPxScripts(setExampleScripts);
  }, []);

  const { data: historyData } = useQuery<{ scriptHistory: ScriptHistory[] }>(SCRIPT_HISTORY);
  const accordionItem = React.useMemo(() => {
    const presetQueries = exampleScripts.map((s) => ({
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
  }, [historyData, exampleScripts]);

  return (<Accordion items={accordionItem} />);
};
