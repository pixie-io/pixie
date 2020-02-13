import * as React from 'react';

import {createStyles, makeStyles, Theme} from '@material-ui/core/styles';

import Completions, {CompletionId, CompletionItem, CompletionTitle} from './completions';
import Input from './input';

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

const Autocomplete: React.FC<AutoCompleteProps> = ({
  onSelection,
  getCompletions,
}) => {
  const classes = useStyles();
  const [inputValue, setInputValue] = React.useState('');
  const [completions, setCompletions] = React.useState([]);
  const [activeItem, setActiveItem] = React.useState<CompletionId>('');
  const itemsMap = React.useMemo(() => {
    const map = new Map<CompletionId, CompletionTitle>();
    for (const item of completions) {
      if (!item.header) {
        map.set(item.id, item.title);
      }
    }
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
    setInputValue(itemsMap.get(id));
  }, [itemsMap]);

  return (
    <div className={classes.root}>
      <Input
        onChange={setInputValue}
        value={inputValue}
        suggestion={itemsMap.get(activeItem) || ''}
      />
      <Completions
        items={completions}
        inputValue={inputValue}
        onActiveChange={setActiveItem}
        onSelection={handleSelection}
        activeItem={activeItem}
      />
    </div>
  );
};

export default Autocomplete;
