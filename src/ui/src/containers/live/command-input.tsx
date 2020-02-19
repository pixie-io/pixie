import Autocomplete from 'components/autocomplete/autocomplete';
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

const MOCK_COMMANDS = [
  'px/service_info',
  'px/service_stats',
  'px/network_stats',
  'px/namespace_stats',
  'service_info',
  'network_info',
];

// TODO(malthus): Figure out the lifecycle of this component. When a command is selected,
// should the component clear the input? What about when the input is dismised?

const CommandInput: React.FC<CommandInputProps> = ({ open, onClose }) => {
  const classes = useStyles();

  return (
    <Modal open={open} onClose={onClose} BackdropProps={{ invisible: true }}>
      <Card className={classes.input}>
        <Autocomplete
          placeholder='Pixie Command'
          onSelection={(id) => {
            onClose();
          }}
          getCompletions={(input) => {
            if (!input) {
              return Promise.resolve([]);
            }
            return new Promise((resolve) => {
              setTimeout(() => {
                const commands = MOCK_COMMANDS.map((cmd, i) => ({
                  title: `${cmd} ${input}`,
                  id: String(i),
                }));
                const results = [{ header: 'Most Recent' }, ...commands];
                results.splice(4, 0, { header: 'Recently Used' });
                resolve(results);
              }, 50);
            });
          }}
        />
      </Card>
    </Modal>
  );
};

export default CommandInput;
