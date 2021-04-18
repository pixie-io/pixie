/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import * as React from 'react';

import Axios from 'axios';

import { Autocomplete } from './autocomplete';
import { Completions } from './completions';
import { FormFieldInput } from './form';
import { Input } from './input';

export default {
  title: 'Autocomplete/v1',
  component: Autocomplete,
  subcomponents: {
    Completions,
    FormFieldInput,
    Input,
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
  const [form, setForm] = React.useState({
    field1: 'value1',
    field2: 'value2',
    field3: 'value3',
  });
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
          type: 'item',
          title: 'px/script1',
          id: 'px-0',
          highlights: [3, 4, 5],
        },
        {
          type: 'item',
          title: 'px/script2',
          id: 'px-1',
          highlights: [3, 4, 5],
        },
        {
          type: 'item',
          title: 'px/script3',
          id: 'px-2',
          highlights: [3, 4, 5],
        },
        {
          type: 'item',
          title: 'px/script4',
          id: 'px-3',
          highlights: [3, 4, 5],
        },
        { type: 'header', header: 'Org scripts' },
        {
          type: 'item',
          title: 'my-org/script1',
          id: 'my-org-4',
          description: 'cool script',
        },
        {
          type: 'item',
          title: 'my-org/script2',
          id: 'my-org-5',
          description: 'another cool script',
        },
        { type: 'item', title: 'my-org/script3', id: 'my-org-6' },
        { type: 'item', title: 'my-org/script4', id: 'my-org-7' },
      ]}
      onActiveChange={setActive}
      activeItem={active}
      // eslint-disable-next-line
      onSelection={console.log}
    />
  );
};
