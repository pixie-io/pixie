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

import { createStyles, makeStyles } from '@mui/styles';

import { WidgetDisplay } from 'app/containers/live/vis';

export interface TextChartDisplay extends WidgetDisplay {
  body: string;
}

export interface TextChartProps {
  display: TextChartDisplay;
}

const useStyles = makeStyles(() => createStyles({
  root: {
    flex: 1,
    display: 'flex',
    justifyContent: 'center',
    alignItems: 'center',
  },
  inner: {
    flex: '0 1 auto',
    maxWidth: '100%',
    maxHeight: '100%',
    display: '-webkit-box',
    WebkitLineClamp: 1, // Overridden later
    WebkitBoxOrient: 'vertical',
    overflow: 'hidden',
    overflowWrap: 'anywhere',
    textOverflow: 'ellipsis',
  },
}), { name: 'TextChart' });

export const TextChart = React.memo<TextChartProps>(({
  display: { body },
}) => {
  const classes = useStyles();

  const [outer, setOuter] = React.useState<HTMLDivElement>(null);
  const outerRef = React.useCallback((el: HTMLDivElement) => setOuter(el), [setOuter]);
  const [inner, setInner] = React.useState<HTMLDivElement>(null);
  const innerRef = React.useCallback((el: HTMLDivElement) => setInner(el), [setInner]);

  const [isOverflowed, setIsOverflowed] = React.useState(false);
  const [lineLim, setLineLim] = React.useState(1);
  const updateLimits = React.useCallback(() => {
    if (!inner) return;

    // Before the next calculation, check if the text would overflow its container without line-clamping
    setIsOverflowed(inner.scrollHeight > inner.clientHeight);

    // Figure out what the actual, pixel line height is so we can define `-webkit-line-clamp` correctly.
    const clone = inner.cloneNode() as HTMLDivElement;
    clone.innerHTML = '<br>';
    inner.appendChild(clone);
    const baseHeight = clone.offsetHeight;
    clone.innerHTML = '<br><br>';
    const lineHeight = (clone.offsetHeight - baseHeight) || baseHeight; // To account for margins correctly
    inner.removeChild(clone);

    const { height: available } = outer.getBoundingClientRect();
    setLineLim(Math.floor(available / lineHeight));
  }, [outer, inner]);
  React.useEffect(() => updateLimits(), [updateLimits]);
  React.useEffect(() => {
    window.addEventListener('resize', updateLimits);
    return () => window.removeEventListener('resize', updateLimits);
  }, [updateLimits]);

  const style = React.useMemo(() => ({ WebkitLineClamp: lineLim }), [lineLim]);

  return (
    <div className={classes.root} ref={outerRef} title={isOverflowed ? body : ''}>
      <div className={classes.inner} ref={innerRef} style={style}>{body}</div>
    </div>
  );
});
TextChart.displayName = 'TextChart';
