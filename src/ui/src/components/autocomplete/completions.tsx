/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import * as React from 'react';

import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { PodIcon } from 'app/components/icons/pod';
import { ServiceIcon } from 'app/components/icons/service';
import { isInView } from 'app/utils/bbox';
import { buildClass } from 'app/utils/build-class';

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
  /**
   * As the user types, the first match is often preselected for them. Instead of using the very first match, this can
   * select the first match that has the highest or is tied for the highest autoSelectPriority. Default 0.
   */
  autoSelectPriority?: number;
}

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
    backgroundColor: theme.palette.background.one,
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
    color: theme.palette.text.disabled,
    backgroundColor: theme.palette.background.one,
    paddingTop: theme.spacing(1),
    paddingBottom: theme.spacing(1),
    cursor: 'pointer',
    '& > span': {
      ...theme.typography.body1,
      fontFamily: theme.typography.monospace.fontFamily,
      '&$highlight': { fontWeight: 600 },
    },
    '&.active': {
      color: theme.palette.text.primary,
      backgroundColor: theme.palette.background.five,
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
}), { name: 'Completions' });

type CompletionProps = Omit<CompletionItem, 'type'> & {
  active: boolean;
  onSelection: (id: string) => void;
  onActiveChange: (id: string) => void;
  /** If set, this will replace the default icon for the completion's type. */
  icon?: React.ReactNode;
};

// eslint-disable-next-line react-memo/require-memo
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

  // Keep the hovered element in the scroll view. This is debounced to prevent two scenarios:
  // 1) Bouncing back to a previous scroll position if scrolling and moving the cursor at the same time
  // 2) Firing 'scrollIntoView` repeatedly while it's already running, causing user-visible performance problems
  const [recentlyScrolled, setRecentlyScrolled] = React.useState(false);
  React.useEffect(() => {
    setRecentlyScrolled(true);
    const delay = setTimeout(() => { setRecentlyScrolled(false); }, 100);
    return () => {
      clearTimeout(delay);
      setRecentlyScrolled(false);
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [ref.current?.parentElement.scrollTop, ref.current?.parentElement.scrollHeight]);
  React.useEffect(() => {
    if (active
        && !recentlyScrolled
        && ref.current.matches(':hover')
        && !isInView(ref.current.parentElement, ref.current)
    ) {
      ref.current.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
    }
  }, [active, ref, recentlyScrolled]);

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
      className={buildClass(classes.completion, active && 'active')}
      onClick={() => onSelection(id)}
      onMouseOver={() => onActiveChange(id)}
    >
      <div className={buildClass(classes.itemType, stateClass)}>
        {icon ?? entityIcon ?? null}
      </div>
      {parts}
    </div>
  );
};
CompletionInternal.displayName = 'CompletionInternal';
CompletionInternal.defaultProps = {
  icon: null,
};

// eslint-disable-next-line react-memo/require-memo
export const Completion: React.FC<CompletionProps> = (props) => {
  const { title } = props;

  if (!title) {
    return null;
  }
  // Need an internal component because of useEffect.
  return <CompletionInternal {...props} />;
};
Completion.displayName = 'Completion';

// eslint-disable-next-line react-memo/require-memo
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
    <div className={buildClass(classes.root, className)}>
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
Completions.displayName = 'Completions';
