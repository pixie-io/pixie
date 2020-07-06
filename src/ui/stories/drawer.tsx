import * as React from 'react';

import { storiesOf } from '@storybook/react';
import Button from '@material-ui/core/Button';
import FixedSizeDrawer from '../src/components/drawer/drawer';

storiesOf('Drawer', module)
  .add('FixedSizeDrawer', () => {
    const [open, setOpen] = React.useState(false);
    const toggleOpen = React.useCallback(() => setOpen((opened) => !opened), []);

    const otherContent = (
      <div>
        <Button onClick={toggleOpen} color='primary'>
          { open ? 'Close' : 'Open'}
        </Button>
        <div>
          Other content. Some text goes here.
        </div>
      </div>
    );

    return (
      <div>
        <FixedSizeDrawer
          drawerDirection='left'
          drawerSize='50px'
          open={open}
          otherContent={otherContent}
        >
          <div>
            Drawer contents
          </div>
        </FixedSizeDrawer>
      </div>
    );
  }, {
    info: { inline: true },
    notes: 'This is the basic fixed-size drawer, opening from the left',
  });
