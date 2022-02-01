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

import {
  Help as HelpIcon,
  Campaign as CampaignIcon,
} from '@mui/icons-material';
import {
  Drawer,
  List,
  ListItem,
  ListItemIcon,
  ListItemText,
  Tooltip,
} from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import AnnounceKit from 'announcekit-react';
import { Link } from 'react-router-dom';

import { ClusterContext } from 'app/common/cluster-context';
import UserContext from 'app/common/user-context';
import {
  ClusterIcon, DocsIcon, NamespaceIcon,
} from 'app/components';
import { LiveRouteContext } from 'app/containers/App/live-routing';
import {
  DOMAIN_NAME, ANNOUNCEMENT_ENABLED,
  ANNOUNCE_WIDGET_URL,
} from 'app/containers/constants';
import { deepLinkURLFromScript } from 'app/containers/live-widgets/utils/live-view-params';
import { showIntercomTrigger, triggerID } from 'app/utils/intercom';
import { SidebarFooter } from 'configurable/sidebar-footer';

const useStyles = makeStyles(({
  spacing,
  palette,
  transitions,
  breakpoints,
  zIndex,
}: Theme) => createStyles({
  announcekit: {
    '& .announcekit-widget-badge': {
      position: 'absolute !important',
      top: spacing(2),
      left: spacing(5),
    },
  },
  drawerClose: {
    borderRightWidth: spacing(0.2),
    borderRightStyle: 'solid',
    transition: transitions.create('width', {
      easing: transitions.easing.sharp,
      duration: transitions.duration.leavingScreen,
    }),
    width: spacing(8),
    zIndex: zIndex.appBar - 1,
    overflowX: 'hidden',
    [breakpoints.down('sm')]: {
      display: 'none',
    },
  },
  compactHamburger: {
    display: 'none',
    [breakpoints.down('sm')]: {
      paddingTop: spacing(1),
      display: 'block',
    },
  },
  drawerOpen: {
    borderRightWidth: spacing(0.2),
    borderRightStyle: 'solid',
    width: spacing(29),
    zIndex: zIndex.appBar - 1,
    flexShrink: 0,
    whiteSpace: 'nowrap',
    transition: transitions.create('width', {
      easing: transitions.easing.sharp,
      duration: transitions.duration.enteringScreen,
    }),
    overflowX: 'hidden',
  },
  expandedProfile: {
    flexDirection: 'column',
  },
  listIcon: {
    paddingLeft: spacing(2.5),
    paddingTop: spacing(1),
    paddingBottom: spacing(1),
    '& > div': {
      color: palette.text.primary,
    },
  },
  clippedItem: {
    height: spacing(6),
  },
  sidebarToggle: {
    position: 'absolute',
    width: spacing(6),
    left: 0,
  },
  sidebarToggleSpacer: {
    width: spacing(6),
  },
  spacer: {
    flex: 1,
  },
}), { name: 'SideBar' });

interface LinkItemProps {
  icon: React.ReactNode;
  link: string;
  text: string;
}

const SideBarInternalLinkItem = React.memo<LinkItemProps>(({
  icon, link, text,
}) => {
  const classes = useStyles();
  return (
    <Tooltip title={text} disableInteractive>
      <ListItem button component={Link} to={link} key={text} className={classes.listIcon}>
        <ListItemIcon>{icon}</ListItemIcon>
        <ListItemText primary={text} />
      </ListItem>
    </Tooltip>
  );
});
SideBarInternalLinkItem.displayName = 'SideBarInternalLinkItem';

const SideBarExternalLinkItem = React.memo<LinkItemProps>(({
  icon, link, text,
}) => {
  const classes = useStyles();
  return (
    <Tooltip title={text} disableInteractive>
      <ListItem button component='a' href={link} key={text} className={classes.listIcon} target='_blank'>
        <ListItemIcon>{icon}</ListItemIcon>
        <ListItemText primary={text} />
      </ListItem>
    </Tooltip>
  );
});
SideBarExternalLinkItem.displayName = 'SideBarExternalLinkItem';

export const SideBar: React.FC<{ open: boolean }> = React.memo(({ open }) => {
  const classes = useStyles();
  const selectedClusterName = React.useContext(ClusterContext)?.selectedClusterName ?? '';

  // If we're not in the live view, LiveViewContext is null.
  const embedState = React.useContext(LiveRouteContext)?.embedState ?? null;

  const { user } = React.useContext(UserContext);

  const navItems = React.useMemo(() => {
    if (!selectedClusterName || !embedState) {
      return [];
    }
    return [{
      icon: <ClusterIcon />,
      link: deepLinkURLFromScript('px/cluster', selectedClusterName, embedState, {}),
      text: 'Cluster',
    },
    {
      icon: <NamespaceIcon />,
      link: deepLinkURLFromScript('px/namespaces', selectedClusterName, embedState, {}),
      text: 'Namespaces',
    }];
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [selectedClusterName, embedState]);

  const drawerClasses = React.useMemo(
    () => ({ paper: open ? classes.drawerOpen : classes.drawerClose }),
    [classes.drawerClose, classes.drawerOpen, open]);

  const announceUser = React.useMemo(() => ({
    id: user.email,
    email: user.email,
  }), [user.email]);
  const announceData = React.useMemo(() => ({ org: user.orgName }), [user.orgName]);

  return (
    <>
      <Drawer
        variant='permanent'
        className={open ? classes.drawerOpen : classes.drawerClose}
        classes={drawerClasses}
      >
        <List>
          <ListItem button className={classes.clippedItem} />
        </List>
        <List>
          {navItems.map(({ icon, link, text }) => (
            <SideBarInternalLinkItem key={text} icon={icon} link={link} text={text} />
          ))}
        </List>
        <div className={classes.spacer} />
        <List>
          <Tooltip title='Announcements' disableInteractive>
            <div className={classes.announcekit}>
              {
                ANNOUNCEMENT_ENABLED && (
                <AnnounceKit widget={ANNOUNCE_WIDGET_URL} user={announceUser} data={announceData}>
                  <ListItem button key='announcements' className={classes.listIcon}>
                    <ListItemIcon><CampaignIcon /></ListItemIcon>
                    <ListItemText primary='Announcements' />
                  </ListItem>
                </AnnounceKit>
                )
              }
            </div>
          </Tooltip>
          <SideBarExternalLinkItem
            key='Docs'
            icon={React.useMemo(() => <DocsIcon />, [])}
            link={`https://docs.${DOMAIN_NAME}`}
            text='Docs'
          />
          {showIntercomTrigger() && (
            <Tooltip title='Help' disableInteractive>
              <ListItem button id={triggerID} className={classes.listIcon}>
                <ListItemIcon><HelpIcon /></ListItemIcon>
                <ListItemText primary='Help' />
              </ListItem>
            </Tooltip>
          )}
          <SidebarFooter clusterName={selectedClusterName} />
        </List>
      </Drawer>
    </>
  );
});
SideBar.displayName = 'SideBar';
