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

import { scrollbarStyles } from 'app/components';

/**
 * Each combination of OS, browser, and system settings can change not only overlay/inlay mode, but dimensions.
 * To guarantee we get the right answer for the default scroll testing element, we need to force a few settings.
 */
const useDefaultStyles = makeStyles((theme: Theme) => createStyles({
  outer: {
    ...scrollbarStyles(theme),
    position: 'absolute',
    top: '-100vh',
    left: '-100vw',
    pointerEvents: 'none',
  },
  inner: {
    width: '100vw',
    height: '100vh',
    overflow: 'scroll',
  },
}), { name: 'ScrollSizeTester' });

const scrollDim = (el: HTMLElement) => ({
  width: el.offsetWidth - el.clientWidth,
  height: el.offsetHeight - el.clientHeight,
});

/**
 * Determines the dimensions of the browser scrollbars on the given element, if it's currently showing them.
 * If no element is provided, determines the default dimensions of the browser scrollbars when they would show.
 * Browsers may overlay scrollbars rather than putting them in-layout. In those cases they're treated as 0-width/height.
 */
export function useScrollbarSize(el?: HTMLElement): { width: number, height: number } {
  const classes = useDefaultStyles();

  const overflow = el && (el.offsetWidth > el.clientWidth || el.offsetHeight > el.clientHeight);

  return React.useMemo(() => {
    if (el) {
      return scrollDim(el);
    }

    const outer = document.createElement('div');
    outer.classList.add(classes.outer);
    const inner = document.createElement('div');
    inner.classList.add(classes.inner);
    inner.appendChild(document.createTextNode('foobarbaz'));
    outer.appendChild(inner);
    document.body.appendChild(outer);

    const dim = scrollDim(inner);
    document.body.removeChild(outer);
    return dim;

    // We also want to check if the element is overflowing - that changes its scrollbar dimensions.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [el, overflow]);
}
