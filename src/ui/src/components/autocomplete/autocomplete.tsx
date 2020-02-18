import * as React from 'react';

import {createStyles, makeStyles, Theme} from '@material-ui/core/styles';

import Completions, {CompletionId, CompletionItem, CompletionTitle} from './completions';
import Input from './input';
import {Key} from './key';

const useStyles = makeStyles((theme: Theme) => {
  // TODO(malthus): Make use of the theme styles.
  return createStyles({
    root: {
      position: 'relative',
      cursor: 'text',
      padding: theme.spacing(2),
    },
  });
});

interface AutoCompleteProps {
  onSelection: (id: CompletionId) => void;
  getCompletions: (input: string) => Promise<CompletionItem[]>;
}

type ItemsMap = Map<CompletionId, { title: CompletionTitle, index: number }>;

function findNextItem(activeItem: CompletionId, itemsMap: ItemsMap, completions: CompletionItem[]): CompletionId {
  const { index } = itemsMap.get(activeItem);
  for (let i = 1; i < completions.length; i++) {
    const next = (index + i) % completions.length;
    if (completions[next].title) {
      return completions[next].id;
    }
  }
  return activeItem;
}

const Autocomplete: React.FC<AutoCompleteProps> = ({
  onSelection,
  getCompletions,
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
        if (completion.title) {
          setActiveItem(completion.id);
          return;
        }
      }
    });
  }, [inputValue]);

  const handleSelection = React.useCallback((id) => {
    onSelection(id);
    setInputValue(itemsMap.get(id).title);
  }, [itemsMap, activeItem]);

  const handleKey = (key: Key) => {
    switch (key) {
      case 'TAB':
        setActiveItem(findNextItem(activeItem, itemsMap, completions));
        break;
      case 'ENTER':
        handleSelection(activeItem);
        break;
    }
  };

  return (
    <div className={classes.root} >
      <Input
        onChange={setInputValue}
        onKey={handleKey}
        value={inputValue}
        // tslint:disable-next-line:whitespace
        suggestion={itemsMap.get(activeItem)?.title || ''}
      />
      <Completions
        items={completions}
        inputValue={inputValue}
        onActiveChange={setActiveItem}
        onSelection={handleSelection}
        activeItem={activeItem}
      />
    </div >
  );
};

export default Autocomplete;
