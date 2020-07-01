import * as infoImage from 'images/new-logo.svg';
import * as React from 'react';
import {
  createStyles, Theme, withStyles, WithStyles,
} from '@material-ui/core';

const styles = ({ palette, spacing }: Theme) => createStyles({
  root: {
    flexDirection: 'column',
    display: 'flex',

    background: palette.common.white,
    border: `1px solid ${palette.grey['300']}`,
    borderRadius: spacing(2),

    color: palette.common.black,
    fontSize: '14px',
  },
  content: {
    display: 'flex',
    flex: 1,
    flexDirection: 'column',
    paddingTop: spacing(30),
    paddingRight: spacing(40),
    paddingBottom: spacing(40),
    paddingLeft: spacing(40),
  },
  header: {
    display: 'flex',
    alignItems: 'center',
    height: spacing(20),
    borderBottom: `1px solid ${palette.grey['300']}`,
    padding: spacing(6),
  },
});

export interface DialogBoxProps extends WithStyles<typeof styles>{
  width?: number;
  children: any;
}

export const DialogBoxPlain = ({ classes, width, children }: DialogBoxProps) => {
  const style = width ? { width } : null;
  return (
    <div className={classes.root} style={style}>
      <div className={classes.header}>
        <img src={infoImage} style={{ width: '55px' }} />
      </div>
      <div className={classes.content}>
        {children}
      </div>
    </div>
  );
};

export const DialogBox = withStyles(styles)(DialogBoxPlain);
