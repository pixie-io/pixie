import clsx from 'clsx';
import * as React from 'react';

import {createStyles, makeStyles, Theme} from '@material-ui/core/styles';

import {Key} from './key';

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
  onKey: (key: Key) => void;
  suggestion?: string;
  className?: string;
  value: string;
}

const Input: React.FC<InputProps> = ({
  onChange,
  onKey,
  suggestion,
  className,
  value,
}) => {
  const classes = useStyles();
  const [focused, setFocused] = React.useState<boolean>(true);
  const inputRef = React.useRef<HTMLInputElement>(null);

  const handleChange = React.useCallback((e) => {
    const val = e.target.value;
    onChange(val);
  }, []);

  const handleKey = React.useCallback((e) => {
    switch (e.key) {
      case 'Tab':
        e.preventDefault();
        onKey('TAB');
        break;
      case 'Enter':
        onKey('ENTER');
        break;
      default:
      // noop
    }
  }, [onKey]);

  const handleFocus = React.useCallback(() => {
    setFocused(true);
  }, []);

  const handleBlur = React.useCallback(() => {
    setFocused(false);
  }, []);

  const focusInput = React.useCallback(() => {
    inputRef.current.focus();
  }, []);

  // Focus the input element whenever the suggestion changes.
  React.useEffect(() => {
    inputRef.current.focus();
  }, [suggestion]);

  const hint = value && suggestion && suggestion.startsWith(value) ?
    suggestion.slice(value.length) : '';

  return (
    <div className={clsx(classes.root, className)} onClick={focusInput}>
      <input
        className={classes.inputElem}
        ref={inputRef}
        value={value}
        onChange={handleChange}
        onFocus={handleFocus}
        onBlur={handleBlur}
        onKeyDown={handleKey}
      />
      <div>
        <span>{value}</span>
        <Caret active={focused} />
        <span className={classes.suggestion}>{hint}</span>
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
