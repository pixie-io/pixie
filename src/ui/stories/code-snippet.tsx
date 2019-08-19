import { action } from '@storybook/addon-actions';
import { storiesOf } from '@storybook/react';

import * as React from 'react';
import { CodeSnippet } from '../src/components/code-snippet/code-snippet';

storiesOf('CodeSnippet', module)
  .add('Basic', () => (
    <CodeSnippet showCopy={false} language=''>
      {'hello\nthis is some text\n'}
    </CodeSnippet>
  ), {
      info: { inline: true },
      notes: 'This is a code snippet without a copy button.',
    }).add('With copy', () => (
    <CodeSnippet showCopy={true} language=''>
      {'here is some longlonglonglonglonglonglonglonglonglonglonglonglong ' +
        'text that should go underneath the button\n a b c d e\n'}
    </CodeSnippet>
  ), {
      info: { inline: true },
      notes: 'This is a code snippet with a copy button.',
    }).add('With syntax highlighting', () => (
    <CodeSnippet showCopy={false} language='python'>
      {'def f():\n \tprint("hello")\n'}
    </CodeSnippet>
  ), {
      info: { inline: true },
      notes: 'This is a code snippet with syntax highlighting',
    });
