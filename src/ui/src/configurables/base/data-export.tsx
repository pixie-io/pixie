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

import { Button, Typography } from '@mui/material';
import { Link } from 'react-router-dom';

import { GQLDetailedRetentionScript, GQLRetentionScript } from 'app/types/schema';

export const DataExportNoPluginsEnabledSplash = React.memo<{ isEmbedded: boolean }>(({ isEmbedded }) => {
  return (
    isEmbedded ? (
      <Typography variant='body2'>
        To use this page, at least one Pixie plugin supporting long-term data export must be enabled.
        <br />
        If you&apos;re seeing this page embedded in another, something has likely been misconfigured.
      </Typography>
    ) : (
      <>
        <Typography variant='body2'>
          Pixie only guarantees data retention for 24 hours.
          <br />
          {'Configure a '}
          <Link to='/admin/plugins'>plugin</Link>
          {' to export and store Pixie data for longer term retention.'}
          <br />
          This data will be accessible and queryable through the plugin provider.
        </Typography>
        <Button component={Link} to='/admin/plugins' variant='contained'>Configure Plugins</Button>
      </>
    )
  );
});
DataExportNoPluginsEnabledSplash.displayName = 'DataExportNoPluginsEnabledSplash';

/** May be useful if your environment has special rules for permitted retention script names. */
export function allowRetentionScriptName(scriptName: string): boolean {
  return typeof scriptName === 'string'; // But by default, no such rule exists
}

/** May be useful for custom environments. By default, doesn't do anything. */
export function customizeDetailedRetentionScript(script: GQLDetailedRetentionScript): GQLDetailedRetentionScript {
  return script;
}

/** May be useful for custom environments. By default, doesn't do anything. */
export function customizeScriptList(scripts: GQLRetentionScript[]): GQLRetentionScript[] {
  return scripts;
}

/** Like customizeScriptList, environments with special rules for retention scripts may alter them. */
export function customizeRetentionScript<T extends GQLRetentionScript | GQLDetailedRetentionScript>(script: T): T {
  return script;
}
