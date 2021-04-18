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

import Button from '@material-ui/core/Button';
import { FixedSizeDrawer } from './drawer';

export default {
  title: 'Drawer/Fixed',
  component: FixedSizeDrawer,
};

export const Basic = () => {
  const [open, setOpen] = React.useState(false);
  const toggleOpen = React.useCallback(() => setOpen((opened) => !opened), []);

  const otherContent = (
    <div>
      <Button onClick={toggleOpen} color='primary'>
        {open ? 'Close' : 'Open'}
      </Button>
      <div>Other content. Some text goes here.</div>
    </div>
  );

  return (
    <FixedSizeDrawer
      drawerDirection='left'
      drawerSize='250px'
      open={open}
      otherContent={otherContent}
      overlay={false}
    >
      <div>Drawer contents</div>
    </FixedSizeDrawer>
  );
};
