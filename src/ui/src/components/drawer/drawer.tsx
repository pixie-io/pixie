// @ts-ignore : TS does not like image files.
import * as collapseLeft from 'images/icons/collapse-left.svg';
// @ts-ignore : TS does not like image files.
import * as collapseRight from 'images/icons/collapse-right.svg';
import * as React from 'react';
import {Button} from 'react-bootstrap';

interface DrawerProps {
  defaultOpened?: boolean;
  openedWidth?: string;
  closedWidth?: string;
}

export const Drawer = React.memo<React.PropsWithChildren<DrawerProps>>(
  ({
    children,
    openedWidth = '10rem',
    closedWidth = '2rem',
    defaultOpened = true,
  }) => {
    const [opened, setOpened] = React.useState<boolean>(defaultOpened);
    const toggleOpened = () => setOpened((isOpened) => !isOpened);

    return (
      <div
        className={`pixie-drawer ${opened ? 'opened' : 'closed'}`}
        style={{
          display: 'flex',
          flexDirection: 'column',
          width: opened ? openedWidth : closedWidth,
        }}
      >
        <div
          className='pixie-drawer-content'
          style={{
            flex: 1,
            ...(opened ? {} : { visibility: 'hidden' }),
          }}
        >
          {children}
        </div>
        <div style={{ display: 'flex', flexDirection: 'row' }}>
          <div style={{ flex: 1 }} />
          <Button size='sm' onClick={toggleOpened}>
            <img src={opened ? collapseLeft : collapseRight} />
          </Button>
        </div>
      </div>
    );
  });
