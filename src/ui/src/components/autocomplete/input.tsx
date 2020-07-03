import clsx from 'clsx';
import * as React from 'react';

import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';

import { Key } from './key';

const useStyles = makeStyles((theme: Theme) => (
  // TODO(malthus): Make use of the theme styles.
  createStyles({
    root: {
      ...theme.typography.h6,
      position: 'relative',
      cursor: 'text',
      padding: theme.spacing(2.5),
      fontWeight: theme.typography.fontWeightLight,
      display: 'flex',
      flexDirection: 'row',
    },
    inputElem: {
      position: 'absolute',
      opacity: 0,
    },
    hint: {
      opacity: 0.2,
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
    inputValue: {
      flex: 1,
    },
    prefix: {
      paddingRight: theme.spacing(2),
    },
  })));

interface InputProps {
  onChange: (val: string) => void;
  onKey: (key: Key) => void;
  suggestion?: string;
  placeholder?: string;
  prefix?: React.ReactNode;
  className?: string;
  value: string;
}

const Input: React.FC<InputProps> = ({
  onChange,
  onKey,
  suggestion,
  className,
  placeholder = '',
  prefix = null,
  value,
}) => {
  const classes = useStyles();
  const [focused, setFocused] = React.useState<boolean>(true);
  const inputRef = React.useRef<HTMLInputElement>(null);

  const handleChange = React.useCallback((e) => {
    const val = e.target.value;
    onChange(val);
  }, [onChange]);

  const handleKey = React.useCallback((e) => {
    switch (e.key) {
      case 'Tab':
        e.preventDefault();
        onKey('TAB');
        break;
      case 'Enter':
        onKey('ENTER');
        break;
      case 'ArrowUp':
        onKey('UP');
        break;
      case 'ArrowDown':
        onKey('DOWN');
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

  const hint = React.useMemo(() => {
    if (!value) {
      return placeholder;
    }
    return suggestion && suggestion.startsWith(value)
      ? suggestion.slice(value.length) : '';
  }, [value, suggestion, placeholder]);

  return (
    <div className={clsx(classes.root, className)} onClick={focusInput}>
      {prefix ? <div className={classes.prefix}>{prefix}</div> : null}
      <input
        className={classes.inputElem}
        ref={inputRef}
        value={value}
        onChange={handleChange}
        onFocus={handleFocus}
        onBlur={handleBlur}
        onKeyDown={handleKey}
      />
      <div className={classes.inputValue}>
        <span>{value}</span>
        <Caret active={focused} />
        <span className={classes.hint} tabIndex={-1}>{hint}</span>
      </div>
    </div>
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

    // eslint-disable-next-line consistent-return
    return () => {
      clearInterval(intervalSub);
    };
  }, [active]);
  return (
    <div className={clsx(classes.caret, active && visible && 'visible')}>&nbsp;</div>);
};

type FormField = [string, string];

interface InputFormProps {
  onInputChanged: (value: string) => void;
  form: FormField[];
}

export const FormInput: React.FC<InputFormProps> = ({ form }) => {
  const classes = useStyles();
  const ref = React.useRef<HTMLTextAreaElement>(null);
  const [value, setValue] = React.useState<string>('');

  React.useEffect(() => {
    const combined = form.map(([key, val]) => `${key}:${val}`).join(' ');
    setValue(combined);
  }, [form]);

  return (
    <textarea className={classes.inputValue} ref={ref} value={value} />
  );
};

export default Input;
