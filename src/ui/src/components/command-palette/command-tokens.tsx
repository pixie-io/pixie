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

import { buildClass } from 'app/components';

import { CommandPaletteContext } from './command-palette-context';
import { Token, TokenType } from './parser';

// eslint-disable-next-line @typescript-eslint/ban-types
const useTokenStyles = makeStyles(({ palette: p }: Theme) => createStyles<TokenType | 'token' | 'underline', {}>({
  token: { whiteSpace: 'pre' },
  none: { color: p.syntax.normal },
  icon: { color: p.success.main },
  key: { color: p.primary.dark },
  eq: { color: p.syntax.divider },
  value: { color: p.syntax.normal },
  typeahead: { color: p.text.disabled },
  error: { color: p.syntax.error },
  underline: {
    borderBottom: `1px ${p.text.disabled} dotted`,
  },
}), { name: 'CommandInputToken' });

export const CommandInputToken = React.memo<{ token: Token }>(({ token }) => {
  const classes = useTokenStyles();
  const { open, selectedTokens } = React.useContext(CommandPaletteContext);

  const isSelected = selectedTokens.some((other) => other.token === token);
  const underline = open && !['none', 'eq'].includes(token.type) && isSelected;

  const className = buildClass(classes.token, classes[token.type], underline && classes.underline);

  return <span className={className}>{token.text}</span>;
});
CommandInputToken.displayName = 'CommandInputToken';
