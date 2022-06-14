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

import { Extension as ExtensionIcon } from '@mui/icons-material';
import { Box } from '@mui/material';

export const PluginIcon = React.memo<{ iconString: string }>(({ iconString }) => {
  const looksValid = iconString?.includes('<svg');
  if (looksValid) {
    // Strip newlines and space that isn't required just in case
    const compacted = iconString.trim().replace(/\s+/gm, ' ').replace(/\s*([><])\s*/g, '$1');
    // fill="#fff", for instance, isn't safe without encoding the #.
    const dataUrl = `data:image/svg+xml;utf8,${encodeURIComponent(compacted)}`;
    const backgroundImage = `url("${dataUrl}")`;

    // eslint-disable-next-line react-memo/require-usememo
    return <Box sx={({ spacing }) => ({
      width: spacing(2.5),
      height: spacing(2.5),
      marginRight: spacing(1.5),
      background: backgroundImage ? `center/contain ${backgroundImage} no-repeat` : 'none',
    })} />;
  }

  // eslint-disable-next-line react-memo/require-usememo
  return <ExtensionIcon sx={{ mr: 2, fontSize: 'body1.fontSize' }} />;
});
PluginIcon.displayName = 'PluginIcon';
