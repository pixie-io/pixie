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

import { Theme, useTheme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import Split from 'react-split';

import { buildClass } from 'app/utils/build-class';
import { WithChildren } from 'app/utils/react-boilerplate';

interface SplitPaneContextProps {
  togglePane: (id: string) => void;
}

const SplitPaneContext = React.createContext<Partial<SplitPaneContextProps>>(
  {},
);
SplitPaneContext.displayName = 'SplitPaneContext';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    height: '100%',
    '& .gutter': {
      backgroundColor: theme.palette.background.five,
    },
  },
  pane: {
    display: 'flex',
    flexDirection: 'column',
  },
  header: {
    ...theme.typography.subtitle1,
    fontWeight: theme.typography.fontWeightMedium,
    padding: theme.spacing(0.75, 3),
    cursor: 'pointer',
    backgroundColor: theme.palette.background.five,
  },
  paneContent: {
    flex: '1',
    minHeight: '0',
    overflow: 'auto',
  },
}), { name: 'SplitPane' });

interface SplitContainerProps {
  initialSizes?: number[];
  children?:
  | React.ReactElement<SplitPaneProps>
  | Array<React.ReactElement<SplitPaneProps>>;
  className?: string;
  onSizeChange?: (splits: number[]) => void;
}

interface SplitContainerState {
  collapsed: number;
  prevSizes: number[];
}

/**
 * Split pane component that supports resizing of vertical panes and headers for collapsing panes.
 * Currently this component only supports 2 panes (only 1 pane is collapsed at a time).
 */
export const SplitContainer = React.memo<SplitContainerProps>((props) => {
  const classes = useStyles();
  const theme = useTheme();
  const splitRef = React.useRef(null);
  const minPaneHeight = theme.spacing(5);
  const children = React.useMemo(() => (
    Array.isArray(props.children) ? props.children : [props.children]
  ), [props.children]);
  const onSizeChange = React.useMemo(() => (props.onSizeChange || (() => {})), [props.onSizeChange]);
  const initialSizes = React.useMemo(() => {
    if (props.initialSizes && props.initialSizes.length === children.length) {
      let sum = 0;
      props.initialSizes.forEach((size) => {
        sum += size;
      });
      if (Math.round(sum) === 100) {
        return props.initialSizes;
      }
    }
    return Array(children.length).fill(100 / children.length);
  }, [props.initialSizes, children.length]);

  const [state, setState] = React.useState<SplitContainerState>({
    collapsed: -1,
    prevSizes: initialSizes,
  });

  const handleDrag = React.useCallback(
    (sizes) => {
      onSizeChange(sizes);
      setState({ collapsed: -1, prevSizes: sizes });
    },
    [onSizeChange],
  );

  const context = React.useMemo(
    () => ({
      togglePane: (id) => {
        const i = children.findIndex((child: any) => child.props.id === id);
        if (i === -1) {
          return;
        }

        setState((prevState) => {
          if (prevState.collapsed === i) {
            return {
              ...prevState,
              collapsed: -1,
            };
          }
          return {
            prevSizes: splitRef.current.split.getSizes(),
            collapsed: i,
          };
        });
      },
    }),
    [children],
  );

  React.useEffect(() => {
    if (state.collapsed === -1) {
      splitRef.current.split.setSizes(state.prevSizes);
    } else {
      splitRef.current.split.collapse(state.collapsed);
    }
    onSizeChange(splitRef.current.split.getSizes());
  }, [state.collapsed, onSizeChange, state.prevSizes]);

  return (
    <SplitPaneContext.Provider value={context}>
      <Split
        ref={splitRef}
        sizes={initialSizes}
        className={buildClass(classes.root, props.className)}
        direction='vertical'
        minSize={minPaneHeight}
        onDragEnd={handleDrag}
        gutterSize={theme.spacing(0.5)}
        snapOffset={10}
      >
        {children}
      </Split>
    </SplitPaneContext.Provider>
  );
});
SplitContainer.displayName = 'SplitContainer';

interface SplitPaneProps {
  id: string;
  title: string;
}

export const SplitPane: React.FC<WithChildren<SplitPaneProps>> = React.memo(({
  title,
  id,
  children,
}) => {
  const classes = useStyles();
  const { togglePane } = React.useContext(SplitPaneContext);
  const headerClickHandler = React.useCallback(() => {
    togglePane(id);
  }, [id, togglePane]);

  return (
    <div className={classes.pane}>
      <div className={classes.header} onClick={headerClickHandler}>
        {title}
      </div>
      <div className={classes.paneContent}>{children}</div>
    </div>
  );
});
SplitPane.displayName = 'SplitPane';
