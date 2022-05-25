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

import { Close as CloseButton } from '@mui/icons-material';
import { Dialog, IconButton, Tooltip } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import 'typeface-walter-turncoat';

import { CONTACT_ENABLED } from 'app/containers/constants';
import { SetStateFunc } from 'app/context/common';
import { WithChildren } from 'app/utils/react-boilerplate';

export interface LiveTourContextProps {
  tourOpen: boolean;
  setTourOpen: SetStateFunc<boolean>;
}
export const LiveTourContext = React.createContext<LiveTourContextProps>({
  tourOpen: false,
  setTourOpen: () => {},
});
LiveTourContext.displayName = 'LiveTourContext';

export const LiveTourContextProvider: React.FC<WithChildren> = React.memo(({ children }) => {
  const [tourOpen, setTourOpen] = React.useState<boolean>(false);
  return <LiveTourContext.Provider value={{ tourOpen, setTourOpen }}>{children}</LiveTourContext.Provider>;
});
LiveTourContextProvider.displayName = 'LiveTourContextProvider';

/**
 * Generates the CSS properties needed to punch holes in the backdrop
 * where the described components are, to make them easy to notice
 */
function buildBackdropMask(s: Theme['spacing']) {
  const commonMaskShape = ''
      + '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" preserveAspectRatio="none">'
      + '<rect width="100" height="100" fill="black"></rect>'
      + '</svg>';

  // x, y, w, h.
  const punchouts: string[][] = [
    /* eslint-disable no-multi-spaces */
    [s(8),         'top',                   '100vw', s(16)],   // Top bars (both of them)
    ['left',       s(8),                    s(8),    '100vh'], // Sidebar
    ['calc(50vw)', `calc(100vh - ${s(3)})`, s(8),    s(3)],    // Drawer button
    /* eslint-enable no-multi-spaces */
  ];
  // Draws white everywhere except the punchout locations. Without this, the
  // exclude composite mode doesn't know what to do and applies no mask at all.
  const outside = 'linear-gradient(#fff,#fff)';

  return {
    maskImage: Array(punchouts.length)
      .fill(`url('data:image/svg+xml;utf8,${commonMaskShape}')`)
      .concat(outside)
      .join(','),
    maskPosition: punchouts.map((pos) => `${pos[0]} ${pos[1]}`).concat('0 0').join(','),
    maskSize: punchouts.map((pos) => `${pos[2]} ${pos[3]}`).concat('100% 100%').join(','),
    // Makes the mask draw only what ISN'T covered in black, rather than the opposite.
    maskComposite: 'exclude',
    maskRepeat: 'no-repeat',
  };
}

const useStyles = makeStyles((theme: Theme) => createStyles({
  tourModal: {
    // Dialog already uses a shaded background; we don't need another solid one in front of it.
    background: 'transparent',
  },
  tourModalBackdrop: {
    // Said shaded background. Punches a few holes to show the elements the modal points to.
    position: 'absolute',
    top: 0,
    left: 0,
    width: '100vw',
    height: '100vh',
    background: 'rgba(32, 32, 32, 0.8)',
    ...buildBackdropMask(theme.spacing),
  },
  closeButton: {
    position: 'absolute',
    top: theme.spacing(1),
    left: theme.spacing(1),
    zIndex: 1,
    background: 'black',
    color: 'white',
    border: '1px rgba(255, 255, 255, 0.2) solid',
  },
  content: {
    width: '100%',
    height: '100%',
    position: 'relative',

    ...theme.typography.body1,
    color: 'white',
    fontFamily: '"Walter Turncoat", cursive',

    '& h3': {
      font: theme.typography.h3.font,
      color: theme.palette.primary.main,
      marginTop: 0,
      marginBottom: theme.spacing(1),
    },

    '& p': {
      margin: 0,
    },

    '& > *': {
      position: 'absolute',
      margin: 0,
    },
  },
  topBarLeft: {
    top: theme.spacing(16),
    left: theme.spacing(52),
  },
  topBarLeftArrow: {
    position: 'absolute',
    top: theme.spacing(3),
    left: 0,
    transform: 'translate(-100%, -100%)',
  },
  topBarRight: {
    top: theme.spacing(16),
    right: theme.spacing(24),
    textAlign: 'right',
  },
  topBarRightArrow: {
    position: 'absolute',
    top: theme.spacing(3),
    right: 0,
    transform: 'translate(100%, -100%)',
  },
  upperSideBar: {
    top: theme.spacing(16),
    left: theme.spacing(16),
  },
  upperSidebarArrow: {
    position: 'absolute',
    top: theme.spacing(3),
    left: 0,
    transform: 'translate(-100%, -100%)',
  },
  lowerSideBar: {
    bottom: theme.spacing(12),
    left: theme.spacing(16),
  },
  lowerSideBarArrow: {
    position: 'absolute',
    top: theme.spacing(2.5),
    left: 0,
    transform: 'translate(-100%, -100%)',
  },
  dataDrawer: {
    bottom: theme.spacing(12),
    left: `calc(50% + ${theme.spacing(4)})`, // Center of the main area rather than of the viewport
    transform: 'translate(-50%, 0)',
    textAlign: 'center',
  },
  dataDrawerArrow: {
    position: 'absolute',
    left: '50%',
    bottom: theme.spacing(0.5),
    transform: 'translate(-50%, 100%)',
  },
}), { name: 'LiveTour' });

