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

import { useQuery } from '@apollo/client';
import {
  ExpandMore as ExpandMoreIcon,
  Extension as ExtensionIcon,
} from '@mui/icons-material';
import {
  Accordion,
  AccordionDetails,
  AccordionSummary,
  Box,
  FormControlLabel,
  Switch as MaterialSwitch,
  Typography,
} from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import {
  Route, Switch, useRouteMatch, RouteComponentProps,
} from 'react-router-dom';

import { GQLPlugin, GQLPluginKind } from 'app/types/schema';

import { getMockRetentionPlugins, useApolloCacheForMock } from './mock-data';
import { PluginConfig } from './plugin-config';
import { GQL_GET_RETENTION_PLUGINS, usePluginToggleEnabled } from './plugin-gql';

const useStyles = makeStyles(({ palette, spacing, typography }: Theme) => createStyles({
  iconContainer: {
    width: spacing(3),
    height: spacing(3),
    marginRight: spacing(2),
    flexShrink: 0,
  },
  accordionSummaryContent: {
    // Ellipsis styles + flexbox = missed edge case for Material's accordion header; this corrects it.
    maxWidth: `calc(100% - ${spacing(3)})`,
  },
  accordionSummaryTitle: {
    ...typography.body1,
    width: '25%',
    flexShrink: 0,
    whiteSpace: 'nowrap',
    textOverflow: 'ellipsis',
    overflow: 'hidden',
  },
  accordionSummaryStatus: {
    flexShrink: 0,
    marginLeft: spacing(1),
    marginRight: spacing(1),
  },
  accordionSummaryDescription: {
    ...typography.caption,
    flex: '1 1 auto',
    color: palette.text.disabled,
    marginLeft: spacing(1),
    marginRight: spacing(1),
    whiteSpace: 'nowrap',
    textOverflow: 'ellipsis',
    overflow: 'hidden',
  },
  accordionDetailsRoot: {
    maxWidth: '100%',
    overflowX: 'auto',
  },
}), { name: 'PluginList' });

const PluginLogo = React.memo<{ logo?: string }>(({ logo }) => {
  const classes = useStyles();

  const looksValid = logo?.includes('<svg');
  if (looksValid) {
    // Strip newlines and space that isn't required just in case
    const compacted = logo.trim().replace(/\s+/gm, ' ').replace(/\s*([><])\s*/g, '$1');
    // fill="#fff", for instance, isn't safe without encoding the #.
    const dataUrl = `data:image/svg+xml;utf8,${encodeURIComponent(compacted)}`;
    const backgroundImage = `url("${dataUrl}")`;

    return <span className={classes.iconContainer} style={{
      background: backgroundImage ? `center/contain ${backgroundImage} no-repeat` : 'none',
    }} />;
  }

  // eslint-disable-next-line react-memo/require-usememo
  return <ExtensionIcon sx={{ mr: 2 }} />;
});
PluginLogo.displayName = 'PluginLogo';

const PluginHeader = React.memo<{ plugin: GQLPlugin }>(({ plugin }) => {
  const classes = useStyles();

  const [pendingToggle, setPendingToggle] = React.useState(false);
  const pushEnableState = usePluginToggleEnabled(plugin);
  const toggleEnabled = React.useCallback((event: React.MouseEvent<HTMLLabelElement>) => {
    // Don't expand/collapse the accordion, but do toggle the switch.
    event.preventDefault();
    event.stopPropagation();
    setPendingToggle(true);
    pushEnableState(!plugin.retentionEnabled).then(() => {
      setPendingToggle(false);
    }).catch(() => {
      setPendingToggle(false);
    });
  }, [pushEnableState, plugin.retentionEnabled]);

  return (
    <>
      <PluginLogo logo={plugin.logo} />
      <span className={classes.accordionSummaryTitle}>
        {plugin.name}
      </span>
      <span className={classes.accordionSummaryDescription}>
        {plugin.description}
      </span>
      <span className={classes.accordionSummaryStatus}>
        <FormControlLabel
          // TODO(nick,PC-1436): Make the label the same width for both states so things line up; place on right.
          label={`${plugin.retentionEnabled ? 'Enabled' : 'Disabled'}`}
          labelPlacement='start'
          onClick={toggleEnabled}
          // eslint-disable-next-line react-memo/require-usememo
          control={
            <MaterialSwitch size='small' disabled={pendingToggle} checked={plugin.retentionEnabled} />
          }
        />
      </span>
    </>
  );
});
PluginHeader.displayName = 'PluginHeader';

