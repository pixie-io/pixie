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

import { LinkItemProps, SideBar } from 'app/containers/App/sidebar';
import { TopBar } from 'app/containers/App/topbar';
import { WithChildren } from 'app/utils/react-boilerplate';

const NavBars = React.memo<WithChildren<{ sidebarButtons?: LinkItemProps[] }>>(({ children, sidebarButtons }) => {
  const [sidebarOpen, setSidebarOpen] = React.useState<boolean>(false);
  const toggleSidebar = React.useCallback(() => setSidebarOpen((open) => !open), [setSidebarOpen]);

  return (
    <>
      <TopBar toggleSidebar={toggleSidebar} sidebarOpen={sidebarOpen} setSidebarOpen={setSidebarOpen}>
        {children}
      </TopBar>
      <SideBar open={sidebarOpen} buttons={sidebarButtons} />
    </>
  );
});
NavBars.displayName = 'NavBars';

export default NavBars;
