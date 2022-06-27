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
  ExpandMore as ExpandMoreIcon,
  Extension as ExtensionIcon,
} from '@mui/icons-material';
import {
  Accordion,
  AccordionDetails,
  AccordionSummary,
  Box,
  Divider,
  FormControlLabel,
  Switch as MaterialSwitch,
  Tooltip,
  Typography,
} from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import {
  Link, Route, Switch, useRouteMatch, RouteComponentProps,
} from 'react-router-dom';

import { useSnackbar } from 'app/components';
import { useRetentionScripts } from 'app/pages/configure-data-export/data-export-gql';
import { GQLPlugin, GQLPluginKind } from 'app/types/schema';
import pixieAnalytics from 'app/utils/analytics';
import { WithChildren } from 'app/utils/react-boilerplate';

import { PluginConfig } from './plugin-config';
import { usePluginList, usePluginToggleEnabled } from './plugin-gql';

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
  toggleWarningTooltip: {
    '& ul': { paddingLeft: spacing(1) },
  },
}), { name: 'PluginList' });

// Not viable as a general solution, but works for this specific layout where the parent element controls the ellipsis
// and is a flex item. If we want this elsewhere in the app we'll need a more general approach for flex items.
const OverflowTooltip: React.FC<WithChildren<{ title: string }>> = React.memo(({ title, children }) => {
  const [overflow, setOverflow] = React.useState(false);
  const [span, setSpan] = React.useState<HTMLSpanElement>(null);
  const spanRef = React.useCallback((el) => setSpan(el), []);

  const updateOverflow = React.useCallback(() => {
    setOverflow(span?.parentElement.scrollWidth > span?.parentElement.clientWidth);
  }, [span]);

  return (
    <Tooltip
      placement='bottom-start'
      enterDelay={200}
      title={overflow ? (<div style={{ whiteSpace: 'pre-wrap' }}>{title}</div>) : ''}
      onMouseEnter={updateOverflow}
      onFocus={updateOverflow}
      onTouchStart={updateOverflow}
    >
      <span ref={spanRef}>
        {children}
      </span>
    </Tooltip>
  );
});
OverflowTooltip.displayName = 'OverflowTooltip';

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

const ToggleWarningTooltip = React.memo<{ mode: 'enable' | 'disable', numPreset: number, numCustom: number }>(({
  mode, numPreset, numCustom,
}) => {
  const classes = useStyles();

  const enableWarning = React.useMemo(() => (
    <div className={classes.toggleWarningTooltip}>
      <p><strong>Enabling</strong> this plugin will:</p>
      <ul>
        <li>Use its default configuration</li>
        <li>
          Enable its <strong>preset</strong> retention scripts for all clusters.
          <br/>
          You can&nbsp;
          {/* eslint-disable-next-line react-memo/require-usememo */}
          <Link to='/configure-data-export' onClick={(e) => e.stopPropagation()}>
            adjust this
          </Link>
          &nbsp;at any time after enabling.
        </li>
      </ul>
    </div>
  ), [classes.toggleWarningTooltip]);

  const disableWarning = React.useMemo(() => (
    <div className={classes.toggleWarningTooltip}>
      <p><strong>Disabling</strong> this plugin will:</p>
      <ul>
        <li>Clear this plugin&apos;s configuration</li>
        {numPreset > 0 && (
          <li>
            Disable&nbsp;
            {/* eslint-disable-next-line react-memo/require-usememo */}
            <Link to='/configure-data-export' onClick={(e) => e.stopPropagation()}>
              {numPreset} <strong>preset</strong> retention script{numPreset > 1 ? 's' : ''}
            </Link>
          </li>
        )}
        {numCustom > 0 && (
          <li>
            Disable&nbsp;
            {/* eslint-disable-next-line react-memo/require-usememo */}
            <Link to='/configure-data-export' onClick={(e) => e.stopPropagation()}>
              {numCustom} <strong>custom</strong> retention script{numCustom > 1 ? 's' : ''}
            </Link>
          </li>
        )}
      </ul>
    </div>
  ), [classes.toggleWarningTooltip, numCustom, numPreset]);

  return mode === 'enable' ? enableWarning : disableWarning;
});
ToggleWarningTooltip.displayName = 'ToggleWarningTooltip';

