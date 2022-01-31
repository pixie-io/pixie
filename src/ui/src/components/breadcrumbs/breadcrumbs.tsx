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

import { ArrowDropDown as ArrowDropDownIcon } from '@mui/icons-material';
import {
  Card,
  Paper,
  Popover,
  PopoverProps,
} from '@mui/material';
import { Theme, ThemeProvider } from '@mui/material/styles';
import createTypography from '@mui/material/styles/createTypography';
import { createStyles, makeStyles } from '@mui/styles';

import { Autocomplete } from 'app/components/autocomplete/autocomplete';
import { AutocompleteContext } from 'app/components/autocomplete/autocomplete-context';
import { CompletionItem } from 'app/components/autocomplete/completions';
import useIsMounted from 'app/utils/use-is-mounted';

const useStyles = makeStyles(({
  spacing, typography, palette, breakpoints,
}: Theme) => createStyles({
  breadcrumbs: {
    height: '100%',
    scrollbarWidth: 'none', // Firefox
    '&::-webkit-scrollbar': {
      display: 'none',
    },
    [breakpoints.down('sm')]: {
      overflowX: 'scroll',
      display: 'flex',
    },
    display: 'flex',
    alignItems: 'center',
  },
  breadcrumb: {
    display: 'inline-flex',
    paddingRight: spacing(0.5),
    alignItems: 'center',
    height: '100%',
  },
  title: {
    fontWeight: typography.fontWeightMedium,
    paddingRight: spacing(0.5),
    paddingLeft: spacing(0.5),
    fontFamily: typography.monospace.fontFamily,
  },
  value: {
    color: palette.primary.main,
    whiteSpace: 'nowrap',
    fontFamily: typography.monospace.fontFamily,
    maxWidth: spacing(50),
    overflowX: 'hidden',
    textOverflow: 'ellipsis',
  },
  selectable: {
    cursor: 'pointer',
    display: 'flex',
    alignItems: 'center',
  },
  body: {
    ...typography.body2,
    display: 'inline-block',
    color: palette.text.primary,
    height: spacing(5),
  },
  content: {
    display: 'flex',
    alignItems: 'center',
    paddingLeft: spacing(1),
    height: spacing(5),
  },
  dropdownArrow: {
    height: spacing(3),
  },
  spacer: {
    width: spacing(2),
  },
  divider: {
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    backgroundColor: palette.foreground.three,
    height: '75%',
    width: '1px',
    opacity: 0.4,
  },
}), { name: 'Breadcrumbs' });

export interface DialogDropdownProps {
  onSelect: (input: string) => void;
  getListItems: (input: string) => Promise<{ items: BreadcrumbListItem[], hasMoreItems: boolean }>;
  onClose: () => void;
  anchorEl: HTMLElement;
  placeholder: string;
  explanation?: React.ReactNode;
}

// Used to shrink the <Autocomplete/>'s fonts and negative space, to not be as huge / central as the Command Input.
const themeCompactor = (theme: Theme) => ({
  ...theme,
  typography: createTypography(theme.palette, {
    fontSize: theme.typography.fontSize * 0.8,
  }),
  spacing: (factor) => theme.spacing(factor * 0.8),
});

const useDialogStyles = makeStyles((theme: Theme) => createStyles({
  card: {
    width: theme.spacing(76), // 608px
  },
  autocomplete: {
    maxHeight: '60vh',
  },
  completionsContainer: {
    maxWidth: `min(95vw, min(auto, ${theme.spacing(76)}px))`,
    maxHeight: '60vh',
  },
  explanationContainer: {
    backgroundColor: theme.palette.foreground.grey3,
    padding: theme.spacing(2),
    borderTop: 'solid 1px',
    borderColor: theme.palette.background.three,
  },
}), { name: 'DialogDropdown' });

