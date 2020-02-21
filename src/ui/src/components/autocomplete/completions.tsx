import clsx from 'clsx';
import * as React from 'react';

import {createStyles, makeStyles, Theme} from '@material-ui/core/styles';

interface CompletionsProps {
  inputValue: string;
  items: CompletionItems;
  activeItem?: string;
  onActiveChange: (id: string) => void;
  onSelection: (id: string) => void;
  className?: string;
}

export type CompletionItems = Array<CompletionItem | CompletionHeader>;

interface CompletionHeader {
  header: string;
}

export type CompletionId = string;
export type CompletionTitle = string;

export interface CompletionItem {
  id?: CompletionId;
  title?: CompletionTitle;
  description?: string;
  highlights?: Array<[number, number]>;
}

const useStyles = makeStyles((theme: Theme) => {
  // TODO(malthus): Make use of the theme styles.
  return createStyles({
    root: {
      overflow: 'auto',
    },
    header: {
      ...theme.typography.overline,
      paddingLeft: theme.spacing(7.5),
      paddingTop: theme.spacing(1),
      opacity: 0.3,
    },
    completion: {
      ...theme.typography.body1,
      padding: theme.spacing(1),
      paddingLeft: theme.spacing(7.5),
      cursor: 'pointer',
      '&.active': {
        backgroundColor: theme.palette.action.active,
        color: theme.palette.text.secondary,
        borderLeftStyle: 'solid',
        borderLeftWidth: theme.spacing(0.5),
        borderLeftColor: theme.palette.primary.main,
        paddingLeft: theme.spacing(7),
      },
    },
    highlight: {
      fontWeight: 600,
    },
  });
});

const Completions: React.FC<CompletionsProps> = (props) => {
  const { items, activeItem, onActiveChange, onSelection, className } = props;
  const classes = useStyles();
  return (
    <div className={clsx(classes.root, className)}>
      {
        items.map((item, i) => {
          const h = item as CompletionHeader;
          if (h.header) {
            return <div key={`header-${i}`} className={classes.header}>{h.header}</div>;
          }
          item = item as CompletionItem;
          return (
            <Completion
              key={item.id}
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
  const ref = React.createRef<HTMLDivElement>();
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
  React.useEffect(() => {
    if (active) {
      ref.current.scrollIntoView();
    }
  }, [active]);
  return (
    <div
      ref={ref}
      className={clsx(classes.completion, active && 'active')}
      onClick={() => onSelection(id)}
      onMouseOver={() => onActiveChange(id)}
    >
      {parts}
    </div>
  );
};

export default Completions;
