import * as React from 'react';
import Card from '@material-ui/core/Card';
import Modal from '@material-ui/core/Modal';

import {
  createStyles, Theme, withStyles, WithStyles,
} from '@material-ui/core/styles';
import { MutationInfo, LifeCycleState } from 'types/generated/vizier_pb';
import { StatusCell } from 'components/status/status';
import ListItem from '@material-ui/core/ListItem';
import ListItemIcon from '@material-ui/core/ListItemIcon';
import ListItemText from '@material-ui/core/ListItemText';
import { CircularProgress } from '@material-ui/core';

import * as moonwalkerSVG from '../../../assets/images/moonwalker.svg';

const styles = ({ spacing, typography, palette }: Theme) => createStyles({
  mutationDisplay: {
    position: 'absolute',
    width: '100%',
    height: '100%',
    zIndex: 1,
  },
  mutationCard: {
    width: '40%',
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    flexDirection: 'column',
    padding: spacing(2),
    paddingBottom: spacing(3),
    transform: 'translate(65%, 20vh)',
  },
  cardHeader: {
    ...typography.h6,
    color: palette.foreground.two,
    paddingBottom: spacing(2),
  },
  image: {
    width: spacing(30),
    paddingBottom: spacing(3),
  },
  icon: {
    minWidth: spacing(4),
  },
  mutation: {
    paddingTop: 0,
    paddingBottom: 0,
  },
  spinner: {
    width: spacing(5),
    height: spacing(5),
  },
  states: {
    paddingBottom: spacing(2),
  },
});

interface MutationModalProps extends WithStyles<typeof styles> {
  mutationInfo: MutationInfo;
}

const MutationState = (props) => {
  switch (props.state) {
    case LifeCycleState.RUNNING_STATE:
      return (<StatusCell statusGroup='healthy' />);
    case LifeCycleState.FAILED_STATE:
      return (<StatusCell statusGroup='unhealthy' />);
    default:
      break;
  }

  return <CircularProgress size={18} />;
};

const MutationModal = ({ classes, mutationInfo }: MutationModalProps) => (
  <Modal open>
    <Card className={classes.mutationCard}>
      <div className={classes.cardHeader}>
        Deploying Tracepoints
      </div>
      <img className={classes.image} src={moonwalkerSVG} />
      <div className={classes.states}>
        {
          mutationInfo.getStatesList().map((mutation) => (
            <ListItem className={classes.mutation} key={mutation.getId()}>
              <ListItemIcon className={classes.icon}>
                <MutationState state={mutation.getState()} classes={classes} />
              </ListItemIcon>
              <ListItemText>{mutation.getName()}</ListItemText>
            </ListItem>
          ))
        }
      </div>
      { mutationInfo.getStatus().getMessage().includes('Schema')
        ? 'Waiting for schema to initialize...' : '' }
    </Card>
  </Modal>
);

export default withStyles(styles)(MutationModal);
