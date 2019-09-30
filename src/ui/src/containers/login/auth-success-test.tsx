import {DialogBox} from 'components/dialog-box/dialog-box';
import {shallow} from 'enzyme';
import * as React from 'react';
import {AuthSuccess} from './auth-success';

describe('<AuthSuccess/> test', () => {
  it('should show correct content', () => {
    const app = shallow(<AuthSuccess />);

    expect(app.find(DialogBox)).toHaveLength(1);
  });
});
