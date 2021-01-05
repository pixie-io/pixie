import { buildClass } from 'utils/build-class';
import * as React from 'react';

import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';

import { AutocompleteContext } from 'components/autocomplete/autocomplete-context';
import { Key } from './key';

// TODO(malthus): Make use of the theme styles.
const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    ...theme.typography.h6,
    position: 'relative',
    cursor: 'text',
    padding: theme.spacing(2.5),
    fontWeight: theme.typography.fontWeightLight,
    display: 'flex',
    flexDirection: 'row',
    alignItems: 'baseline',

    '> *': {
      flex: '1',
    },
  },
  inputWrapper: {
    position: 'relative',
    minWidth: '0.1px',
  },
  inputElem: {
    // Match the root surrounding it
    color: 'inherit',
    background: 'inherit',
    border: 'none',
    fontSize: 'inherit',
    fontWeight: 'inherit',
    lineHeight: 'inherit',
    position: 'absolute',
    top: '0',
    left: '0',
    padding: '0',
    // <input /> is a replaced element, and as such is frustratingly disobedient with CSS. Its width calculation is
    // off slightly; oversizing it allows it to fit its whole contents when growing to, well, fit its contents.
    // The hint still lines up because its position is based on the adjacent dummy, which sizes correctly on its own.
    // https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input#CSS
    // https://developer.mozilla.org/en-US/docs/Web/CSS/Replaced_element
    width: '120%',
  },
  // Since HTMLInputElement does not obey normal width calculations, we position it atop an invisible span that does.
  dummy: {
    color: 'transparent',
    pointerEvents: 'none',
    paddingRight: '0.01px', // Guarantees that inputWrapper will have a nonzero width, so the input knows where to be.
  },
  hint: {
    opacity: 0.2,
  },
  textArea: {
    flex: 1,
  },
  prefix: {
    paddingRight: theme.spacing(2),
  },
}));

interface InputProps {
  onChange: (val: string) => void;
  onKey: (key: Key) => void;
  suggestion?: string;
  placeholder?: string;
  prefix?: React.ReactNode;
  className?: string;
  value: string;
  customRef?: React.MutableRefObject<HTMLInputElement>;
}

export const Input: React.FC<InputProps> = ({
  onChange,
  onKey,
  suggestion,
  className,
  placeholder = '',
  prefix = null,
  value,
  customRef,
}) => {
  const classes = useStyles();
  const { openMode } = React.useContext(AutocompleteContext);
  const dummyElement = React.useRef<HTMLSpanElement>(null);
  const defaultRef = React.useRef<HTMLInputElement>(null);
  const inputRef = customRef || defaultRef;

  React.useEffect(() => {
    if (openMode !== 'none' && inputRef.current) {
      // Need to wait for the value to propagate, so let the render complete before manipulating the selection directly.
      setTimeout(() => {
        if (openMode === 'select') inputRef.current.setSelectionRange(0, inputRef.current.value.length);
        if (openMode === 'clear') inputRef.current.value = '';
        inputRef.current.focus();
      }, 0);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [openMode, inputRef]);

  const handleChange = React.useCallback(
    (e) => {
      const val = e.target.value;
      onChange(val);
    },
    [onChange],
  );

  const handleKey = React.useCallback(
    (e) => {
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
    },
    [onKey],
  );

  const focusInput = React.useCallback(() => {
    inputRef.current?.focus();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Focus the input element whenever the suggestion changes.
  React.useEffect(() => {
    inputRef.current?.focus();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [suggestion]);

  const hint = React.useMemo(() => {
    if (!value) {
      return placeholder;
    }
    return suggestion && suggestion.startsWith(value)
      ? suggestion.slice(value.length)
      : '';
  }, [value, suggestion, placeholder]);

  return (
    <div className={buildClass(classes.root, className)} onClick={focusInput}>
      <span className={classes.inputWrapper}>
        <span className={classes.dummy} ref={dummyElement}>
          {value}
        </span>
        <input
          type='text'
          className={classes.inputElem}
          ref={inputRef}
          value={value}
          onChange={handleChange}
          onKeyDown={handleKey}
          size={0} // Ensures that CSS controls the width of this element
        />
      </span>
      {prefix ? <div className={classes.prefix}>{prefix}</div> : null}
      <span className={classes.hint} tabIndex={-1}>
        {hint}
      </span>
    </div>
  );
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

  return <textarea className={classes.textArea} ref={ref} value={value} />;
};