interface LiveTourArrowProps {
  width: number;
  height: number;
  tipX: number;
  tipY: number;
  // The tip is drawn pointing northwest.
  tipAngleDeg: number;
  path: string;
  pointLength?: number;
  className: string;
}

const LiveTourArrow = React.memo<LiveTourArrowProps>(({
  path,
  width,
  height,
  tipX,
  tipY,
  tipAngleDeg,
  pointLength = 10,
  className,
}) => {
  const tipPath = `M ${tipX + pointLength} ${tipY} h -${pointLength} v ${pointLength}`;
  return (
    <svg width={width} height={height} className={className} xmlns='http://www.w3.org/2000/svg'>
      <path d={path} stroke='white' fill='transparent' strokeWidth={2} strokeLinecap='round' />
      <path
        d={tipPath}
        transform={`rotate(${tipAngleDeg},${tipX},${tipY})`}
        stroke='white'
        fill='transparent'
        strokeWidth={2}
        strokeLinecap='round'
      />
    </svg>
  );
});
LiveTourArrow.displayName = 'LiveTourArrow';

/**
 * Contents of the tour for Live pages. Meant to exist within a fullscreen MUIDialog.
 * @constructor
 */
export const LiveTour = React.memo(() => {
  const classes = useStyles();
  // TODO(nick): On mobile, the layout is entirely different (and as of this writing, core functionality is unavailable)
  //  For now, this is hidden for those users (the component to open this hides).
  return (
    <div className={classes.content}>
      <div className={classes.upperSideBar}>
        <LiveTourArrow
          path='M 10 10 Q 40 10, 40 30 T 70 50'
          tipX={10}
          tipY={10}
          tipAngleDeg={-45}
          pointLength={5}
          className={classes.upperSidebarArrow}
          width={80}
          height={60}
        />
        <h3>Navigation</h3>
        <p>Navigate to K8s objects.</p>
      </div>
      <div className={classes.topBarLeft}>
        <LiveTourArrow
          path='M 80 80 Q 10 80, 10 10'
          tipX={10}
          tipY={10}
          tipAngleDeg={45}
          className={classes.topBarLeftArrow}
          width={90}
          height={90}
        />
        <h3>Select cluster, script, & optional parameters</h3>
        <p>Display your cluster&apos;s data by running PxL scripts.</p>
      </div>
      <div className={classes.topBarRight}>
        <LiveTourArrow
          path='M 10 80 Q 80 80, 80 10'
          tipX={80}
          tipY={10}
          tipAngleDeg={45}
          className={classes.topBarRightArrow}
          width={90}
          height={90}
        />
        <h3>Run & Edit Scripts</h3>
        <p>Run with Ctrl/Cmd+Enter.</p>
        <p>Edit with Ctrl/Cmd+E.</p>
      </div>
      <div className={classes.lowerSideBar}>
        <LiveTourArrow
          path='M 5 5 L 65 5'
          tipX={5}
          tipY={5}
          tipAngleDeg={-45}
          className={classes.lowerSideBarArrow}
          width={75}
          height={10}
        />
        <h3>Pixie Info / Settings</h3>
        <div>Docs</div>
        { CONTACT_ENABLED && <div>Help</div>}
      </div>
      <div className={classes.dataDrawer}>
        <LiveTourArrow
          path='M 10 10 L 10 80'
          tipX={10}
          tipY={75}
          tipAngleDeg={225}
          className={classes.dataDrawerArrow}
          width={20}
          height={75}
        />
        <h3>Data Drawer</h3>
        <p>Inspect the raw data (Ctrl/Cmd+D).</p>
      </div>
    </div>
  );
});
LiveTour.displayName = 'LiveTour';

const LiveTourBackdrop = React.memo(() => <div className={useStyles().tourModalBackdrop} />);
LiveTourBackdrop.displayName = 'LiveTourBackdrop';

interface LiveTourDialogProps {
  onClose: () => void;
}

export const LiveTourDialog = React.memo<LiveTourDialogProps>(({ onClose }) => {
  const classes = useStyles();
  const { tourOpen } = React.useContext(LiveTourContext);
  return (
    <Dialog
      open={tourOpen}
      onClose={onClose}
      // eslint-disable-next-line react-memo/require-usememo
      classes={{
        paperFullScreen: classes.tourModal,
      }}
      BackdropComponent={LiveTourBackdrop}
      fullScreen
    >
      <Tooltip title='Close (Esc)'>
        <IconButton onClick={onClose} className={classes.closeButton}>
          <CloseButton />
        </IconButton>
      </Tooltip>
      <LiveTour />
    </Dialog>
  );
});
LiveTourDialog.displayName = 'LiveTourDialog';
