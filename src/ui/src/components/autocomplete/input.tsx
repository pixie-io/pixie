import clsx from 'clsx';
import * as React from 'react';

import {createStyles, makeStyles, Theme} from '@material-ui/core/styles';

const useStyles = makeStyles((theme: Theme) => {
  // TODO(malthus): Make use of the theme styles.
  return createStyles({
    root: {
      position: 'relative',
      cursor: 'text',
      padding: theme.spacing(2),
    },
    inputElem: {
      position: 'absolute',
      opacity: 0,
    },
    suggestion: {
      opacity: 0.7,
    },
    caret: {
      display: 'inline-block',
      width: 0,
      height: '100%',
      borderRight: '1px solid white',
      visibility: 'hidden',
      '&.visible': {
        visibility: 'visible',
      },
    },
  });
});

interface InputProps {
  onChange: (val: string) => void;
  suggestion?: string;
  className?: string;
}

const Input: React.FC<InputProps> = (props) => {
  const classes = useStyles();
  const [input, setInput] = React.useState<string>('');
  const [focused, setFocused] = React.useState<boolean>(true);
  const inputRef = React.useRef<HTMLInputElement>(null);

  const handleChange = React.useCallback((e) => {
    const val = e.target.value;
    setInput(val);
    props.onChange(val);
  }, []);

  const handleTab = React.useCallback((e) => {
    if (e.key !== 'Tab' || !props.suggestion || !input) {
      return;
    }
    if (props.suggestion !== input && props.suggestion.startsWith(input)) {
      e.preventDefault();
      setInput(props.suggestion);
    }
  }, [input, props.suggestion]);

  const handleFocus = React.useCallback(() => {
    setFocused(true);
  }, []);

  const handleBlur = React.useCallback(() => {
    setFocused(false);
  }, []);

  const focusInput = React.useCallback(() => {
    inputRef.current.focus();
  }, []);

  // Focus the input element on first render only.
  React.useEffect(() => {
    inputRef.current.focus();
  }, []);

  const suggestion = input && props.suggestion && props.suggestion.startsWith(input) ?
    props.suggestion.slice(input.length) : '';

  return (
    <div className={clsx(classes.root, props.className)} onClick={focusInput}>
      <input
        className={classes.inputElem}
        ref={inputRef}
        value={input}
        onChange={handleChange}
        onFocus={handleFocus}
        onBlur={handleBlur}
        onKeyDown={handleTab}
      />
      <div>
        <span>{input}</span>
        <Caret active={focused} />
        <span className={classes.suggestion}>{suggestion}</span>
      </div>
    </div >
  );
};

const BLINK_INTERVAL = 500; // 1000ms = 1s

const Caret: React.FC<{ active: boolean }> = ({ active }) => {
  const classes = useStyles();
  const [visible, setVisible] = React.useState(true);
  React.useEffect(() => {
    if (!active) { return; }

    const intervalSub = setInterval(() => {
      setVisible((show) => !show);
    }, BLINK_INTERVAL);

    return () => {
      clearInterval(intervalSub);
    };
  }, [active]);
  return (
    <div className={clsx(classes.caret, active && visible && 'visible')}>&nbsp;</div>);
};

export default Input;
