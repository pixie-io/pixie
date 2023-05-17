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
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { AutocompleteContext } from 'app/components/autocomplete/autocomplete-context';
import { DialogDropdown, BreadcrumbListItem } from 'app/components/breadcrumbs/breadcrumbs';
import { SetStateFunc } from 'app/context/common';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    color: theme.palette.text.secondary,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    cursor: 'pointer',
  },
  selector: {
    display: 'flex',
    alignItems: 'center',
    marginLeft: theme.spacing(-0.1),
  },
}), { name: 'Select' });

export interface SelectProps {
  value: React.ReactNode;
  getListItems?: (input: string) => Promise<{ items: BreadcrumbListItem[], hasMoreItems: boolean }>;
  onSelect?: (input: string) => void;
  setOpen?: SetStateFunc<boolean>;
  requireCompletion?: boolean;
  placeholder?: string;
}

export const Select: React.FC<SelectProps> = React.memo(({
  value, getListItems, onSelect, setOpen, requireCompletion, placeholder,
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
  }, [setAnchorEl]);

  React.useEffect(() => {
    setOpen?.(!!anchorEl);
  }, [setOpen, anchorEl]);

  // In case a breadcrumb doesn't override, give it the nearest context's values.
  return (
    <AutocompleteContext.Provider
      value={{
        allowTyping: true,
        requireCompletion,
        onOpen: 'clear',
      }}
    >
      <div className={classes.root} onClick={handleClick}>
        { value }
        <div className={classes.selector}><ArrowDropDownIcon /></div>
      </div>
      <DialogDropdown
        placeholder={placeholder || 'Filter...'}
        onSelect={onSelect}
        onClose={onClose}
        getListItems={getListItems}
        anchorEl={anchorEl}
      />
    </AutocompleteContext.Provider>
  );
});
Select.displayName = 'Select';
