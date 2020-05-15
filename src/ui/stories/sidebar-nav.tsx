import * as React from 'react';
import { BrowserRouter as Router } from 'react-router-dom';

import { storiesOf } from '@storybook/react';

import * as codeImage from '../assets/images/icons/agent.svg';
import * as infoImage from '../assets/images/icons/query.svg';
import * as logoImage from '../assets/images/logo.svg';
import { SidebarNav } from '../src/components/sidebar-nav/sidebar-nav';

storiesOf('SidebarNav', module)
  .add('Basic', () => (
    <Router>
      <SidebarNav
        logo={logoImage}
        items={[
          { link: '/', selectedImg: infoImage, unselectedImg: codeImage },
          { link: '/test', selectedImg: codeImage, unselectedImg: infoImage },
        ]}
        footerItems={[
          { link: '/footer', selectedImg: infoImage, unselectedImg: codeImage },
          { selectedImg: infoImage, unselectedImg: codeImage, menu: { a: '/a-link', b: '/another-link' } },
        ]}
      />
    </Router>
  ), {
    info: { inline: true },
    notes: 'This is a sidebar nav that will provide navigation to different pages in our UI. '
      + 'It must be nested within a React Router. Clicking an icon should route to a different page, and update the '
      + 'selected path\'s icon. (Note: this does not work here because the React router does not work in storybook).',
  });
