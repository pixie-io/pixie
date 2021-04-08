import * as React from 'react';
import { CommandAutocompleteInput } from './command-autocomplete-input';
import { CommandAutocomplete } from './command-autocomplete';

export default {
  title: 'Autocomplete/v2',
  component: CommandAutocomplete,
  subcomponents: { AutocompleteInput: CommandAutocompleteInput },
};

export const Basic = () => (
  <CommandAutocomplete
    isValid={false}
    onSubmit={() => {}}
    // eslint-disable-next-line
    onChange={() => {}}
    completions={[
      {
        index: 2,
        executableAfterSelect: false,
        suggestions: [
          {
            type: 'item',
            title: 'my-org/script3',
            id: 'my-org-6',
            itemType: 'script',
          },
          {
            type: 'item',
            title: 'my-org/script5',
            id: 'my-org-5',
            itemType: 'script',
          },
        ],
      },
      {
        index: 3,
        executableAfterSelect: false,
        suggestions: [
          {
            type: 'item',
            title: 'my-org/script4',
            id: 'my-org-4',
            itemType: 'script',
          },
        ],
      },
      {
        index: 4,
        executableAfterSelect: false,
        suggestions: [
          {
            type: 'item',
            title: 'test',
            id: 'test-1',
            itemType: 'svc',
          },
        ],
      },
    ]}
    tabStops={[
      {
        Index: 2,
        Label: 'svc_name',
        Value: 'pl/test',
        CursorPosition: 0,
      },
      {
        Index: 3,
        Label: 'pod',
        Value: 'pl/pod',
        CursorPosition: -1,
      },
      { Index: 4, Value: 'test', CursorPosition: -1 },
    ]}
  />
);

export const InputOnly = () => {
  const [cursor, setCursor] = React.useState(0);
  return (
    <CommandAutocompleteInput
      // eslint-disable-next-line
      onKey={() => {}}
      // eslint-disable-next-line
      onChange={() => {}}
      setCursor={(pos) => {
        setCursor(pos);
      }}
      isValid={false}
      cursorPos={cursor}
      value={[
        {
          type: 'key',
          value: 'svc: ',
        },
        {
          type: 'value',
          value: 'pl/test',
        },
      ]}
    />
  );
};
