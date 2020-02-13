import Axios from 'axios';
import Autocomplete from 'components/autocomplete';
import Completions from 'components/autocomplete/completions';
import Input from 'components/autocomplete/input';
import * as React from 'react';

import {action} from '@storybook/addon-actions';
import {storiesOf} from '@storybook/react';

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
        onChange={setInput}
        value={input}
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
        inputValue='script'
        items={[
          { header: 'Recently used' },
          { title: 'px/script1', id: 'px-0', highlights: [[3, 5]] },
          { title: 'px/script2', id: 'px-1', highlights: [[3, 5]] },
          { title: 'px/script3', id: 'px-2', highlights: [[3, 5]] },
          { title: 'px/script4', id: 'px-3', highlights: [[3, 5]] },
          { header: 'Org scripts' },
          { title: 'hulu/script1', id: 'hulu-4' },
          { title: 'hulu/script2', id: 'hulu-5' },
          { title: 'hulu/script3', id: 'hulu-6' },
          { title: 'hulu/script4', id: 'hulu-7' },
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
  .add('autocomplete component', () => {
    return (
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
          return resp.data.map((suggestion, i) => ({ title: suggestion.word, id: i }));
        }}
      />
    );
  }, {
    info: { inline: true },
    notes: 'completions list with active item',
  });
