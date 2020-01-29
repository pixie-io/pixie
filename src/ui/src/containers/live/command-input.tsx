import * as React from 'react';

import {createStyles, makeStyles, Theme} from '@material-ui/core';
import Card from '@material-ui/core/Card';
import Modal from '@material-ui/core/Modal';

interface CommandInputProps {
  open: boolean;
  onClose: () => void;
}

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    input: {
      position: 'absolute',
      height: '200px',
      width: '500px',
      top: '50%',
      left: '50%',
      transform: 'translate(-50%, -50%)',
    },
  }),
);

// TODO(malthus): Figure out the lifecycle of this component. When a command is selected,
// should the component clear the input? What about when the input is dismised?

const CommandInput: React.FC<CommandInputProps> = ({ open, onClose }) => {
  const classes = useStyles();

  return (
    <Modal open={open} onClose={onClose} BackdropProps={{ invisible: true }}>
      <Card className={classes.input}>
        autocomplete goes here
        <input autoFocus />
      </Card>
    </Modal>
  );
};

export default CommandInput;
