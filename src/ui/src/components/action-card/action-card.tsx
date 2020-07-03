import * as React from 'react';
import {
  createStyles, Theme, WithStyles, withStyles,
} from '@material-ui/core/styles';
import {
  Card, CardContent, Divider, Typography,
} from '@material-ui/core';
import * as logoImage from 'images/pixie-logo.svg';

const styles = ({ spacing }: Theme) => createStyles({
  root: {
  },
  title: {
    padding: spacing(2),
    paddingBottom: spacing(5),
  },
  section1: {
    paddingTop: spacing(15),
    paddingBottom: spacing(15),
    paddingLeft: spacing(2),
    paddingRight: spacing(2),
  },
  bottom: {
    display: 'flex',
    paddingTop: spacing(2),
    paddingRight: 0,
    justifyContent: 'flex-end',
  },

});

interface ActionCardProps extends WithStyles<typeof styles> {
  title: string;
  children?: React.ReactNode;
  width?: string;
  minWidth?: string;
}

// ActionCard defines a simple card that used as a CTA card within Pixie.
// It consists of a title, body and logo on the bottom of the card.
const ActionCard = ({
  title, children, classes, width, minWidth,
}: ActionCardProps) => (
  <Card className={classes.root} style={{ width, minWidth }}>
    <CardContent>
      <div className={classes.title}>
        <Typography variant='h6' color='textSecondary'>{title}</Typography>
      </div>
      <Divider variant='middle' />
      <div className={classes.section1}>
        {children}
      </div>
      <Divider variant='middle' />
      <div className={classes.bottom}>
        <img src={logoImage} style={{ width: '55px' }} />
      </div>
    </CardContent>
  </Card>
);

export default withStyles(styles)(ActionCard);
