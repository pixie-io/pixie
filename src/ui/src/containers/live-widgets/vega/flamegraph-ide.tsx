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

import Link from '@material-ui/core/Link';
import ListItem from '@material-ui/core/ListItem';
import Menu from '@material-ui/core/Menu';
import MenuItem from '@material-ui/core/MenuItem';
import OrgContext from 'app/common/org-context';
import { GQLIDEPath } from 'app/types/schema';
import Mustache from 'mustache';
import * as React from 'react';

interface FlamegraphConfig {
  name: string;
  path: string;
}

export const executePathTemplate = ((tmplConfigs: GQLIDEPath[], symbol: string, fullPath: string)
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
  // Fill in the template paths.
  const paths = tmplConfigs.map((tmpl) => (
    {
      name: tmpl.IDEName,
      path: Mustache.render(tmpl.path, {
        function: encodeURIComponent(fn),
        symbol: encodeURIComponent(symbol),
        path: encodeURIComponent(path),
        pod: encodeURIComponent(pod),
      }),
    }),
  );

  return paths;
});

export interface FlamegraphIDEMenuProps {
  symbol: string;
  fullPath: string;
  open: boolean;
  onClose: (boolean) => void;
}

export const FlamegraphIDEMenu: React.FC<FlamegraphIDEMenuProps> = React.memo(function FlamegraphIDEMenu({
  symbol,
  fullPath,
  open,
  onClose,
}) {
  const { org } = React.useContext(OrgContext);

  const paths = executePathTemplate(org.idePaths, symbol, fullPath);

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
        <Link key={conf.name} href={conf.path}>
          <MenuItem>
            {conf.name}
          </MenuItem>
        </Link>
      )))
    }
    </Menu>
  );
});
