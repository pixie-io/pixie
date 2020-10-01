import * as React from 'react';
import {
  createStyles, Theme, WithStyles, withStyles, ThemeProvider,
} from '@material-ui/core/styles';
import ArrowDropDownIcon from '@material-ui/icons/ArrowDropDown';
import { Card, Popover } from '@material-ui/core';
import createTypography from '@material-ui/core/styles/createTypography';
import createSpacing from '@material-ui/core/styles/createSpacing';
import { CompletionItem } from 'components/autocomplete/completions';
import Autocomplete from 'components/autocomplete';
import { AutocompleteContext } from 'components/autocomplete/autocomplete';

const styles = ({ spacing, typography, palette }: Theme) => createStyles({
  breadcrumbs: {
    height: '100%',
    overflowX: 'scroll',
    scrollbarWidth: 'none', // Firefox
    '&::-webkit-scrollbar': {
      display: 'none',
    },
    display: 'flex',
  },
  breadcrumb: {
    display: 'inline-flex',
    paddingRight: spacing(0.5),
    alignItems: 'center',
  },
  title: {
    fontWeight: typography.fontWeightMedium,
    paddingRight: spacing(0.5),
    paddingLeft: spacing(0.5),
    fontFamily: '"Roboto Mono", Monospace',
  },
  card: {
    width: '608px',
  },
  autocomplete: {
    maxHeight: '60vh',
  },
  value: {
    color: palette.primary.main,
    whiteSpace: 'nowrap',
    fontFamily: '"Roboto Mono", Monospace',
  },
  body: {
    ...typography.body2,
    display: 'inline-block',
    color: palette.text.primary,
    height: spacing(3),
  },
  content: {
    display: 'flex',
    alignItems: 'center',
    paddingLeft: spacing(1),
    height: spacing(3),
  },
  dropdownArrow: {
    cursor: 'pointer',
    height: spacing(3),
  },
  spacer: {
    width: spacing(2),
  },
  input: {
    padding: `0 ${spacing(1)}px ${spacing(1)}px ${spacing(1)}px`,
  },
  completionsContainer: {
    // 80% as wide as the Command Input, and using 80% of the fontSize and 80% of the spacing
    maxWidth: '608px',
    maxHeight: '60vh',
  },
  // CSS for the front/back of the breadcrumb element.
  triangle: {
    position: 'absolute',
    height: spacing(3),
    width: spacing(3),
    overflow: 'hidden',
  },
  angle: {
    float: 'right', // Take the triangle out of the DOM, so it can overlap with other breadcrumbs.
  },
  tail: {
    width: spacing(2.4),
    height: spacing(3),
  },
  topArrow: {
    position: 'absolute',
    width: spacing(3),
    height: spacing(3),
    top: -1 * spacing(1.5),
    right: -1 * spacing(1.6),
    background: palette.foreground.grey3,
    transform: 'rotate(45deg)',
    borderRadius: spacing(0.4),
  },
  bottomArrow: {
    position: 'absolute',
    width: spacing(3),
    height: spacing(3),
    bottom: `-${spacing(1.5)}px`,
    right: `-${spacing(1.6)}px`,
    background: palette.foreground.grey3,
    transform: 'rotate(45deg)',
    borderRadius: spacing(0.4),
  },
  arrow: {
    position: 'absolute',
    width: spacing(3),
    height: spacing(3),
    left: `-${spacing(1.5)}px`,
    background: palette.foreground.grey3,
    transform: 'rotate(45deg)',
    borderRadius: spacing(0.4),
  },
  inputItem: {
    '&:hover': {
      backgroundColor: 'initial',
    },
    '&.Mui-focusVisible': {
      backgroundColor: 'initial',
    },
  },
  separator: {
    display: 'flex',
    alignItems: 'center',
    color: palette.foreground.one,
    fontWeight: 1000,
    width: spacing(1),
  },
  icon: {
    minWidth: spacing(4),
  },
});

interface DialogDropdownProps extends WithStyles<typeof styles> {
  onSelect: (input: string) => void;
  getListItems: (input: string) => Promise<BreadcrumbListItem[]>;
  onClose: () => void;
  anchorEl: HTMLElement;
}

