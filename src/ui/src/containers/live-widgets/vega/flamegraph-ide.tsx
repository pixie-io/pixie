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
  Link,
  ListItem,
  Menu,
  MenuItem,
} from '@mui/material';
import Mustache from 'mustache';

import { ClusterContext } from 'app/common/cluster-context';
import OrgContext from 'app/common/org-context';
import { GQLIDEPath } from 'app/types/schema';

interface FlamegraphConfig {
  name: string;
  path: URL;
}

export const executePathTemplate = ((tmplConfigs: GQLIDEPath[], symbol: string, fullPath: string, cluster: string)
: FlamegraphConfig[] => {
  if (!symbol) {
    return [];
  }

  // Parse symbol.
  const reGolang = /(.*)\.(.*)/;
  const parsedGolang = reGolang.exec(symbol);

  // This was likely not a Go symbol. We can't parse the rest properly, so don't show the menu.
  if (parsedGolang == null || parsedGolang.length !== 3) {
    return [];
  }

  let path = parsedGolang[1];
  if (path.indexOf('.(') !== -1) {
    path = path.slice(0, path.indexOf('.('));
  }
  const fn = parsedGolang[2];

  // Parse pod.
  const rePod = /pod: (.*?);/;
  const parsedPod = rePod.exec(fullPath);
  let pod = '';
  if (parsedPod != null && parsedPod.length > 1) {
    pod = parsedPod[1];
  }

  // Parse namespace.
  const reNs = /namespace: (.*?);/;
  const parsedNs = reNs.exec(fullPath);
  let ns = '';
  if (parsedNs != null && parsedNs.length > 1) {
    ns = parsedNs[1];
  }

  // Fill in the template paths.
  const paths = [];
  tmplConfigs.forEach((tmpl) => {
    try {
      const filledPath = Mustache.render(tmpl.path, {
        function: encodeURIComponent(fn),
        symbol: encodeURIComponent(symbol),
        path: encodeURIComponent(path),
        pod: encodeURIComponent(pod),
        namespace: encodeURIComponent(ns),
        cluster: encodeURIComponent(cluster),
      });
      paths.push({ name: tmpl.IDEName, path: new URL(filledPath) });
      // eslint-disable-next-line no-empty
    } catch {}
  });

  return paths;
});

export interface FlamegraphIDEMenuProps {
  symbol: string;
  fullPath: string;
  open: boolean;
  onClose: (boolean) => void;
}

export const FlamegraphIDEMenu: React.FC<FlamegraphIDEMenuProps> = React.memo(({
  symbol,
  fullPath,
  open,
  onClose,
}) => {
  const { org } = React.useContext(OrgContext);
  const selectedClusterName = React.useContext(ClusterContext)?.selectedClusterName ?? '';

  const paths = executePathTemplate(org.idePaths, symbol, fullPath, selectedClusterName);

  // Gets the position of the current tooltip in Vega and overlays the menu on top.
  const getTooltip = React.useCallback(() => document.querySelector('.vg-tooltip'), []);
  // Don't show the menu if there are no IDE paths/the symbol was invalid.
  if (paths.length === 0) {
    return null;
  }

  return (
    <Menu
      open={open}
      onClose={onClose}
      anchorEl={getTooltip}
    >
    <ListItem
      divider
    >
      Open in IDE
    </ListItem>
    {
      Array.from(paths.map((conf) => (
        <Link target='_blank' key={conf.name} href={conf.path.href}>
          <MenuItem>
            {conf.name}
          </MenuItem>
        </Link>
      )))
    }
    </Menu>
  );
});
FlamegraphIDEMenu.displayName = 'FlamegraphIDEMenu';
