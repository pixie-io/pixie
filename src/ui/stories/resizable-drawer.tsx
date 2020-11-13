import * as React from 'react';

import Button from '@material-ui/core/Button';
import { ResizableDrawer } from 'pixie-components';

export default {
  title: 'Drawer/Resizable',
  component: ResizableDrawer,
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
    <ResizableDrawer
      drawerDirection='left'
      open={open}
      otherContent={otherContent}
      initialSize={200}
      overlay={false}
    >
      <div>
        Drawer contents
      </div>
    </ResizableDrawer>
  );
};
