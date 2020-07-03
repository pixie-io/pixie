import clsx from 'clsx';
import * as React from 'react';
import { isInView } from 'utils/bbox';

import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';

interface CompletionsProps {
  items: CompletionItems;
  activeItem?: string;
  onActiveChange: (id: string) => void;
  onSelection: (id: string) => void;
  className?: string;
}

export type CompletionItems = Array<CompletionItem | CompletionHeader>;

export interface CompletionHeader {
  type: 'header';
  header: string;
}

export type CompletionId = string;
export type CompletionTitle = string;

export interface CompletionItem {
  type: 'item';
  id: CompletionId;
  title: CompletionTitle;
  description?: string;
  highlights?: Array<number>;
  itemType?: string;
}

const useStyles = makeStyles((theme: Theme) => (
  // TODO(malthus): Make use of the theme styles.
  createStyles({
    root: {
      display: 'flex',
      flexDirection: 'row',
    },
    items: {
      overflow: 'auto',
      flex: 3,
      '& > *': {
        paddingLeft: theme.spacing(7.5),
      },
    },
    description: {
      flex: 2,
      borderLeftStyle: 'solid',
      borderLeftWidth: '1px',
      borderLeftColor: theme.palette.divider,
      paddingLeft: theme.spacing(3),
      paddingRight: theme.spacing(3),
      paddingBottom: theme.spacing(3),
      overflow: 'hidden',
    },
    header: {
      ...theme.typography.overline,
      paddingTop: theme.spacing(1),
      opacity: 0.3,
    },
    completion: {
      ...theme.typography.body1,
      paddingTop: theme.spacing(1),
      paddingBottom: theme.spacing(1),
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
  })));

const Completions: React.FC<CompletionsProps> = (props) => {
  const {
    items, activeItem, onActiveChange, onSelection, className,
  } = props;
  const classes = useStyles();

  const description = (() => {
    for (const item of items) {
      if (item.type === 'item' && item.id === activeItem) {
        return item.description || null;
      }
    }
    return null;
  })();

  return (
    <div className={clsx(classes.root, className)}>
      <div className={classes.items}>
        {
          items.map((item, i) => {
            switch (item.type) {
              case 'header':
                return <div key={`header-${i}`} className={classes.header}>{item.header}</div>;
              case 'item':
                return (
                  <Completion
                    key={item.id}
                    active={item.id === activeItem}
                    onSelection={onSelection}
                    onActiveChange={onActiveChange}
                    {...item}
                  />
                );
              default:
                throw new Error('unknown type');
            }
          })
        }
      </div>
      {
        description
        && (
        <div className={classes.description}>
          <div className={classes.header}>Description</div>
          {description}
        </div>
        )
      }
    </div>
  );
};

type CompletionProps = Omit<CompletionItem, 'type'> & {
  active: boolean;
  onSelection: (id: string) => void;
  onActiveChange: (id: string) => void;
};

export const Completion = (props: CompletionProps) => {
  const { title } = props;

  if (!title) {
    return null;
  }
  // Need an internal component because of useEffect.
  return <CompletionInternal {...props} />;
};

const CompletionInternal = (props: CompletionProps) => {
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
  const ref = React.createRef<HTMLDivElement>();
  let remainingIdx = 0;
  for (const start of highlights) {
    const prev = title.substring(remainingIdx, start);
    if (prev) {
      parts.push(<span key={`title-${remainingIdx}`}>{prev}</span>);
    }
    const highlight = title.substring(start, start + 1);
    if (highlight) {
      parts.push(<span key={`title-${start}`} className={classes.highlight}>{highlight}</span>);
    }
    remainingIdx = start + 1;
  }
  if (remainingIdx < title.length) {
    parts.push(
      <span key={`title-${remainingIdx}`}>
        {title.substring(remainingIdx)}
      </span>,
    );
  }
  React.useEffect(() => {
    if (active && !isInView(ref.current.parentElement, ref.current)) {
      ref.current.scrollIntoView();
    }
  }, [active, ref]);
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
CompletionInternal.displayName = "CompletionInternal";

export default Completions;
