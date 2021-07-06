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
} from '@material-ui/core';
import {
  WithStyles,
  withStyles,
} from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import ArrowDropDownIcon from '@material-ui/icons/ArrowDropDown';
import { DialogDropdown, BreadcrumbListItem } from 'app/components/breadcrumbs/breadcrumbs';
import { AutocompleteContext } from 'app/components/autocomplete/autocomplete-context';

const styles = (theme: Theme) => createStyles({
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
});

export interface SelectProps extends WithStyles<typeof styles> {
  value: string;
  getListItems?: (input: string) => Promise<BreadcrumbListItem[]>;
  onSelect?: (input: string) => void;
  requireCompletion?: boolean;
  placeholder?: string;
}

const SelectImpl = ({
  classes, value, getListItems, onSelect, requireCompletion, placeholder,
}: SelectProps) => {
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
};

export const Select = withStyles(styles)(SelectImpl);
