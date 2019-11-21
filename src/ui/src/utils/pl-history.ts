import { createBrowserHistory } from 'history';
import analytics from 'utils/analytics';

const history = createBrowserHistory();

history.listen((location, action) => {
  analytics.page();
});

export default history;
