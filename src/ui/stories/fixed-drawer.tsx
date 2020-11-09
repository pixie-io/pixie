import * as React from 'react';

import Button from '@material-ui/core/Button';
import { FixedSizeDrawer } from '../src/components/drawer/drawer';

export default {
  title: 'Drawer/Fixed',
  component: FixedSizeDrawer,
};

export const Basic = () => {
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
    <FixedSizeDrawer
      drawerDirection='left'
      drawerSize='250px'
      open={open}
      otherContent={otherContent}
      overlay={false}
    >
      <div>
        Drawer contents
      </div>
    </FixedSizeDrawer>
  );
};
