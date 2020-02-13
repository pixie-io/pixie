import clsx from 'clsx';
import * as React from 'react';

import {createStyles, makeStyles, Theme} from '@material-ui/core/styles';

interface CompletionsProps {
  inputValue: string;
  items: Array<CompletionItem | CompletionHeader>;
  activeItem?: string;
  onActiveChange: (id: string) => void;
  onSelection: (id: string) => void;
}

interface CompletionHeader {
  header: string;
}

interface CompletionItem {
  id: string;
  title: string;
  description?: string;
  highlights?: Array<[number, number]>;
}

const useStyles = makeStyles((theme: Theme) => {
  // TODO(malthus): Make use of the theme styles.
  return createStyles({
    root: {},
    header: theme.typography.subtitle1,
    completion: {
      cursor: 'pointer',
      '&.active': {
        backgroundColor: 'white',
      },
    },
    highlight: {
      fontWeight: 600,
    },
  });
});

const Completions: React.FC<CompletionsProps> = (props) => {
  const { items, activeItem, onActiveChange, onSelection } = props;
  const classes = useStyles();
  return (
    <div className={classes.root}>
      {
        items.map((item) => {
          const h = item as CompletionHeader;
          if (h.header) {
            return <div className={classes.header}>{h.header}</div>;
          }
          item = item as CompletionItem;
          return (
            <Completion
              active={item.id === activeItem}
              onSelection={onSelection}
              onActiveChange={onActiveChange}
              {...item}
            />
          );
        })
      }
    </div>
  );
};

type CompletionProps = CompletionItem & {
  active: boolean;
  onSelection: (id: string) => void;
  onActiveChange: (id: string) => void;
};

export const Completion = (props: CompletionProps) => {
  const {
    id,
    title,
    highlights = [],
    onSelection,
    onActiveChange,
    active,
  } = props;
  const classes = useStyles();
  const parts = [];
  if (!title) {
    return null;
  }
  let remainingIdx = 0;
  for (const [start, end] of highlights) {
    const prev = title.substring(remainingIdx, start);
    if (prev) {
      parts.push(<span key={`title-${remainingIdx}`}>{prev}</span>);
    }
    const highlight = title.substring(start, end);
    if (highlight) {
      parts.push(<span key={`title-${start}`} className={classes.highlight}>{highlight}</span>);
    }
    remainingIdx = end;
  }
  if (remainingIdx < title.length) {
    parts.push(
      <span key={`title-${remainingIdx}`}>
        {title.substring(remainingIdx)}
      </span>,
    );
  }
  return (
    <div
      className={clsx(classes.completion, active && 'active')}
      onClick={() => onSelection(id)}
      onMouseOver={() => onActiveChange(id)}
    >
      {parts}
    </div>
  );
};

export default Completions;
