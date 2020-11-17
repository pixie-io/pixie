import clsx from 'clsx';
import * as React from 'react';
import Split from 'react-split';

import {
  createStyles,
  makeStyles,
  Theme,
  useTheme,
} from '@material-ui/core/styles';

interface SplitPaneContextProps {
  togglePane: (id: string) => void;
}

const SplitPaneContext = React.createContext<Partial<SplitPaneContextProps>>(
  {}
);

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    root: {
      height: '100%',
      '& .gutter': {
        backgroundColor: theme.palette.background.three,
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
      backgroundColor: theme.palette.background.three,
    },
    paneContent: {
      flex: '1',
      minHeight: '0',
      overflow: 'auto',
    },
  })
);

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

// Split pane component that supports resizing of vertial panes and headers for collapsing panes.
// Currently this component only supports 2 panes (only 1 pane is collapsed at a time).
export const SplitContainer = (
  props: React.PropsWithChildren<SplitContainerProps>
) => {
  const classes = useStyles();
  const theme = useTheme();
  const splitRef = React.useRef(null);
  const minPaneHeight = theme.spacing(5);
  const children = Array.isArray(props.children)
    ? props.children
    : [props.children];
  const onSizeChange =
    props.onSizeChange ||
    (() => {
      /* noop */
    });
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

  // TODO(malthus): Persist the state with localstorage or apollo client.
  const [state, setState] = React.useState<SplitContainerState>({
    collapsed: -1,
    prevSizes: initialSizes,
  });

  const handleDrag = React.useCallback(
    (sizes) => {
      onSizeChange(sizes);
      setState({ collapsed: -1, prevSizes: sizes });
    },
    [onSizeChange]
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
    [children]
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
        className={clsx(classes.root, props.className)}
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
};

interface SplitPaneProps {
  id: string;
  title: string;
}

export const SplitPane: React.FC<SplitPaneProps> = ({
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
};