interface PluginHeaderProps {
  plugin: GQLPlugin;
  history: RouteComponentProps.history;
  onToggle: (isEnabled: boolean) => void;
  numPreset: number;
  numCustom: number;
}
const PluginHeader = React.memo<PluginHeaderProps>(({ plugin, history, onToggle, numPreset, numCustom }) => {
  const classes = useStyles();
  const showSnackbar = useSnackbar();

  const [pendingToggle, setPendingToggle] = React.useState(false);
  const pushEnableState = usePluginToggleEnabled(plugin);
  const toggleEnabled = React.useCallback((event: React.MouseEvent<HTMLLabelElement>) => {
    const enabled = !plugin.retentionEnabled;
    // Don't expand/collapse the accordion, but do toggle the switch.
    event.preventDefault();
    event.stopPropagation();
    setPendingToggle(true);
    pixieAnalytics.track('Retention plugin toggled', {
      enabled,
      plugin: plugin.id,
    });
    pushEnableState(enabled).then(() => {
      setPendingToggle(false);
      onToggle(enabled);
      const message = `${enabled ? 'Enabled' : 'Disabled'} plugin "${plugin.id}" and its retention scripts`;
      showSnackbar({
        message,
        actionTitle: enabled ? 'Configure Scripts' : undefined,
        action: enabled ? (() => history.push('/configure-data-export')) : undefined,
      });
    }).catch(() => {
      setPendingToggle(false);
    });
  }, [pushEnableState, onToggle, plugin.retentionEnabled, plugin.id, history, showSnackbar]);

  const tooltip = React.useMemo(() => (
    <ToggleWarningTooltip
      mode={plugin.retentionEnabled ? 'disable' : 'enable'}
      numPreset={numPreset}
      numCustom={numCustom}
    />
  ), [numCustom, numPreset, plugin.retentionEnabled]);

  return (
    <>
      <PluginLogo logo={plugin.logo} />
      <span className={classes.accordionSummaryTitle}>
        {plugin.name}
      </span>
      <span className={classes.accordionSummaryDescription}>
        <OverflowTooltip title={plugin.description}>
          <>{plugin.description}</>
        </OverflowTooltip>
      </span>
      <span className={classes.accordionSummaryStatus}>
        <Tooltip arrow title={tooltip}>
          <FormControlLabel
            // TODO(nick,PC-1436): Make the label the same width for both states so things line up; place on right.
            label={`${plugin.retentionEnabled ? 'Enabled' : 'Disabled'}`}
            labelPlacement='start'
            onClick={toggleEnabled}
            // eslint-disable-next-line react-memo/require-usememo
            control={
              <MaterialSwitch size='small' disabled={pendingToggle} checked={!!plugin.retentionEnabled} />
            }
          />
        </Tooltip>
      </span>
    </>
  );
});
PluginHeader.displayName = 'PluginHeader';

const PluginList = React.memo<RouteComponentProps<{ expandId?: string }>>(({ match, history }) => {
  const classes = useStyles();
  const showSnackbar = useSnackbar();

  const { scripts } = useRetentionScripts();
  const { loading, error, plugins } = usePluginList(GQLPluginKind.PK_RETENTION);
  const { expandId } = match.params; // TODO(nick,PC-1436): Scroll to it on page load if feasible?

  const getAccordionToggle = React.useCallback((id: string) => (_: unknown, expand: boolean) => {
    const base = match.url.split('/configure')[0];
    if (expand && (!expandId || expandId !== id)) {
      history.push(`${base}/configure/${id}`.replace('//', '/'));
    } else if (!expand && id === expandId) {
      history.push(base);
    }
  }, [expandId, history, match]);

  const getOnPluginToggled = React.useCallback((id: string) => (isEnabled: boolean) => {
    const toggleAccordion = getAccordionToggle(id);
    toggleAccordion(undefined, isEnabled);
  }, [getAccordionToggle]);

  React.useEffect(() => {
    if (error && !loading) {
      showSnackbar({ message: `Error loading plugin list: ${error.message}` });
      console.error('Error loading plugin list', error);
    }
  }, [loading, error, showSnackbar]);

  if (loading && !plugins.length) return <>Loading...</>;
  if (!plugins.length) return <h3>No retention plugins available. This is probably an error.</h3>;
  /* eslint-disable react-memo/require-usememo */
  return (
    <div>
      {plugins.filter(p => p.supportsRetention).map((p) => (
        <Accordion
          key={p.id}
          expanded={expandId === p.id}
          onChange={getAccordionToggle(p.id)}
        >
          <AccordionSummary
            expandIcon={<ExpandMoreIcon />}
            id={`plugin-accordion-${p.id}-header`}
            aria-controls={`plugin-accordion-${p.id}-content`}
            classes={{ content: classes.accordionSummaryContent }}
          >
            <PluginHeader
              plugin={p}
              history={history}
              onToggle={getOnPluginToggled(p.id)}
              numPreset={scripts?.filter(s => s.pluginID === p.id && s.isPreset).length ?? 0}
              numCustom={scripts?.filter(s => s.pluginID === p.id && !s.isPreset).length ?? 0}
            />
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
        <br/>
        {'Go to '}
        <Link to='/configure-data-export' onClick={(e) => e.stopPropagation()}>
          Long-term Data Export
        </Link>
        {' to configure what data is exported.'}
      </Typography>
      <Switch>
        <Route exact path={`${path}`} component={PluginList} />
        <Route path={`${path}/configure/:expandId`} component={PluginList} />
      </Switch>
      <Divider variant='middle' sx={{ mt: 4, mb: 4 }} />
      <Typography variant='h3'>Alerting</Typography>
      <Typography variant='body1' sx={{ mt: 1, mb: 4 }}>
        Coming soon: configure alerts using Pixie data to monitor your cluster and applications.
      </Typography>
    </Box>
  );
  /* eslint-enable react-memo/require-usememo */
});
PluginsOverview.displayName = 'PluginsOverview';
