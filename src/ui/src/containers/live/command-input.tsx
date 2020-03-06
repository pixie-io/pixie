import Autocomplete from 'components/autocomplete/autocomplete';
import {
    CompletionHeader, CompletionItem, CompletionItems,
} from 'components/autocomplete/completions';
import MagicIcon from 'components/icons/magic';
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
    card: {
      position: 'absolute',
      width: '40vw',
      top: '40%',
      left: '50%',
      transform: 'translate(-50%, -10vh)',
    },
    input: {
      maxHeight: '40vh',
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
  'cpu_info',
  'memory_info',
  'service_info',
  'network_info',
  'cpu_info',
  'memory_info',
];

const DESCRIPTION = `
  Lorem ipsum dolor sit amet, consectetur adipiscing elit. Vestibulum aliquet tincidunt ligula. \
  Vivamus congue odio sed nisi volutpat venenatis. Morbi pulvinar neque nisl, in ornare massa \
  pellentesque viverra. Pellentesque auctor suscipit ex, quis dignissim ligula. Mauris urna purus, \
`;

// TODO(malthus): Figure out the lifecycle of this component. When a command is selected,
// should the component clear the input? What about when the input is dismised?

const CommandInput: React.FC<CommandInputProps> = ({ open, onClose }) => {
  const classes = useStyles();

  return (
    <Modal open={open} onClose={onClose} BackdropProps={{}}>
      <Card className={classes.card}>
        <Autocomplete
          className={classes.input}
          placeholder='Pixie Command'
          prefix={<MagicIcon />}
          onSelection={(id) => {
            onClose();
          }}
          getCompletions={(input) => {
            if (!input) {
              return Promise.resolve([]);
            }
            return new Promise((resolve) => {
              setTimeout(() => {
                const completions: CompletionItems = MOCK_COMMANDS.map((cmd, i) => ({
                  type: 'item',
                  title: `${cmd} ${input}`,
                  id: String(i),
                  description: `${cmd} ${DESCRIPTION}`,
                }));
                completions.splice(0, 0, { type: 'header', header: 'Most Recent' });
                completions.splice(4, 0, { type: 'header', header: 'Recently Used' });
                resolve(completions);
              }, 50);
            });
          }}
        />
      </Card>
    </Modal>
  );
};

export default CommandInput;
