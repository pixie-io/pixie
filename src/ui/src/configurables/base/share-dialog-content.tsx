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

import { Link } from 'react-router-dom';

export const ShareDialogContent = React.memo<{ classes: Record<string, string> }>(({ classes }) => (
  <>
    <div className={classes.body}>
      {'Send this link to share this page with other users in your organization.'}
      <br/>
      {'You may invite other users to your organization in the '}
      <Link className={classes.link} to='/admin/users'>Admin View</Link>.
    </div>
    <div className={classes.body}>
      {'If Pixie Cloud has not been exposed to a public domain, the other user must run '}
      <Link
        className={classes.link}
        to='https://docs.px.dev/installing-pixie/install-guides/self-hosted-pixie/#set-up-dns'
        target='_blank'
      >
        these instructions
      </Link>
      {' to configure their DNS settings.'}
    </div>
  </>
));
ShareDialogContent.displayName = 'ShareDialogContent';
