import * as React from 'react';

import { CodeEditor } from 'pixie-components';

export default {
  title: 'CodeEditor',
  component: CodeEditor,
  decorators: [(Story) => <div style={{ height: '150px' }}><Story /></div>],
};

export const Basic = () => <CodeEditor shortcutKeys={[]} />;
