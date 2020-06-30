import { NewAutocomplete, TabSuggestion } from 'components/autocomplete/new-autocomplete';
import { TabStop } from 'components/autocomplete/utils';
import PixieCommandIcon from 'components/icons/pixie-command';
import { ScriptsContext } from 'containers/App/scripts-context';
import * as React from 'react';
import { argsForVis } from 'utils/args-utils';
import gql from 'graphql-tag';
import { useApolloClient } from '@apollo/react-hooks';

import { createStyles, makeStyles } from '@material-ui/core';
import Card from '@material-ui/core/Card';
import Modal from '@material-ui/core/Modal';

import { ExecuteContext } from 'context/execute-context';
import { ParseFormatStringToTabStops } from './autocomplete-parser';
import { entityTypeToString } from './autocomplete-utils';
import { LiveViewPage } from '../../components/live-widgets/utils/live-view-params';
import { parseVis } from '../live/vis';

const AUTOCOMPLETE_QUERY = gql`
query autocomplete($input: String, $cursor: Int, $action: AutocompleteActionType) {
  autocomplete(input: $input, cursorPos: $cursor, action: $action) {
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
      }
    }
  }
}
`;

interface NewCommandInputProps {
  open: boolean;
  onClose: () => void;
}

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

const NewCommandInput: React.FC<NewCommandInputProps> = ({ open, onClose }) => {
  const classes = useStyles();
  const [tabStops, setTabStops] = React.useState<Array<TabStop>>([]);
  const [tabSuggestions, setTabSuggestions] = React.useState<Array<TabSuggestion>>([]);
  const [isValid, setIsValid] = React.useState(false);

  const { execute } = React.useContext(ExecuteContext);
  const { scripts } = React.useContext(ScriptsContext);

  const client = useApolloClient();
  const onChange = React.useCallback((input, cursor, action, updatedTabStops) => {
    if (updatedTabStops !== null) {
      setTabStops(updatedTabStops);
      setIsValid(false);
      return;
    }

    return client.query({
      query: AUTOCOMPLETE_QUERY,
      fetchPolicy: 'network-only',
      variables: {
        input,
        cursor,
        action: action === 'SELECT' ? 'AAT_SELECT' : 'AAT_EDIT',
      },
    }).then(({ data }) => {
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
        }));

        return {
          index: s.tabIndex,
          executableAfterSelect: s.executableAfterSelect,
          suggestions,
        };
      });
      setTabSuggestions(completions);
    });
  }, [client]);

  const onSubmit = React.useCallback(() => {
    if (isValid) {
      const script = scripts.get(tabStops[0].Value);
      const vis = parseVis(script.vis);
      if (script) {
        const args = {};
        tabStops.forEach((ts, idx) => {
          if (idx !== 0) { // Skip the "script" argument.
            args[ts.Label] = ts.Value;
          }
        });

        execute({
          liveViewPage: LiveViewPage.Default,
          entityParamNames: [],
          pxl: script.code,
          vis,
          id: script.id,
          args: argsForVis(vis, args),
        });
        onClose();
      }
    }
  }, [tabStops, isValid]);

  return (
    <Modal open={open} onClose={onClose} BackdropProps={{}}>
      <Card className={classes.card}>
        <NewAutocomplete
          className={classes.input}
          onSubmit={onSubmit}
          onChange={onChange}
          completions={tabSuggestions}
          tabStops={tabStops}
          placeholder='Pixie Command'
          prefix={<PixieCommandIcon />}
        />
      </Card>
    </Modal>
  );
};

export default NewCommandInput;