function usePluginList(kind: GQLPluginKind): { loading: boolean, plugins: GQLPlugin[] } {
  if (kind !== GQLPluginKind.PK_RETENTION) {
    throw new Error(`Plugins of kind "${GQLPluginKind[kind]}" aren't supported yet`);
  }

  const apolloCache = useApolloCacheForMock();
  const { data, loading, error } = useQuery<{ plugins: GQLPlugin[] }>(
    GQL_GET_RETENTION_PLUGINS,
    {
      // TODO(nick,PC-1436): Drop this cache manipulation once mocks are no longer needed
      //  Also, this is (correctly) being refetched after saving configs, but mock data clobbers the change.
      onError(err) {
        if (err?.message?.includes('Unimplemented')) {
          console.warn('MOCK DATA! Plugins list is NYI upstream; directly writing to cache with mock data instead.');
          apolloCache.writeQuery({
            query: GQL_GET_RETENTION_PLUGINS,
            data: { plugins: getMockRetentionPlugins().map(p => ({ logo: null, enabledVersion: null, ...p })) },
          });
        }
      },
    },
  );

  // Memo so that the empty array retains its identity until there is an actual change
  const plugins = React.useMemo(() => data?.plugins ?? [], [data?.plugins]);
  return { loading: loading || (!data && !error), plugins };
}

const PluginList = React.memo<RouteComponentProps<{ expandId?: string }>>(({ match, history }) => {
  const classes = useStyles();

  const { loading, plugins } = usePluginList(GQLPluginKind.PK_RETENTION);
  const { expandId } = match.params; // TODO(nick,PC-1436): Scroll to it on page load if feasible?

  const getAccordionToggle = React.useCallback((id: string) => (_: unknown, isExpanded: boolean) => {
    const base = match.url.split('/configure')[0];
    if (isExpanded && (!expandId || expandId !== id)) {
      history.push(`${base}/configure/${id}`);
    } else if (!isExpanded && id === expandId) {
      history.push(base);
    } else {
      throw new Error(`Broken accordion state? ${JSON.stringify({ expandId, id, isExpanded })}`);
    }
  }, [expandId, history, match]);

  if (loading) return <>Loading...</>;
  if (!plugins.length) return <h3>No retention plugins available. This is probably an error.</h3>;
  /* eslint-disable react-memo/require-usememo */
  return (
    <div>
      {plugins.filter(p => p.supportsRetention).map((p) => (
        <Accordion key={p.id} expanded={expandId === p.id} onChange={getAccordionToggle(p.id)}>
          <AccordionSummary
            expandIcon={<ExpandMoreIcon />}
            id={`plugin-accordion-${p.id}-header`}
            aria-controls={`plugin-accordion-${p.id}-content`}
            classes={{ content: classes.accordionSummaryContent }}
          >
            <PluginHeader plugin={p} />
          </AccordionSummary>
          {/* eslint-disable-next-line react-memo/require-usememo */}
          <AccordionDetails classes={{ root: classes.accordionDetailsRoot }}>
            <PluginConfig plugin={p} />
          </AccordionDetails>
        </Accordion>
      ))}
    </div>
  );
  /* eslint-enable react-memo/require-usememo */
});
PluginList.displayName = 'PluginConfig';

export const PluginsOverview = React.memo(() => {
  const { path } = useRouteMatch();

  /* eslint-disable react-memo/require-usememo */
  return (
    <Box sx={{ pl: 2, pr: 2, maxWidth: 'lg', mt: 1, ml: 'auto', mr: 'auto' }}>
      <Typography variant='h3'>Long-term Data Retention</Typography>
      <Typography variant='body1' sx={{ mt: 1, mb: 4 }}>
        Pixie only retains up to 24 hours of data.
        To process longer time spans, plugins can integrate Pixie with long-term data solutions.
      </Typography>
      <Switch>
        <Route exact path={`${path}`} component={PluginList} />
        <Route path={`${path}/configure/:expandId`} component={PluginList} />
      </Switch>
    </Box>
  );
  /* eslint-enable react-memo/require-usememo */
});
PluginsOverview.displayName = 'PluginsOverview';
