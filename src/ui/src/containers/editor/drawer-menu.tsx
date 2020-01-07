import {Accordion} from 'components/accordion';
import * as React from 'react';
import * as toml from 'toml';

// @ts-ignore : TS does not seem to like this import.
import * as PresetQueriesTOML from '../vizier/preset-queries.toml';

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
      name: s.name,
      onClick: () => {
        if (props.onSelect) {
          props.onSelect(s);
        }
      },
    })), []);
  return (<Accordion
    items={[
      {
        name: 'Example Scripts',
        key: 'example',
        children: presetQueries,
      },
    ]}
  />
  );
};

export default EditorDrawerMenu;
