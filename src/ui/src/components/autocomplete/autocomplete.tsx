import clsx from 'clsx';
import { scrollbarStyles } from 'common/mui-theme';
import * as React from 'react';

import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';

import Completions, {
  CompletionId, CompletionItem, CompletionItems, CompletionTitle,
} from './completions';
import Input from './input';
import { Key } from './key';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    backgroundColor: theme.palette.background.three,
    cursor: 'text',
    display: 'flex',
    flexDirection: 'column',
    // TODO(malthus): remove this once the scrollbar theme is set at global level.
    ...scrollbarStyles(theme),
  },
  input: {
    backgroundColor: theme.palette.background.two,
  },
  completions: {
    flex: 1,
    minHeight: 0,
  },
}));

interface AutoCompleteProps {
  onSelection: (id: CompletionId) => void;
  getCompletions: (input: string) => Promise<CompletionItems>;
  placeholder?: string;
  prefix?: React.ReactNode;
  className?: string;
}

type ItemsMap = Map<CompletionId, { title: CompletionTitle; index: number }>;

function findNextItem(activeItem: CompletionId, itemsMap: ItemsMap, completions: CompletionItems, direction = 1):
CompletionId {
  if (!activeItem || completions.length === 0) {
    return '';
  }
  const { index } = itemsMap.get(activeItem);
  const { length } = completions;
  for (let i = 1; i < length; i++) {
    const nextIndex = (index + (i * direction) + length) % length;
    const next = completions[nextIndex] as CompletionItem;
    if (next.title && next.id) {
      return next.id;
    }
  }
  return activeItem;
}

const Autocomplete: React.FC<AutoCompleteProps> = ({
  onSelection,
  getCompletions,
  placeholder,
  prefix,
  className,
}) => {
  const classes = useStyles();
  const [inputValue, setInputValue] = React.useState('');
  const [completions, setCompletions] = React.useState([]);
  const [activeItem, setActiveItem] = React.useState<CompletionId>('');
  const itemsMap = React.useMemo(() => {
    const map: ItemsMap = new Map();
    completions.forEach((item, index) => {
      if (!item.header) {
        map.set(item.id, { title: item.title, index });
      }
    });
    return map;
  }, [completions]);

  React.useEffect(() => {
    getCompletions(inputValue).then((cmpls) => {
      setCompletions(cmpls);
      for (const completion of cmpls) {
        const cmpl = completion as CompletionItem;
        if (cmpl.title && cmpl.id) {
          setActiveItem(cmpl.id);
          return;
        }
      }
    });
  }, [inputValue, getCompletions]);

  const handleSelection = React.useCallback((id) => {
    const item = itemsMap.get(id);
    if (!item) {
      return;
    }
    onSelection(id);
    setInputValue(item.title);
  }, [itemsMap, onSelection]);

  const handleKey = (key: Key) => {
    switch (key) {
      case 'UP':
        setActiveItem(findNextItem(activeItem, itemsMap, completions, -1));
        break;
      case 'DOWN':
        setActiveItem(findNextItem(activeItem, itemsMap, completions));
        break;
      case 'ENTER':
        handleSelection(activeItem);
        break;
      default:
        break;
    }
  };

  return (
    <div className={clsx(classes.root, className)}>
      <Input
        className={classes.input}
        onChange={setInputValue}
        onKey={handleKey}
        value={inputValue}
        placeholder={placeholder}
        prefix={prefix}
        suggestion={itemsMap.get(activeItem)?.title || ''}
      />
      <Completions
        className={classes.completions}
        items={completions}
        onActiveChange={setActiveItem}
        onSelection={handleSelection}
        activeItem={activeItem}
      />
    </div>
  );
};

export default Autocomplete;
