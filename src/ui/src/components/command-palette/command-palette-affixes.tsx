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

import { Close as CloseIcon } from '@mui/icons-material';
import { Button, IconButton, Tooltip } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { SxProps } from '@mui/system';

import { CommandPaletteContext } from './command-palette-context';

export const CommandPaletteSuffix = React.memo(() => {
  const { inputValue, setInputValue, cta } = React.useContext(CommandPaletteContext);

  const onClear = React.useCallback(() => {
    setInputValue('');
  }, [setInputValue]);

  const sx: SxProps<Theme> = React.useMemo(() => ({
    // Push the edges of the button to the edges of the input, and square its left side
    p: 1,
    mb: 0.25,
    borderRadius: 0,
    minWidth: (t) => t.spacing(10),
    boxShadow: 'none',
  }), []);

  return (
    <>
      {inputValue.length > 0 && (
        <IconButton onClick={onClear}><CloseIcon /></IconButton>
      )}

      <Tooltip title={cta?.tooltip ?? ''}>
        <span> {/* Required for the tooltip to show up when the button is disabled */}
          <Button
            size='small'
            variant='contained'
            disabled={(cta?.disabled ?? true) === true }
            sx={sx}
            onClick={cta?.action ?? (() => {})}
          >
              {cta?.label ?? '...'}
          </Button>
        </span>
      </Tooltip>
    </>
  );
});
CommandPaletteSuffix.displayName = 'CommandPaletteSuffix';
