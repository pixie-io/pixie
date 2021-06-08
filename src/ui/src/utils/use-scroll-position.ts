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

type Positions = { scrollLeft: number; scrollTop: number; };

/** A rate-limited way to watch scrollLeft and scrollTop on an element. Use this to avoid excessive handler calls. */
export function useScrollPosition(el: Element): Positions {
  const [waiting, setWaiting] = React.useState(false);
  const [positions, setPositions] = React.useState<Positions>({
    scrollLeft: el?.scrollLeft ?? 0,
    scrollTop: el?.scrollTop ?? 0,
  });

  const handler = React.useMemo(() => () => {
    if (el && !waiting) {
      setWaiting(true);
      requestAnimationFrame(() => {
        setPositions({ scrollLeft: el.scrollLeft, scrollTop: el.scrollTop });
        setWaiting(false);
      });
    }

    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [el]);

  React.useEffect(() => {
    handler();
    el?.addEventListener('scroll', handler);
    return () => el?.removeEventListener('scroll', handler);
  }, [el, handler]);

  return positions;
}