export const DialogDropdown = React.memo<DialogDropdownProps>(({
  placeholder,
  onSelect,
  onClose,
  getListItems,
  anchorEl,
  explanation,
}) => {
  const classes = useDialogStyles();

  const mounted = useIsMounted();
  const { allowTyping, requireCompletion } = React.useContext(AutocompleteContext);
  const inputRef = React.useRef<HTMLInputElement>(null);

  React.useEffect(() => {
    if (anchorEl && inputRef.current) {
      inputRef.current.focus();
    }
  }, [anchorEl]);

  const onCompletionSelected = React.useCallback(
    (itemValue: string) => {
      if (requireCompletion) {
        // Completion required, refresh the valid options and verify that the user picked one of them.
        // As getListItems is async, we refresh it so that the check is run atomically and can't be out of date.
        if (typeof getListItems !== 'function') {
          throw new Error(
            `List items not gettable when selecting "${itemValue}"!`,
          );
        }
        getListItems(itemValue).then(({ items }) => {
          if (items.length < 1) throw new Error('Failed to match the input to a valid choice!');
          if (mounted.current) {
            onSelect(itemValue);
          }
          // These two conditionals ARE separate: onSelect is able to cause this component to unmount before onClose.
          // If we called onClose anyway, React could try to update state on an unmounted component, which is an error.
          if (mounted.current) {
            onClose();
          }
        });
      } else {
        // Completion not required, so we don't care if the user picked an item that actually exists. Skip validation.
        onSelect(itemValue);
        onClose();
      }
    },
    // `mounted` is not in this array because changing it should ONLY affect whether the resolved promise acts.
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [requireCompletion, getListItems, onClose, onSelect],
  );

  const getCompletions = React.useCallback(
    (input: string) => {
      if (typeof getListItems === 'function') {
        return getListItems(input).then(({ items, hasMoreItems }) => {
          // TODO(nick,PC-630): This should be done on the API side. Once it is, remove this defensive coding.
          const seenNames = new Set<string>();
          const deduped = items.filter((item) => {
            if (seenNames.has(item.value)) {
              return false;
            }
            seenNames.add(item.value);
            return true;
          });
          const mapped: CompletionItem[] = deduped.map((item) => ({
            type: 'item',
            id: item.value,
            description: item.description,
            icon: item.icon,
            title: item?.title ?? item.value,
            highlights: item.highlights ?? [],
            autoSelectPriority: item.autoSelectPriority ?? 0,
          }));

          // If we don't require that the final result be one of the suggested items,
          // then add the user's current input as the first option, unless they have
          // already typed an existing option.
          if (!requireCompletion && !seenNames.has(input)) {
            mapped.unshift({
              type: 'item',
              id: input,
              description: '',
              icon: '',
              title: input,
              autoSelectPriority: 1,
            });
          }

          return { items: mapped, hasMoreItems };
        });
      }

      return Promise.resolve({ items: [], hasMoreItems: false });
    },
    [getListItems, requireCompletion],
  );

  const autocompleteContextValue = React.useMemo(() => ({
    allowTyping,
    requireCompletion,
    inputRef,
    onOpen: 'clear' as const,
    hidden: !anchorEl,
  }), [allowTyping, requireCompletion, anchorEl]);

  const popoverProps: PopoverProps = React.useMemo(() => ({
    elevation: 0,
    keepMounted: false,
    classes: { paper: classes.completionsContainer },
    onClose,
    anchorEl,
    open: !!anchorEl,
    anchorOrigin: { vertical: 'bottom', horizontal: 'left' },
    transformOrigin: { vertical: 'top', horizontal: 'left' },
  }), [onClose, anchorEl, classes.completionsContainer]);

  return (
    <Popover {...popoverProps}>
      <ThemeProvider theme={themeCompactor}>
        <Card className={classes.card}>
          <AutocompleteContext.Provider value={autocompleteContextValue}>
            <Autocomplete
              className={classes.autocomplete}
              placeholder={placeholder}
              onSelection={onCompletionSelected}
              getCompletions={getCompletions}
            />
          </AutocompleteContext.Provider>
          {
            explanation != null
            && (
              <div className={classes.explanationContainer}>
                { explanation }
              </div>
            )
          }
        </Card>
      </ThemeProvider>
    </Popover>
  );
});
DialogDropdown.displayName = 'DialogDropdown';

