import {shallow} from 'enzyme';
import {act} from 'react-dom/test-utils';

export async function shallowAsync(component) {
  let wrapper;
  await act(async () => {
    wrapper = shallow(component);
  });
  return wrapper;
}
