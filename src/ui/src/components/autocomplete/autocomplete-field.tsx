import clsx from 'clsx';
import { CompletionItem } from 'components/autocomplete/completions';
import useAutocomplete, { GetCompletionsFunc } from 'components/autocomplete/use-autocomplete';
import { Spinner } from 'components/spinner/spinner';
import * as React from 'react';
import { isInView } from 'utils/bbox';

import Card from '@material-ui/core/Card';
import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    marginLeft: theme.spacing(1),
    display: 'flex',
    flexDirection: 'row',
    height: theme.spacing(3),
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
      outline: 'none',
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
  completions: {
    position: 'absolute',
    zIndex: theme.zIndex.modal,
    top: theme.spacing(3), // height of the input
    left: theme.spacing(0.5),
    width: 'max(100%, 120px)',
    minWidth: '120px',
    maxHeight: '200px',
    overflow: 'auto',
    display: 'none',
    '&.visible': {
      display: 'unset',
    },
  },
  completion: {
    cursor: 'pointer',
    padding: theme.spacing(0.5),
  },
  activeCompletion: {
    color: 'white',
    backgroundColor: theme.palette.action.active,
  },
  spinner: {
    height: '100px',
    display: 'flex',
    flexDirection: 'row',
    overflow: 'hidden',
    '& > *': {
      margin: 'auto',
    },
  },
  highlight: {
    fontWeight: 600,
  },
}));

interface AutocompleteArgumentFieldProps {
  name: string;
  value: string;
  onEnterKey: () => void;
  onValueChange: (newVal: string) => void;
  getCompletions: GetCompletionsFunc;
}

const AutocompleteInputField = (props: AutocompleteArgumentFieldProps) => {
  const {
    name, value, onEnterKey, onValueChange, getCompletions,
  } = props;
  const classes = useStyles();
  const ref = React.useRef<HTMLInputElement>(null);
  const [opened, setOpened] = React.useState<boolean>(false);
  const {
    loading,
    completions,
    input,
    activeCompletion,
    highlightPrev,
    highlightNext,
    highlightById,
  } = useAutocomplete(getCompletions, value);

  const onInputChange = (newInput: string) => {
    onValueChange(newInput);
    setOpened(true);
  };

  const keyHandler = (event: React.KeyboardEvent) => {
    switch (event.key) {
      case 'Enter':
        if (activeCompletion == null) {
          onEnterKey();
        } else {
          onValueChange(activeCompletion?.title);
        }
        setOpened(false);
        break;
      case 'ArrowUp':
        highlightPrev();
        break;
      case 'ArrowDown':
        highlightNext();
        break;
      case 'Escape':
        setOpened(false);
        break;
      default:
      // noop
    }
  };

  const onCompletionClicked = (event: React.MouseEvent, completion: CompletionItem) => {
    // When the completion is clicked, preventDefault to keep the input focused.
    event.preventDefault();
    onValueChange(completion.title);
    setOpened(false);
  };

  return (
    <div className={classes.root}>
      <span onClick={() => { ref.current.focus(); }}>{name}</span>
      :
      <div className={classes.measurer}>
        <div className={classes.measurerContent}>{value}</div>
        <input
          ref={ref}
          className={classes.input}
          value={input}
          onChange={(event) => onInputChange(event.target.value)}
          onBlur={() => setOpened(false)}
          onFocus={() => setOpened(true)}
          onKeyDown={keyHandler}
        />
        <Card className={clsx(classes.completions, opened && 'visible')}>
          {
            loading
              ? <div className={classes.spinner}><Spinner /></div>
              : completions.map((completion) => {
                if (completion.type === 'header') {
                  return null;
                }
                return (
                  <FieldCompletion
                    key={completion.id}
                    active={activeCompletion?.id === completion.id}
                    onHover={() => highlightById(completion.id)}
                    onClick={(event) => onCompletionClicked(event, completion)}
                    value={completion.title}
                    highlightedIndexes={completion.highlights}
                  />
                );
              })
          }
        </Card>
      </div>
    </div>
  );
};

interface FieldCompletionProps {
  active: boolean;
  onClick: (event: React.MouseEvent) => void;
  onHover: () => void;
  value: string;
  highlightedIndexes: Array<number>;
}

const FieldCompletion = (props: FieldCompletionProps) => {
  const ref = React.useRef(null);
  const {
    active, onClick, onHover, value, highlightedIndexes,
  } = props;
  const classes = useStyles();

  const className = clsx(
    classes.completion,
    active && classes.activeCompletion,
  );

  React.useEffect(() => {
    if (active && !isInView(ref.current.parentElement, ref.current)) {
      ref.current.scrollIntoView();
    }
  }, [active]);

  const highlightedString = [];
  for (let i = 0; i < value.length; i++) {
    highlightedString.push(
      <span className={highlightedIndexes.includes(i) ? classes.highlight : ''}>
        {value.charAt(i)}
      </span>);
  }

  return (
    <div
      ref={ref}
      className={className}
      onMouseOver={onHover}
      // use mousedown instead of click because the blur handler occurs before the click handler.
      onMouseDown={(event) => onClick(event)}
    >
      {highlightedString}
    </div>
  );
};

export default AutocompleteInputField;
