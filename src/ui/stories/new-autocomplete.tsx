import * as React from 'react';
import { AutocompleteInput } from 'components/autocomplete/new-autocomplete-input';
import { NewAutocomplete } from 'components/autocomplete/new-autocomplete';

export default {
  title: 'Autocomplete/v2',
  component: NewAutocomplete,
  subcomponents: { AutocompleteInput },
};

export const Basic = () => (
  <NewAutocomplete
    isValid={false}
    onSubmit={() => { }}
  // eslint-disable-next-line
  onChange={(input, cursor, action, updatedTabStops) => { }}
    completions={[
      {
        index: 2,
        executableAfterSelect: false,
        suggestions: [
          {
            type: 'item', title: 'hulu/script3', id: 'hulu-6', itemType: 'script',
          },
          {
            type: 'item', title: 'hulu/script5', id: 'hulu-5', itemType: 'script',
          },
        ],
      },
      {
        index: 3,
        executableAfterSelect: false,
        suggestions: [
          {
            type: 'item', title: 'hulu/script4', id: 'hulu-4', itemType: 'script',
          },
        ],
      },
      {
        index: 4,
        executableAfterSelect: false,
        suggestions: [
          {
            type: 'item', title: 'test', id: 'test-1', itemType: 'svc',
          },
        ],
      },
    ]}
    tabStops={
    [
      {
        Index: 2, Label: 'svc_name', Value: 'pl/test', CursorPosition: 0,
      },
      {
        Index: 3, Label: 'pod', Value: 'pl/pod', CursorPosition: -1,
      },
      { Index: 4, Value: 'test', CursorPosition: -1 },
    ]
  }
  />
);

export const InputOnly = () => {
  const [cursor, setCursor] = React.useState(0);
  return (
    <AutocompleteInput
    // eslint-disable-next-line
    onKey={(key) => { }}
    // eslint-disable-next-line
    onChange={(val, cursor) => { }}
      setCursor={(pos) => {
        setCursor(pos);
      }}
      isValid={false}
      cursorPos={cursor}
      value={
      [{
        type: 'key',
        value: 'svc: ',
      },
      {
        type: 'value',
        value: 'pl/test',
      }]
    }
    />
  );
};
