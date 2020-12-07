import {
  CommandAutocomplete, TabSuggestion,
  TabStop, PixieCommandIcon,
} from 'pixie-components';
import { ScriptsContext } from 'containers/App/scripts-context';
import * as React from 'react';
import gql from 'graphql-tag';
import { useApolloClient } from '@apollo/react-hooks';
import ClusterContext from 'common/cluster-context';

import { createStyles, makeStyles, Theme } from '@material-ui/core';
import Card from '@material-ui/core/Card';
import Modal from '@material-ui/core/Modal';

import { ScriptContext, ExecuteArguments } from 'context/script-context';
import { ContainsMutation } from 'utils/pxl';
import { entityPageForScriptId } from 'containers/live-widgets/utils/live-view-params';
import { ParseFormatStringToTabStops } from './autocomplete-parser';
import { entityTypeToString } from './autocomplete-utils';

const AUTOCOMPLETE_QUERY = gql`
query autocomplete($input: String, $cursor: Int, $action: AutocompleteActionType, $clusterUID: String) {
  autocomplete(input: $input, cursorPos: $cursor, action: $action, clusterUID: $clusterUID) {
    formattedInput
    isExecutable
    tabSuggestions {
      tabIndex
      executableAfterSelect
      suggestions {
        kind
        name
        description
        matchedIndexes
        state
      }
    }
  }
}
`;

interface NewCommandInputProps {
  open: boolean;
  onClose: () => void;
}

interface CurrentInput {
  text: string;
}

const useHintStyles = makeStyles((theme: Theme) => (createStyles({
  hotkeyHint: {
    opacity: 0.6,
    fontSize: '75%',
    flex: '0 0 auto',
    position: 'relative',
    bottom: theme.spacing(0.25), // Vertically aligns the shift/enter symbols with lowercase letters
    pointerEvents: 'none',
    userSelect: 'none',
    paddingLeft: theme.spacing(1.25),

    '& code': {
      border: '1px rgba(255, 255, 255, 0.5) solid',
      borderRadius: theme.spacing(0.25),
      padding: `${theme.spacing(0.625)}px ${theme.spacing(1)}px`,
    },

    '& span': {
      margin: '0 0.5em',
    },
  },
})));

const PixieCommandSubmitHint: React.FC = () => {
  const classes = useHintStyles();
  return (
    <span className={classes.hotkeyHint} tabIndex={-1} aria-label='Press Shift+Enter to run this command'>
      <code>{'\u21E7' /* Shift as shown on many older keyboards */}</code>
      <span>+</span>
      <code>{'\u23CE' /* Enter as shown on many older keyboards */}</code>
    </span>
  );
};

const useStyles = makeStyles(() => (createStyles({
  card: {
    position: 'absolute',
    width: '760px',
    top: '40%',
    left: '50%',
    transform: 'translate(-50%, -20vh)',
  },
  input: {
    maxHeight: '60vh',
  },
})));

const CommandInput: React.FC<NewCommandInputProps> = ({ open, onClose }) => {
  const classes = useStyles();
  const [tabStops, setTabStops] = React.useState<Array<TabStop>>([]);
  const [tabSuggestions, setTabSuggestions] = React.useState<Array<TabSuggestion>>([]);
  const [isValid, setIsValid] = React.useState(false);
  const { selectedClusterUID } = React.useContext(ClusterContext);

  const {
    execute, setScript, parseVisOrShowError, argsForVisOrShowError,
  } = React.useContext(ScriptContext);
  const { scripts } = React.useContext(ScriptsContext);
  const [currentInput] = React.useState({} as CurrentInput);

  const client = useApolloClient();
  const onChange = React.useCallback((input, cursor, action, updatedTabStops) => {
    if (updatedTabStops !== null) {
      setTabStops(updatedTabStops);
      setIsValid(false);
    }
    currentInput.text = input;

    return client.query({
      query: AUTOCOMPLETE_QUERY,
      fetchPolicy: 'network-only',
      variables: {
        input,
        cursor,
        action: action === 'SELECT' ? 'AAT_SELECT' : 'AAT_EDIT',
        clusterUID: selectedClusterUID,
      },
    }).then(({ data }) => {
      if (input === currentInput.text) {
        setIsValid(data.autocomplete.isExecutable);
        setTabStops(ParseFormatStringToTabStops(data.autocomplete.formattedInput));
        const completions = data.autocomplete.tabSuggestions.map((s) => {
          const suggestions = s.suggestions.map((sugg, i) => ({
            type: 'item',
            id: sugg.name + i,
            title: sugg.name,
            itemType: entityTypeToString(sugg.kind),
            description: sugg.description,
            highlights: sugg.matchedIndexes,
            state: sugg.state,
          }));

          return {
            index: s.tabIndex,
            executableAfterSelect: s.executableAfterSelect,
            suggestions,
          };
        });
        setTabSuggestions(completions);
      }
    });
  }, [client, selectedClusterUID, currentInput]);

  // Make an API call to get a list of initial suggestions when the command input is first loaded.
  React.useEffect(() => {
    onChange('', 0, 'EDIT', null);
    // We only want this useEffect to be called the first time the command input is loaded.
    // eslint-disable-next-line
  }, []);

  const onSubmit = React.useCallback(() => {
    if (isValid) {
      const script = scripts.get(tabStops[0].Value);
      const vis = parseVisOrShowError(script.vis);
      const args = {};
      tabStops.forEach((ts, idx) => {
        if (idx !== 0) { // Skip the "script" argument.
          args[ts.Label] = ts.Value;
        }
      });
      const parsedArgs = argsForVisOrShowError(vis, args);

      if (script && vis && parsedArgs) {
        const execArgs: ExecuteArguments = {
          liveViewPage: entityPageForScriptId(script.id),
          pxl: script.code,
          vis,
          id: script.id,
          args: parsedArgs,
        };
        setTabStops([{ CursorPosition: 0, Index: 1, Value: '' }]);
        setIsValid(false);
        setScript(execArgs.vis, execArgs.pxl, execArgs.args, execArgs.id, execArgs.liveViewPage);
        if (!ContainsMutation(execArgs.pxl)) {
          execute(execArgs);
        }
        onClose();
      }
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [tabStops, isValid, execute, onClose, scripts]);

  return (
    <Modal open={open} onClose={onClose} BackdropProps={{}}>
      <Card className={classes.card}>
        <CommandAutocomplete
          className={classes.input}
          onSubmit={onSubmit}
          onChange={onChange}
          completions={tabSuggestions}
          tabStops={tabStops}
          placeholder='Type a script or entity...'
          isValid={isValid}
          prefix={<PixieCommandIcon />}
          suffix={<PixieCommandSubmitHint />}
        />
      </Card>
    </Modal>
  );
};

export default CommandInput;
