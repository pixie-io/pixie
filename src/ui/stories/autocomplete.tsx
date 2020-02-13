import Axios from 'axios';
import Input from 'components/autocomplete/input';
import * as React from 'react';

import {createStyles, makeStyles, Theme} from '@material-ui/core/styles';
import {storiesOf} from '@storybook/react';

storiesOf('AutoComplete', module)
  .add('Input', () => {
    const [suggestion, setSuggestion] = React.useState('');
    return (
      <Input
        suggestion={suggestion}
        onChange={(val) => {
          setSuggestion(val + 'world');
        }}
      />
    );
  }, {
    info: { inline: true },
    notes: 'an input box that shows the completion suggestion',
  })
  .add('dynamic suggestions', () => {
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
      />
    );
  }, {
    info: { inline: true },
    notes: 'this story shows how the suggestion works from a remote server',
  });
