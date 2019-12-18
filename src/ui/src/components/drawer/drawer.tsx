import './drawer.scss';

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

export const Drawer =
  ({
    children,
    openedWidth = '10rem',
    closedWidth = '2rem',
    defaultOpened = true,
  }) => {
    const [opened, setOpened] = React.useState<boolean>(defaultOpened);
    const toggleOpened = React.useCallback(
      () => setOpened((isOpened) => !isOpened),
      [setOpened]);
    const styles = React.useMemo(() => ({
      width: opened ? openedWidth : closedWidth,
    }), [opened, openedWidth, closedWidth]);

    return (
      <div
        className='pixie-drawer'
        style={styles}
      >
        <div className={`pixie-drawer-content ${opened ? 'opened' : 'closed'}`}>
          <div>{children}</div>
        </div>
        <div className='pixie-drawer-footer-row'>
          <div className='spacer' />
          <Button size='sm' onClick={toggleOpened}>
            <img src={opened ? collapseLeft : collapseRight} />
          </Button>
        </div>
      </div>
    );
  };
