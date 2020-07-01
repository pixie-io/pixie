import Axios from 'axios';
import Autocomplete from 'components/autocomplete';
import AutocompleteInputField from 'components/autocomplete/autocomplete-field';
import Completions from 'components/autocomplete/completions';
import FormInput from 'components/autocomplete/form';
import Input from 'components/autocomplete/input';
import * as React from 'react';
import { AutocompleteInput } from 'components/autocomplete/new-autocomplete-input';
import { NewAutocomplete } from 'components/autocomplete/new-autocomplete';
import { action } from '@storybook/addon-actions';
import { storiesOf } from '@storybook/react';

storiesOf('AutoComplete', module)
  .add('input with hint', () => {
    const [suggestion, setSuggestion] = React.useState('');
    const [input, setInput] = React.useState('');
    React.useEffect(() => {
      if (!input) {
        return;
      }
      Axios({
        method: 'get',
        url: 'https://api.datamuse.com/sug',
        params: { s: input },
      }).then(({ data, status }) => {
        if (status !== 200) {
          return;
        }
        setSuggestion(data[0].word);
      });
    }, [input]);
    return (
      <Input
        suggestion={suggestion}
        placeholder='start typing'
        onChange={setInput}
        onKey={action('key')}
        value={input}
      />
    );
  }, {
    info: { inline: true },
    notes: 'this story shows how the suggestion works from a remote server',
  })
  .add('form input', () => {
    const [form, setForm] = React.useState({ field1: 'value1', field2: 'value2', field3: 'value3' });
    return (
      <FormInput
        form={form}
        onValueChange={([field, value]) => {
          setForm((oldForm) => ({
            ...oldForm,
            [field]: value,
          }));
        }}
      />
    );
  }, {
    info: { inline: true },
    notes: 'this story shows how the suggestion works from a remote server',
  })
  .add('completions list', () => {
    const [active, setActive] = React.useState('');
    return (
      <Completions
        items={[
          { type: 'header', header: 'Recently used' },
          {
            type: 'item', title: 'px/script1', id: 'px-0', highlights: [3, 4, 5],
          },
          {
            type: 'item', title: 'px/script2', id: 'px-1', highlights: [3, 4, 5],
          },
          {
            type: 'item', title: 'px/script3', id: 'px-2', highlights: [3, 4, 5],
          },
          {
            type: 'item', title: 'px/script4', id: 'px-3', highlights: [3, 4, 5],
          },
          { type: 'header', header: 'Org scripts' },
          {
            type: 'item', title: 'hulu/script1', id: 'hulu-4', description: 'cool script',
          },
          {
            type: 'item', title: 'hulu/script2', id: 'hulu-5', description: 'another cool script',
          },
          { type: 'item', title: 'hulu/script3', id: 'hulu-6' },
          { type: 'item', title: 'hulu/script4', id: 'hulu-7' },
        ]}
        onActiveChange={setActive}
        activeItem={active}
        onSelection={action('selected')}
      />
    );
  }, {
    info: { inline: true },
    notes: 'completions list with active item',
  })
  .add('autocomplete component', () => (
    <Autocomplete
      onSelection={action('selection')}
      getCompletions={async (input) => {
        if (!input) {
          return [];
        }
        const resp = await Axios({
          method: 'get',
          url: 'https://api.datamuse.com/sug',
          params: { s: input },
        });

        if (resp.status !== 200) {
          return [];
        }
        return [
          { type: 'header', header: 'Suggested words' },
          ...resp.data.map((suggestion, i) => ({
            type: 'item',
            title: suggestion.word,
            id: i,
            description: `score: ${suggestion.score}`,
          })),
        ];
      }}
    />
  ), {
    info: { inline: true },
    notes: 'completions list with active item',
  })
  .add('autocomplete field component', () => {
    const [value, setValue] = React.useState('');
    return (
      <AutocompleteInputField
        name='An autocomplete input'
        value={value}
        onEnterKey={() => { }}
        onValueChange={setValue}
        getCompletions={async (input) => {
          // Add some fake delay to the API call.
          await new Promise((resolve) => setTimeout(resolve, 2000));
          if (!input) {
            return [];
          }
          return [
            { type: 'header', header: 'suggestions' },
            {
              type: 'item',
              title: 'some suggestion',
              id: '1',
              highlights: [0, 1],
            },
            {
              type: 'item',
              title: 'awesome',
              id: '2',
              highlights: [3, 4],
            },
          ];
        }}
      />
    );
  }, {
    info: { inline: true },
    notes: 'completions list with active item',
  })
  .add('new autocomplete input component', () => {
    const [cursor, setCursor] = React.useState(0);
    return (
      <AutocompleteInput
        // eslint-disable-next-line
        onKey={(key) => { }}
        // eslint-disable-next-line
        onChange={(val, cursor)=> { }}
        setCursor={(pos) => {
          setCursor(pos);
        }}
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
  }, {
    info: { inline: true },
    notes: 'new autocomplete input component',
  })
  .add('new autocomplete component', () => (
    <NewAutocomplete
      onSubmit={() => {}}
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
          {
            Index: 4, Value: 'test', CursorPosition: -1,
          },
        ]
      }
    />
  ), {
    info: { inline: true },
    notes: 'new autocomplete component',
  });
