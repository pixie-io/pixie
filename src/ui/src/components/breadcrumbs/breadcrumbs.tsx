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
import {
  Theme,
  makeStyles,
  ThemeProvider,
} from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import ArrowDropDownIcon from '@material-ui/icons/ArrowDropDown';
import { Card, Popover } from '@material-ui/core';
import createTypography from '@material-ui/core/styles/createTypography';
import createSpacing from '@material-ui/core/styles/createSpacing';

import { CompletionItem } from 'app/components/autocomplete/completions';
import { Autocomplete } from 'app/components/autocomplete/autocomplete';
import useIsMounted from 'app/utils/use-is-mounted';
import { AutocompleteContext } from 'app/components/autocomplete/autocomplete-context';
import Paper from '@material-ui/core/Paper';

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
    top: spacing(-1.5),
    right: spacing(-1.6),
    background: palette.foreground.grey3,
    transform: 'rotate(45deg)',
    borderRadius: spacing(0.4),
  },
  bottomArrow: {
    position: 'absolute',
    width: spacing(3),
    height: spacing(3),
    bottom: spacing(-1.5),
    right: spacing(-1.6),
    background: palette.foreground.grey3,
    transform: 'rotate(45deg)',
    borderRadius: spacing(0.4),
  },
  arrow: {
    position: 'absolute',
    width: spacing(3),
    height: spacing(3),
    left: spacing(-1.5),
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
  getListItems: (input: string) => Promise<BreadcrumbListItem[]>;
  onClose: () => void;
  anchorEl: HTMLElement;
  placeholder: string;
  explanation?: React.ReactNode;
}

const useDialogStyles = makeStyles((theme) => createStyles({
  card: {
    width: '608px',
  },
  autocomplete: {
    maxHeight: '60vh',
  },
  completionsContainer: {
    // 80% as wide as the Command Input, and using 80% of the fontSize and 80% of the spacing
    maxWidth: '608px',
    maxHeight: '60vh',
  },
  explanationContainer: {
    backgroundColor: theme.palette.foreground.grey3,
    padding: theme.spacing(2),
    borderTop: 'solid 1px',
    borderColor: theme.palette.background.three,
  },
}));

export const DialogDropdown: React.FC<DialogDropdownProps> = ({
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

  if (anchorEl && inputRef.current) {
    inputRef.current?.focus();
  }

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
        getListItems(itemValue).then((items) => {
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
        return getListItems(input).then((items) => {
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

          return mapped;
        });
      }

      return Promise.resolve([]);
    },
    [getListItems, requireCompletion],
  );

  // Used to shrink the <Autocomplete/>'s fonts and negative space, to not be as huge / central as the Command Input.
  const compactThemeBuilder = React.useMemo<(theme: Theme) => Theme>(
    () => (theme: Theme) => ({
      ...theme,
      typography: createTypography(theme.palette, {
        fontSize: theme.typography.fontSize * 0.8,
      }),
      spacing: createSpacing((factor) => theme.spacing(factor * 0.8)),
    }),
  [],
  );

  return (
    <Popover
      classes={{ paper: classes.completionsContainer }}
      anchorEl={anchorEl}
      keepMounted
      open={Boolean(anchorEl)}
      onClose={onClose}
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
          <AutocompleteContext.Provider
            value={{
              allowTyping,
              requireCompletion,
              inputRef,
              onOpen: 'clear',
              hidden: !anchorEl,
            }}
          >
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
};

export interface BreadcrumbProps {
  title: string;
  value: string;
  selectable: boolean;
  getListItems?: (input: string) => Promise<Array<BreadcrumbListItem>>;
  onSelect?: (input: string) => void;
  omitKey?: boolean;
  placeholder?: string;
  explanation?: React.ReactElement;
}

const Breadcrumb: React.FC<BreadcrumbProps> = ({
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
};

export interface BreadcrumbListItem {
  value: string;
  title?: string;
  description?: string;
  icon?: React.ReactNode;
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

export const Breadcrumbs: React.FC<BreadcrumbsProps> = ({ breadcrumbs }) => {
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
};
