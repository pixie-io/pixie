import clsx from 'clsx';
import * as React from 'react';

import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';

import { PodIcon } from 'components/icons/pod';
import { ServiceIcon } from 'components/icons/service';
import { isInView } from 'utils/bbox';

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
  state?: string;
  icon?: React.ReactNode;
}

// TODO(malthus): Make use of the theme styles.
const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    display: 'flex',
    flexDirection: 'row',
  },
  items: {
    overflow: 'auto',
    flex: 3,
    '& > *': {
      paddingLeft: theme.spacing(2),
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
      paddingLeft: theme.spacing(1.5),
    },
    display: 'flex',
    alignItems: 'center',
  },
  highlight: {
    fontWeight: 600,
  },
  status: {
    width: theme.spacing(4),
    display: 'inline-block',
    paddingLeft: theme.spacing(1),
  },
  itemType: {
    width: theme.spacing(4),
    display: 'inline-block',
  },
  healthy: {
    '& svg': {
      fill: theme.palette.success.main,
    },
  },
  unhealthy: {
    '& svg': {
      fill: theme.palette.error.main,
    },
  },
  pending: {
    '& svg': {
      fill: theme.palette.warning.main,
    },
  },
  terminated: {
    '& svg': {
      fill: theme.palette.foreground?.grey1,
    },
  },
}));

export const Completions: React.FC<CompletionsProps> = (props) => {
  const {
    items, activeItem, onActiveChange, onSelection, className,
  } = props;
  const classes = useStyles();

  const description = (() => {
    for (let i = 0; i < items.length; ++i) {
      const item = items[i];
      if (item.type === 'item' && item.id === activeItem) {
        return item.description || null;
      }
    }
    return null;
  })();

  return (
    <div className={clsx(classes.root, className)}>
      <div className={classes.items}>
        {items.map((item, i) => {
          switch (item.type) {
            case 'header':
              return (
                <div key={`header-${i}`} className={classes.header}>
                  {item.header}
                </div>
              );
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
        })}
      </div>
      {description && (
        <div className={classes.description}>
          <div className={classes.header}>Description</div>
          {description}
        </div>
      )}
    </div>
  );
};

type CompletionProps = Omit<CompletionItem, 'type'> & {
  active: boolean;
  onSelection: (id: string) => void;
  onActiveChange: (id: string) => void;
  /** If set, this will replace the default icon for the completion's type. */
  icon?: React.ReactNode;
};

export const Completion: React.FC<CompletionProps> = (props) => {
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
    state,
    itemType,
    icon,
  } = props;

  const classes = useStyles();
  const parts = [];
  const ref = React.createRef<HTMLDivElement>();
  let remainingIdx = 0;

  highlights.forEach((start) => {
    const prev = title.substring(remainingIdx, start);
    if (prev) {
      parts.push(<span key={`title-${remainingIdx}`}>{prev}</span>);
    }
    const highlight = title.substring(start, start + 1);
    if (highlight) {
      parts.push(
        <span key={`title-${start}`} className={classes.highlight}>
          {highlight}
        </span>,
      );
    }
    remainingIdx = start + 1;
  });
  if (remainingIdx < title.length) {
    parts.push(
      <span key={`title-${remainingIdx}`}>{title.substring(remainingIdx)}</span>,
    );
  }
  React.useEffect(() => {
    if (active && !isInView(ref.current.parentElement, ref.current)) {
      ref.current.scrollIntoView();
    }
  }, [active, ref]);

  let stateClass = null;
  switch (state) {
    case 'AES_TERMINATED':
      stateClass = classes.terminated;
      break;
    case 'AES_FAILED':
      stateClass = classes.unhealthy;
      break;
    case 'AES_PENDING':
      stateClass = classes.pending;
      break;
    case 'AES_RUNNING':
      stateClass = classes.healthy;
      break;
    default:
  }

  let entityIcon = null;
  switch (itemType) {
    case 'svc':
      entityIcon = <ServiceIcon />;
      break;
    case 'pod':
      entityIcon = <PodIcon />;
      break;
    default:
  }

  return (
    <div
      ref={ref}
      className={clsx(classes.completion, active && 'active')}
      onClick={() => onSelection(id)}
      onMouseOver={() => onActiveChange(id)}
    >
      <div className={clsx(classes.itemType, stateClass)}>
        {icon ?? entityIcon ?? null}
      </div>
      {parts}
    </div>
  );
};
CompletionInternal.displayName = 'CompletionInternal';
