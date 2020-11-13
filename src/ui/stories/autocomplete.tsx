import * as React from 'react';

import Axios from 'axios';

import {
  Autocomplete, AutocompleteInputField,
  Completions, FormFieldInput, Input,
} from 'pixie-components';

export default {
  title: 'Autocomplete/v1',
  component: Autocomplete,
  subcomponents: {
    AutocompleteInputField, Completions, FormFieldInput, Input,
  },
};

export const Basic = () => (
  <Autocomplete
    // eslint-disable-next-line
    onSelection={console.log}
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
);

export const InputField = () => {
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
};

export const InputWithHint = () => {
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
      // eslint-disable-next-line
      onKey={console.log}
      value={input}
    />
  );
};

export const BasicFormInput = () => {
  const [form, setForm] = React.useState({ field1: 'value1', field2: 'value2', field3: 'value3' });
  return (
    <FormFieldInput
      form={form}
      onValueChange={([field, value]) => {
        setForm((oldForm) => ({
          ...oldForm,
          [field]: value,
        }));
      }}
    />
  );
};

export const BasicCompletions = () => {
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
      // eslint-disable-next-line
      onSelection={console.log}
    />
  );
};
