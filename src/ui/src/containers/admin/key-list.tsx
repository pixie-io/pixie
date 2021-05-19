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

import {
  makeStyles, Theme, withStyles,
} from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import ListItemIcon from '@material-ui/core/ListItemIcon';
import ListItemText from '@material-ui/core/ListItemText';
import Menu from '@material-ui/core/Menu';

export const UseKeyListStyles = makeStyles((theme: Theme) => createStyles({
  keyValue: {
    padding: 0,
    fontWeight: theme.typography.fontWeightLight,
    fontSize: '14px',
    color: theme.palette.foreground.one,
    borderWidth: 8,
    borderColor: theme.palette.background.default,
  },
  actionsButton: {
    padding: 0,
  },
  copyBtn: {
    minWidth: '30px',
  },
  error: {
    padding: theme.spacing(1),
  },
}));

export const KeyListItemIcon = withStyles(() => createStyles({
  root: {
    minWidth: 30,
    marginRight: 5,
  },
}))(ListItemIcon);

export const KeyListItemText = withStyles((theme: Theme) => createStyles({
  primary: {
    fontWeight: theme.typography.fontWeightLight,
    fontSize: '14px',
    color: theme.palette.foreground.one,
  },
}))(ListItemText);

export const KeyListMenu = withStyles((theme: Theme) => createStyles({
  paper: {
    borderWidth: 8,
    borderColor: theme.palette.background.default,
  },
}))(Menu);
