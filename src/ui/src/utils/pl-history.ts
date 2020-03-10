import { createBrowserHistory } from 'history';
import analytics from 'utils/analytics';

const history = createBrowserHistory();

const showIntercom = (location) => {
  return location === '/login' || location === '/signup';
};

const sendPageEvent = (location) => {
  analytics.page(location.pathname, {},  {Intercom: {hideDefaultLauncher: showIntercom(window.location.pathname)}});
};

// Emit a page event for the first loaded page.
sendPageEvent(window.location);

history.listen((location, action) => {
  sendPageEvent(location);
});

export default history;
