import * as React from 'react';
import {ListGroup, ListGroupItem} from 'react-bootstrap';
import * as toml from 'toml';

// @ts-ignore : TS does not seem to like this import.
import * as PresetQueriesTOML from '../vizier/preset-queries.toml';

interface PresetQueriesProps {
  onQuerySelect?: (query: PresetQuery) => void;
}

export interface PresetQuery {
  name: string;
  code: string;
}

const PRESET_QUERIES: PresetQuery[] = toml.parse(PresetQueriesTOML).queries.map(
  (query) => ({ name: query[0], code: query[1] }));

export const PresetQueries = React.memo<PresetQueriesProps>((props) => {
  const getHandler = (query: PresetQuery) => (() => {
    if (props.onQuerySelect) {
      props.onQuerySelect(query);
    }
  });
  return (
    <ListGroup>
      {PRESET_QUERIES.map((query) => (
        <ListGroupItem
          action
          key={query.name}
          onClick={getHandler(query)}
        >
          {query.name}
        </ListGroupItem>
      ))}
    </ListGroup>
  );
});