export interface BreadcrumbProps {
  title: string;
  value: string;
  selectable: boolean;
  getListItems?: (input: string) => Promise<{ items: BreadcrumbListItem[], hasMoreItems: boolean }>;
  onSelect?: (input: string) => void;
  omitKey?: boolean;
  placeholder?: string;
  explanation?: React.ReactElement;
}

const Breadcrumb = React.memo<BreadcrumbProps>(({
  title,
  value,
  selectable,
  getListItems,
  onSelect,
  omitKey,
  placeholder,
  explanation,
}) => {
  const classes = useStyles();
  const [anchorEl, setAnchorEl] = React.useState<null | HTMLElement>(null);

  const handleClick = React.useCallback(
    (event: React.MouseEvent<HTMLElement>) => {
      setAnchorEl(event.currentTarget.parentElement);
    },
    [setAnchorEl],
  );

  const onClose = React.useCallback(() => {
    setAnchorEl(null);
    // The input from the autocomplete retains focus despite being hidden otherwise.
    (document.activeElement as HTMLElement)?.blur();
  }, [setAnchorEl]);

  return (
    <div className={classes.breadcrumb}>
      <div className={classes.body}>
        <div className={classes.content}>
          {!omitKey && <span className={classes.title}>{`${title}: `}</span>}
          <span className={selectable ? classes.selectable : ''} onClick={selectable ? handleClick : null}>
            <span className={classes.value} >{value}</span>
            {selectable && (
              <div className={classes.dropdownArrow}>
                <ArrowDropDownIcon />
              </div>
            )}
          </span>
          {!selectable && <div className={classes.spacer} />}
          <DialogDropdown
            placeholder={placeholder || 'Filter...'}
            onSelect={onSelect}
            onClose={onClose}
            getListItems={getListItems}
            anchorEl={anchorEl}
            explanation={explanation}
          />
        </div>
      </div>
    </div>
  );
});
Breadcrumb.displayName = 'Breadcrumb';

export interface BreadcrumbListItem {
  value: string;
  title?: string;
  description?: string;
  icon?: React.ReactNode;
  /** @see{CompletionItem} */
  highlights?: number[];
  /** @see{CompletionItem} */
  autoSelectPriority?: number;
}

export interface BreadcrumbOptions extends BreadcrumbProps {
  allowTyping?: boolean;
  requireCompletion?: boolean;
  divider?: boolean;
}

export interface BreadcrumbsProps {
  breadcrumbs: BreadcrumbOptions[];
}

export const Breadcrumbs: React.FC<BreadcrumbsProps> = React.memo(({ breadcrumbs }) => {
  const classes = useStyles();
  // In case a breadcrumb doesn't override, give it the nearest context's values.
  const { allowTyping, requireCompletion } = React.useContext(
    AutocompleteContext,
  );
  return (
    <Paper elevation={2}>
      <div className={classes.breadcrumbs}>
        {breadcrumbs.map((breadcrumb, i) => (
          // Key is needed to prevent a console error when a key is missing in a list element.
          <AutocompleteContext.Provider
            key={`i-${breadcrumb.title}`}
            value={{
              allowTyping: breadcrumb.allowTyping ?? allowTyping,
              requireCompletion:
                breadcrumb.requireCompletion ?? requireCompletion,
              onOpen: 'clear',
            }}
          >
            <Breadcrumb key={i} {...breadcrumb} />
            {
              breadcrumb.divider && i !== breadcrumbs.length - 1 && <div className={classes.divider} />
            }
          </AutocompleteContext.Provider>
        ))}
      </div>
    </Paper>
  );
});
Breadcrumbs.displayName = 'Breadcrumbs';
