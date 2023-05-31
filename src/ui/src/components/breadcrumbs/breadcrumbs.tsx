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
  Fade,
  FadeProps,
  Paper,
  Popover,
  PopoverProps,
} from '@mui/material';
import { Theme, ThemeProvider, alpha } from '@mui/material/styles';
import createTypography from '@mui/material/styles/createTypography';
import { createStyles, makeStyles } from '@mui/styles';
import { deepmerge } from '@mui/utils';

import { Autocomplete } from 'app/components/autocomplete/autocomplete';
import { AutocompleteContext } from 'app/components/autocomplete/autocomplete-context';
import { CompletionItem } from 'app/components/autocomplete/completions';
import { buildClass } from 'app/utils/build-class';
import useIsMounted from 'app/utils/use-is-mounted';

const TRANSITION_DURATION_MS = 250;

const useStyles = makeStyles(({
  shape, spacing, typography, palette, breakpoints,
}: Theme) => createStyles({
  triggerWrapper: {
    background: 'none',
  },
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

    transition: 'all 0.125s linear',
    border: `1px ${palette.foreground.grey1} solid`,
    '&:not($active)': {
      '&:not(:first-child)': { borderLeftColor: 'transparent' },
      '&:not(:last-child)': { borderRightColor: 'transparent' },
      '&:first-child': { borderTopLeftRadius: spacing(2), borderBottomLeftRadius: spacing(2) },
      '&:last-child': { borderTopRightRadius: spacing(2), borderBottomRightRadius: spacing(2) },
    },
  },
  active: {
    backgroundColor: palette.background.three,
    position: 'relative',
    // Illusion to merge this element's border with the popover
    borderTopLeftRadius: shape.borderRadius,
    borderTopRightRadius: shape.borderRadius,
    borderBottomLeftRadius: 0,
    borderBottomRightRadius: 0,
  },
  title: {
    cursor: 'pointer',
    fontWeight: typography.fontWeightMedium,
    paddingRight: spacing(0.5),
    paddingLeft: spacing(0.5),
  },
  value: {
    color: palette.primary.main,
    whiteSpace: 'nowrap',
    fontFamily: typography.monospace.fontFamily,
    maxWidth: spacing(50),
    overflowX: 'hidden',
    textOverflow: 'ellipsis',
    // Font metrics are a bit wonky between this and the body font next to it, so re-center
    position: 'relative',
    top: '-1px',
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
    height: spacing(4),
  },
  content: {
    display: 'flex',
    alignItems: 'center',
    paddingLeft: spacing(1),
    height: spacing(4),
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
    height: '100%',
    width: '3px',
    borderTop: `1px ${palette.foreground.grey1} solid`,
    borderBottom: `1px ${palette.foreground.grey1} solid`,
    position: 'relative',
    left: '-1px',
    marginRight: '-2px',

    '&::after': {
      display: 'block',
      pointerEvents: 'none',
      color: 'transparent',
      content: '"\u00a0"', // nbsp
      fontSize: '0.01px',
      backgroundColor: palette.foreground.three,
      margin: '12.5% 0',
      height: '75%',
      width: '1px',
      opacity: 0.4,
    },
  },
  dividerHidden: {
    '&::after': { opacity: 0 },
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
const themeCompactor = (theme: Theme) => (deepmerge<any>(
  theme,
  {
    typography: createTypography(theme.palette, {
      fontSize: theme.typography.fontSize * 0.8,
    }),
    spacing: (factor: number) => theme.spacing(factor * 0.8),
  },
));

const useDialogStyles = makeStyles((theme: Theme) => createStyles({
  card: {
    width: theme.spacing(76), // 608px
    // Make it look like it's selected
    borderTopLeftRadius: 0,
    border: `1px ${alpha(theme.palette.foreground.grey1, 1)} solid`,

    // An illusion to erase the part of the border between the anchor and the popover so they appear to be one element
    // These two CSS vars are computed when the card opens, so that the illusion can be placed correctly.
    '--illusion-border-width': '0px',
    '--illusion-border-left': '0px',
    '&::after': {
      pointerEvents: 'none',
      color: 'transparent',
      content: '"\u00a0"', // nbsp
      fontSize: '0.01px',
      zIndex: theme.zIndex.modal + 2,
      position: 'absolute',
      top: '-1px',
      left: 'calc(var(--illusion-border-left) + 1px)',
      width: 'max(0px, calc(var(--illusion-border-width) - 2px))',
      height: 0,
      borderBottom: `2px ${theme.palette.background.three} solid`,
    },
  },
  autocomplete: {
    maxHeight: '60vh',
    borderRadius: 'inherit',
    overflow: 'hidden', // Scroll container is deeper within
  },
  completionsContainer: {
    overflow: 'visible', // So the border doesn't create a nesting scrollbar
    borderTopLeftRadius: 0,
    maxWidth: `min(95vw, min(auto, ${theme.spacing(76)}px))`,
    maxHeight: '60vh',
  },
  explanationContainer: {
    backgroundColor: theme.palette.foreground.grey3,
    padding: theme.spacing(2),
    borderTop: 'solid 1px',
    borderColor: theme.palette.background.five,
  },
}), { name: 'DialogDropdown' });

// eslint-disable-next-line react/display-name
const DialogTransition = React.forwardRef<typeof Fade, FadeProps>(
  // eslint-disable-next-line react-memo/require-memo
  (props, ref) => <Fade ref={ref} timeout={TRANSITION_DURATION_MS} {...props} />,
);

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
    elevation: 3,
    keepMounted: false,
    classes: { paper: classes.completionsContainer },
    onClose,
    anchorEl,
    open: !!anchorEl,
    anchorOrigin: { vertical: 'bottom', horizontal: 'left' },
    transformOrigin: { vertical: 'top', horizontal: 'left' },
    TransitionComponent: DialogTransition,
    transitionDuration: TRANSITION_DURATION_MS,
  }), [onClose, anchorEl, classes.completionsContainer]);

  // To place the eraser border, we need to figure out where the anchor element actually is relative to the dialog.
  const [cardEl, setCardEl] = React.useState<HTMLElement>(null);
  const setCardRef = React.useCallback((el?: HTMLElement) => setCardEl(el), []);
  const [cardLeft, setCardLeft] = React.useState(0);
  React.useEffect(() => {
    if (anchorEl && cardEl) {
      // Need to delay for the dialog's position to change
      setTimeout(() => {
        setCardLeft(cardEl.getBoundingClientRect().x);
      });
    } else {
      setCardLeft(0);
    }
  }, [anchorEl, cardEl]);

  const cardStyle: React.CSSProperties = React.useMemo(() => {
    let w = 0;
    let l = 0;

    if (anchorEl) {
      w = anchorEl.offsetWidth;
      const { x: anchorLeft } = anchorEl.getBoundingClientRect();
      l = anchorLeft - cardLeft;
    }

    return {
      '--illusion-border-width': `${w}px`,
      '--illusion-border-left': `${l}px`,
    } as unknown as React.CSSProperties;
  }, [cardLeft, anchorEl]);

  return (
    <Popover {...popoverProps}>
      <ThemeProvider theme={themeCompactor}>
        <Card className={classes.card} ref={setCardRef} style={cardStyle}>
          <AutocompleteContext.Provider value={autocompleteContextValue}>
            <Autocomplete
              className={classes.autocomplete}
              placeholder={placeholder}
              onSelection={onCompletionSelected}
              getCompletions={getCompletions}
              hint={explanation && (
                <div className={classes.explanationContainer}>
                  { explanation }
                </div>
              )}
            />
          </AutocompleteContext.Provider>
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
      setAnchorEl(event.currentTarget);
    },
    [setAnchorEl],
  );

  const onClose = React.useCallback(() => {
    setAnchorEl(null);
    // The input from the autocomplete retains focus despite being hidden otherwise.
    (document.activeElement as HTMLElement)?.blur();
  }, [setAnchorEl]);

  return (
    <>
      <div
        className={buildClass([classes.breadcrumb, !!anchorEl && classes.active])}
        onClick={selectable ? handleClick : null}
      >
        <div className={classes.body}>
          <div className={classes.content}>
            {!omitKey && <span className={classes.title} >{`${title}: `}</span>}
            <span className={selectable ? classes.selectable : ''}>
              <span className={classes.value} >{value}</span>
              {selectable && (
                <div className={classes.dropdownArrow}>
                  <ArrowDropDownIcon />
                </div>
              )}
            </span>
            {!selectable && <div className={classes.spacer} />}
          </div>
        </div>
      </div>
      <DialogDropdown
        placeholder={placeholder || 'Filter...'}
        onSelect={onSelect}
        onClose={onClose}
        getListItems={getListItems}
        anchorEl={anchorEl}
        explanation={explanation}
      />
    </>
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
    <Paper className={classes.triggerWrapper} elevation={0}>
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
              i !== breadcrumbs.length - 1 && (
                <div className={buildClass([classes.divider, !breadcrumb.divider && classes.dividerHidden])} />
              )
            }
          </AutocompleteContext.Provider>
        ))}
      </div>
    </Paper>
  );
});
Breadcrumbs.displayName = 'Breadcrumbs';
