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

import React from 'react';

// eslint-disable-next-line no-undef
if (process.env.NODE_ENV === 'development') {
  // eslint-disable-next-line global-require,no-undef
  const whyDidYouRender = require('@welldone-software/why-did-you-render');
  // To use this on a FunctionComponent, attach `whyDidYouRender = true` to it. You may also set `whyDidYouRender` to
  // an object like this one to override the defaults. Details: https://github.com/welldone-software/why-did-you-render
  whyDidYouRender(React, {
    trackAllPureComponents: false,
    collapseGroups: true,
    // The defaults (#058, #00F, and #F00 respectively) have illegible contrast ratios in dev tools and vs each other.
    titleColor: '#21AFA5',
    diffNameColor: '#09F',
    diffPathColor: '#FA8072',
  });
}
