import { mount } from 'enzyme';
import * as React from 'react';
import Highlight from 'react-highlight/lib/optimized';

import { CodeSnippet } from './code-snippet';

describe.skip('<CodeSnippet/> test', () => {
  it('should show correct contents', () => {
    const wrapper = mount(<CodeSnippet
      showCopy={false}
      language=''
    >
      {'Here is some text.'}
    </CodeSnippet>);
    expect(wrapper.text()).toEqual('Here is some text.');
  });

  it('should not show copy button', () => {
    const wrapper = mount(<CodeSnippet
      showCopy={false}
      language=''
    >
      {'Here is some text.'}
    </CodeSnippet>);
    expect(wrapper.find('.code-snippet--copy-button')).toHaveLength(0);
  });

  it('should should show copy button', () => {
    const wrapper = mount(<CodeSnippet
      showCopy={true}
      language=''
    >
      {'Here is some text.'}
    </CodeSnippet>);
    expect(wrapper.find('.code-snippet--copy-button')).toHaveLength(1);
  });

  it('should use highlighted syntax ', () => {
    const wrapper = mount(<CodeSnippet
      showCopy={false}
      language='python'
    >
      {'Here is some text.'}
    </CodeSnippet>);
    expect(wrapper.find(Highlight)).toHaveLength(1);
  });

  it('should not use highlighted syntax ', () => {
    const wrapper = mount(<CodeSnippet
      showCopy={false}
      language=''
    >
      {'Here is some text.'}
    </CodeSnippet>);
    expect(wrapper.find(Highlight)).toHaveLength(0);
  });
});
