import AutocompleteInputField from 'components/autocomplete/autocomplete-field';
import gql from 'graphql-tag';
import * as React from 'react';

import { useApolloClient } from '@apollo/react-hooks';
import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';
import Tooltip from '@material-ui/core/Tooltip';

import { getArgTypesForVis } from 'utils/args-utils';
import { ExecuteContext } from './context/execute-context';
import { ScriptContext } from './context/script-context';
import { EntityType, pxTypetoEntityType } from './utils/autocomplete-utils';

const ArgsEditor = () => {
  const {
    vis, pxl, args, id, liveViewPage, setArgs,
  } = React.useContext(ScriptContext);
  const { execute } = React.useContext(ExecuteContext);

  if (!args) {
    return null;
  }

  const argTypes = getArgTypesForVis(vis);

  const argsList = Object.entries(args).filter(([argName]) => argName !== 'script');
  return (
    <>
      {
        argsList.map(([argName, argVal]) => {
          const entityType = pxTypetoEntityType(argTypes[argName]);
          const argProps = {
            name: argName,
            value: argVal,
            onValueChange: (newVal) => {
              setArgs({ ...args, [argName]: newVal });
            },
          };
          if (entityType !== 'AEK_UNKNOWN') {
            // If the argument corresponds to a known entity type, we can autocomplete the field.
            return (
              <AutocompleteArgumentField
                key={argName}
                kind={entityType}
                {...argProps}
              />
            );
          }
          return (
            <ArgumentField
              key={argName}
              onEnterKey={() => {
                execute({
                  pxl, vis, args, id, liveViewPage,
                });
              }}
              {...argProps}
            />
          );
        })
      }
    </>
  );
};

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    marginLeft: theme.spacing(1),
    display: 'flex',
    flexDirection: 'row',
  },
  input: {
    backgroundColor: 'transparent',
    border: 'none',
    padding: 0,
    color: theme.palette.foreground.two,
    borderBottom: `1px solid ${theme.palette.foreground.one}`,
    marginLeft: theme.spacing(0.5),
    position: 'absolute',
    left: 0,
    top: 0,
    width: '100%',
    ...theme.typography.subtitle2,
    fontWeight: theme.typography.fontWeightLight,
    '&:focus': {
      borderBottom: `1px solid ${theme.palette.primary.main}`,
    },
  },
  measurer: {
    position: 'relative',
  },
  measurerContent: {
    opacity: 0,
    minWidth: theme.spacing(2),
    paddingRight: theme.spacing(1),
  },
}));

interface ArgumentInputFieldProps {
  name: string;
  value: string;
  onValueChange: (val: string) => void;
  onEnterKey?: () => void;
}

const ENTER_KEY_CODE = 13;

const ArgumentField = (props: ArgumentInputFieldProps) => {
  const classes = useStyles();
  const {
    name, value, onValueChange, onEnterKey,
  } = props;
  const ref = React.useRef(null);
  return (
    <Tooltip title='Edit arg'>
      <div className={classes.root}>
        <span onClick={() => { ref.current.focus(); }}>{name}</span>
        :
        <div className={classes.measurer}>
          <div className={classes.measurerContent}>{value}</div>
          <input
            ref={ref}
            className={classes.input}
            value={value}
            onChange={(event) => onValueChange(event.target.value)}
            onKeyPress={(event) => {
              if (event.which === ENTER_KEY_CODE) {
                onEnterKey();
              }
            }}
          />
        </div>
      </div>
    </Tooltip>
  );
};

const AUTOCOMPLETE_FIELD_QUERY = gql`
query getCompletions($input: String, $kind: AutocompleteEntityKind) {
  autocompleteField(input: $input, fieldType: $kind) {
    name
    description
    matchedIndexes
  }
}
`;

interface AutocompleteArgumentFieldProps extends ArgumentInputFieldProps {
  kind: EntityType;
}

const AutocompleteArgumentField = (props: AutocompleteArgumentFieldProps) => {
  const { name, value, onValueChange } = props;
  const client = useApolloClient();
  const getCompletions = React.useCallback((newInput: string) => (client.query({
    query: AUTOCOMPLETE_FIELD_QUERY,
    fetchPolicy: 'network-only',
    variables: {
      input: newInput,
      kind: props.kind,
    },
  })
    .then(({ data }) => {
      const completions = data.autocompleteField.map((suggestion) => ({
        type: 'item',
        id: suggestion.name,
        title: suggestion.name,
        description: suggestion.description,
        highlights: suggestion.matchedIndexes,
      }));
      completions.unshift({ type: 'header', header: props.kind });
      return completions;
    })
  ), [client]);

  return (
    <Tooltip title='Edit arg'>
      <div>
        <AutocompleteInputField
          name={name}
          value={value}
          onValueChange={onValueChange}
          getCompletions={getCompletions}
        />
      </div>
    </Tooltip>
  );
};

export default ArgsEditor;
