import * as React from 'react';
import Card from '@material-ui/core/Card';
import Modal from '@material-ui/core/Modal';
import Button from '@material-ui/core/Button';
import Grid from '@material-ui/core/Grid';

import {
  createStyles, Theme, withStyles, WithStyles,
} from '@material-ui/core/styles';
import { MutationInfo, LifeCycleState } from 'types/generated/vizierapi_pb';
import { StatusCell } from 'pixie-components';
import ListItem from '@material-ui/core/ListItem';
import ListItemIcon from '@material-ui/core/ListItemIcon';
import ListItemText from '@material-ui/core/ListItemText';
import { CircularProgress } from '@material-ui/core';
import { ScriptContext } from 'context/script-context';

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
    display: 'flex',
    justifyContent: 'center',
  },
  image: {
    width: spacing(16),
  },
  icon: {
    minWidth: spacing(4),
  },
  mutation: {
    paddingTop: 0,
    paddingBottom: 0,
  },
  schema: {
    paddingTop: spacing(1),
    paddingBottom: spacing(1),
    paddingLeft: spacing(5),
  },
  spinner: {
    width: spacing(5),
    height: spacing(5),
  },
  states: {
    paddingBottom: spacing(2),
  },
  leftStates: {
    '& > li': {
      paddingLeft: spacing(5),
    },
  },
  cancelButton: {
    display: 'flex',
    flexDirection: 'row-reverse',
    alignSelf: 'flex-end',
    paddingBottom: spacing(1),
  },
  schemaText: {
    flex: 'none',
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

const MutationModal = ({ classes, mutationInfo }: MutationModalProps) => {
  const { cancelExecution } = React.useContext(ScriptContext);

  return (
    <Modal open>
      <Card className={classes.mutationCard}>
        <Grid container spacing={3}>
          <Grid item xs={12}>
            <div className={classes.cardHeader}>
              Deploying Tracepoints
            </div>
          </Grid>
          <Grid item xs={6} className={classes.leftStates}>
            {
               mutationInfo.getStatesList().slice(0, Math.ceil(mutationInfo.getStatesList().length)).map((mutation) => (
                 <ListItem className={classes.mutation} key={mutation.getId()}>
                   <ListItemIcon className={classes.icon}>
                     <MutationState state={mutation.getState()} classes={classes} />
                   </ListItemIcon>
                   <ListItemText>{mutation.getName()}</ListItemText>
                 </ListItem>
               ))
            }
          </Grid>
          <Grid item xs={6}>
            {
              mutationInfo.getStatesList().slice(Math.ceil(mutationInfo.getStatesList().length)).map((mutation) => (
                <ListItem className={classes.mutation} key={mutation.getId()}>
                  <ListItemIcon className={classes.icon}>
                    <MutationState state={mutation.getState()} classes={classes} />
                  </ListItemIcon>
                  <ListItemText>{mutation.getName()}</ListItemText>
                </ListItem>
              ))
            }
          </Grid>
          <Grid item xs={12}>
            {
              (true || mutationInfo.getStatus().getMessage().includes('Schema'))
              && (
                <ListItem className={classes.schema} key='schema'>
                  <ListItemIcon className={classes.icon}>
                    <MutationState state={LifeCycleState.PENDING_STATE} classes={classes} />
                  </ListItemIcon>
                  <ListItemText className={classes.schemaText}>Prepare schema</ListItemText>
                </ListItem>
              )
            }
          </Grid>
          <Grid item xs={6}>
            <img className={classes.image} src={moonwalkerSVG} />
          </Grid>
          <Grid item xs={6} className={classes.cancelButton}>
            <Button onClick={cancelExecution}>
              Cancel
            </Button>
          </Grid>
        </Grid>
      </Card>
    </Modal>
  );
};

export default withStyles(styles)(MutationModal);
