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

import { Close as CloseIcon } from '@mui/icons-material';
import { IconButton, Typography } from '@mui/material';
import { Theme, useTheme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { buildClass } from 'app/components';
import { JSONData } from 'app/containers/format-data/json-data';

const useDetailPaneClasses = makeStyles((theme: Theme) => createStyles({
  details: {
    position: 'relative',
    flex: 1,
    minWidth: 0,
    minHeight: 0,
    whiteSpace: 'pre-wrap',
    overflow: 'auto',
    borderLeft: `1px solid ${theme.palette.background.three}`,
  },
  horizontal: {
    borderLeft: 0,
    borderTop: `1px solid ${theme.palette.background.three}`,
  },
  header: {
    display: 'flex',
    flexFlow: 'row nowrap',
    justifyContent: 'space-between',
    alignItems: 'center',
    position: 'sticky',
    top: 0,
    width: '100%',
    height: theme.spacing(5),
    background: theme.palette.background.paper,
    // The parent has a Paper elevation=1, which uses an overlay to change shades. Match it.
    // Only in dark mode; see https://mui.com/components/paper/#elevation toward the bottom.
    backgroundImage: (theme.palette.mode === 'dark'
      ? 'linear-gradient(rgba(255, 255, 255, 0.05), rgba(255, 255, 255, 0.05))'
      : 'none'
    ),
    paddingLeft: theme.spacing(2),
    '& > h4': {
      margin: 0,
      textTransform: 'uppercase',
    },
    '@media not (prefers-reduced-motion: reduce)': {
      transition: 'box-shadow .1s linear',
    },
  },
  pinned: {
    boxShadow: theme.shadows[1],
  },
  pinDetect: {
    position: 'sticky',
    opacity: 0,
    pointerEvents: 'none',
    height: theme.spacing(5),
    width: '1px',
    overflow: 'hidden',
    marginTop: theme.spacing(-5),
  },
  content: {
    padding: theme.spacing(2),
    paddingTop: 0,
  },
}), { name: 'DetailPane' });

interface DetailPaneProps {
  details: Record<string, any> | null;
  closeDetails: () => void;
  splitMode?: 'horizontal' | 'vertical';
}

export const DetailPane = React.memo<DetailPaneProps>(({ details, closeDetails, splitMode = 'vertical' }) => {
  const classes = useDetailPaneClasses();
  const { spacing } = useTheme();

  const [pinned, setPinned] = React.useState(false);

  // Trickery with IntersectionObserver to add a shadow to the sidebar header when needed.
  const root = React.useRef<HTMLDivElement>();
  const detector = React.useRef<HTMLDivElement>();
  const header = React.useRef<HTMLDivElement>();
  React.useEffect(() => {
    if (!(details && root.current && detector.current && header.current)) {
      return () => {};
    }

    const target = detector.current;
    const obs = new IntersectionObserver(
      ([e]) => setPinned(e.intersectionRatio < 1),
      { threshold: 1, root: root.current, rootMargin: spacing(5) },
    );
    obs.observe(target);
    return () => {
      obs.unobserve(target);
      setPinned(false);
    };
  }, [details, spacing]);

  if (!details) return null;
  return (
    <div
      className={buildClass(classes.details, splitMode === 'horizontal' && classes.horizontal)}
      ref={root}
    >
      <div className={classes.pinDetect} ref={detector}></div>
      <div className={buildClass(classes.header, pinned && classes.pinned)} ref={header}>
        <Typography variant='h4'>Details</Typography>
        <IconButton
          aria-label='close details'
          onClick={closeDetails}
        >
          <CloseIcon />
        </IconButton>
      </div>
      <div className={classes.content}>
        <JSONData data={details} multiline />
      </div>
    </div>
  );
});
DetailPane.displayName = 'DetailPane';
