import { createBrowserHistory } from 'history';
import analytics from 'utils/analytics';

const history = createBrowserHistory();

function showIntercom(path: string): boolean {
  return path === '/auth/login' || path === '/auth/signup';
}

function sendPageEvent(path: string) {
  analytics.page(
    '', // category
    path, // name
    {}, // properties
    {
      integrations: {
        Intercom: { hideDefaultLauncher: !showIntercom(path) },
      },
    }, // options
  );
}

// Emit a page event for the first loaded page.
sendPageEvent(window.location.pathname);

history.listen((location) => {
  sendPageEvent(location.pathname);
});

export default history;