const DialogDropdown = ({
  classes, onSelect, onClose, getListItems, anchorEl,
}: DialogDropdownProps) => {
  const { allowTyping, requireCompletion, preSelect } = React.useContext(AutocompleteContext);
  const [completionItems, setCompletionItems] = React.useState<CompletionItem[]>([]);
  const inputRef = React.useRef<HTMLInputElement>(null);

  if (anchorEl && inputRef.current) {
    setTimeout(() => {
      inputRef.current?.focus();
    }, 100);
  }

  const onCompletionSelected = React.useCallback((itemValue: string) => {
    if (requireCompletion) {
      // Completion required, refresh the valid options and verify that the user picked one of them.
      // As getListItems is async, we refresh it so that the check is run atomically and can't be out of date.
      if (typeof getListItems !== 'function') throw new Error(`List items not gettable when selecting "${itemValue}"!`);
      getListItems(itemValue).then((items) => {
        if (items.length < 1) throw new Error('Failed to match the input to a valid choice!');
        onSelect(itemValue);
        onClose();
      });
    } else {
      // Completion not required, so we don't care if the user picked an item that actually exists. Skip validation.
      onSelect(itemValue);
      onClose();
    }
  }, [requireCompletion, getListItems, onClose, onSelect]);

  const getCompletions = React.useCallback((input: string) => {
    if (typeof getListItems === 'function') {
      return getListItems(input).then((items) => {
        const mapped: CompletionItem[] = items.map((item) => ({
          type: 'item' as 'item',
          id: item.value,
          description: item.description,
          icon: item.icon,
          title: item.value,
        }));
        setCompletionItems(mapped);
        return mapped;
      });
    }

    setCompletionItems([]);
    return Promise.resolve([]);
  }, [getListItems]);

  // Used to shrink the <Autocomplete/>'s fonts and negative space, to not be as huge / central as the Command Input.
  const compactThemeBuilder = React.useMemo<(theme: Theme) => Theme>(() => (theme: Theme) => ({
    ...theme,
    typography: createTypography(theme.palette, { fontSize: theme.typography.fontSize * 0.8 }),
    spacing: createSpacing((factor) => (factor * 0.8 * theme.spacing(1))),
  }), []);

  return (
    <Popover
      classes={{ paper: classes.completionsContainer }}
      anchorEl={anchorEl}
      keepMounted
      open={Boolean(anchorEl)}
      onClose={onClose}
      getContentAnchorEl={null}
      anchorOrigin={{
        vertical: 'bottom',
        horizontal: 'left',
      }}
      transformOrigin={{
        vertical: 'top',
        horizontal: 'left',
      }}
      elevation={0}
    >
      <ThemeProvider theme={compactThemeBuilder}>
        <Card className={classes.card}>
          <AutocompleteContext.Provider value={{
            allowTyping,
            requireCompletion,
            inputRef,
            preSelect: !!(preSelect && anchorEl),
          }}
          >
            <Autocomplete
              className={classes.autocomplete}
              placeholder='Filter...'
              onSelection={onCompletionSelected}
              getCompletions={getCompletions}
            />
          </AutocompleteContext.Provider>
        </Card>
      </ThemeProvider>
    </Popover>
  );
};

interface BreadcrumbProps extends WithStyles<typeof styles> {
  title: string;
  value: string;
  selectable: boolean;
  getListItems?: (input: string) => Promise<Array<BreadcrumbListItem>>;
  onSelect?: (input: string) => void;
  omitKey?: boolean;
}

const Breadcrumb = ({
  classes, title, value, selectable, getListItems, onSelect, omitKey,
}: BreadcrumbProps) => {
  const [anchorEl, setAnchorEl] = React.useState<null | HTMLElement>(null);

  const handleClick = React.useCallback((event: React.MouseEvent<HTMLElement>) => {
    setAnchorEl(event.currentTarget.parentElement);
  }, [setAnchorEl]);

  const onClose = React.useCallback(() => {
    setAnchorEl(null);
  }, [setAnchorEl]);

  return (
    <div className={classes.breadcrumb}>
      <div className={classes.body}>
        <div className={classes.content}>
          {!omitKey && <span className={classes.title}>{`${title}: `}</span>}
          <span className={classes.value}>{value}</span>
          {selectable && <div className={classes.dropdownArrow} onClick={handleClick}><ArrowDropDownIcon /></div>}
          {!selectable && <div className={classes.spacer} />}
          <DialogDropdown
            classes={classes}
            onSelect={onSelect}
            onClose={onClose}
            getListItems={getListItems}
            anchorEl={anchorEl}
          />
        </div>
      </div>
    </div>
  );
};

interface BreadcrumbListItem {
  value: string;
  description?: string;
  icon?: React.ReactNode;
}

export interface BreadcrumbOptions {
  title: string;
  value: string;
  selectable: boolean;
  allowTyping?: boolean;
  getListItems?: (input: string) => Promise<Array<BreadcrumbListItem>>;
  onSelect?: (input: string) => void;
  requireCompletion?: boolean;
}

interface BreadcrumbsProps extends WithStyles<typeof styles> {
  breadcrumbs: BreadcrumbOptions[];
}

// TODO(nserrino/michelle): Support links (non-menu) as a type of breadcrumb,
// replace breadcrumbs in cluster details page with that new type of breadcrumb.
const Breadcrumbs = ({
  classes, breadcrumbs,
}: BreadcrumbsProps) => {
  // In case a breadcrumb doesn't override, give it the nearest context's values.
  const { allowTyping, requireCompletion } = React.useContext(AutocompleteContext);
  return (
    <div className={classes.breadcrumbs}>
      {
        breadcrumbs.map((breadcrumb, i) => (
          // Key is needed to prevent a console error when a key is missing in a list element.
          <AutocompleteContext.Provider
            key={`i-${breadcrumb.title}`}
            value={{
              allowTyping: breadcrumb.allowTyping ?? allowTyping,
              requireCompletion: breadcrumb.requireCompletion ?? requireCompletion,
              preSelect: true,
            }}
          >
            <Breadcrumb
              key={i}
              classes={classes}
              {...breadcrumb}
            />
          </AutocompleteContext.Provider>
        ))
      }
    </div>
  );
};

export default withStyles(styles)(Breadcrumbs);
