import * as React from 'react';

import {
  createStyles, Theme, withStyles,
} from '@material-ui/core/styles';
import { useQuery, useApolloClient } from '@apollo/client';

import {
  Breadcrumbs, BreadcrumbOptions,
  PixieCommandIcon, StatusCell,
} from '@pixie/components';
import { ClusterContext } from 'common/cluster-context';
import { CLUSTER_STATUS_DISCONNECTED } from 'common/vizier-grpc-client-context';
import {
  getArgTypesForVis, getArgVariableMap,
} from 'utils/args-utils';
import { SCRATCH_SCRIPT, ScriptsContext } from 'containers/App/scripts-context';
import { ScriptContext } from 'context/script-context';
import { entityPageForScriptId, optionallyGetNamespace } from 'containers/live-widgets/utils/live-view-params';
import { EntityType, pxTypetoEntityType, entityStatusGroup } from 'containers/command-input/autocomplete-utils';
import { clusterStatusGroup } from 'containers/admin/utils';
import { containsMutation, CLUSTER_QUERIES, AUTOCOMPLETE_QUERIES } from '@pixie/api';
import ExecuteScriptButton from './execute-button';
import { Variable } from './vis';

const styles = (({ shape, palette, spacing }: Theme) => createStyles({
  root: {
    display: 'flex',
    alignItems: 'center',
    marginTop: spacing(1),
    marginRight: spacing(3.5),
    marginLeft: spacing(3),
    marginBottom: spacing(1),
    background: palette.background.three,
    // This adds a scroll to the breadcrumbs on overflow,
    // but it's hard for the user to know it exists. Perhaps we can
    // consider adding a scroll effect or something to make it easier to
    // discover.
    borderRadius: shape.borderRadius,
    boxShadow: '4px 4px 4px rgba(0, 0, 0, 0.25)',
    border: palette.border.unFocused,
  },
  spacer: {
    flex: 1,
  },
  verticalLine: {
    borderLeft: `2px solid ${palette.foreground.grey1}`,
    height: spacing(2.7),
    padding: 0,
  },
  pixieIcon: {
    color: palette.primary.main,
    marginRight: spacing(1),
    marginLeft: spacing(0.5),
  },
  iconContainer: {
    padding: 0,
  },
  breadcrumbs: {
    width: '100%',
    overflow: 'hidden',
    display: 'flex',
  },
}));

const LiveViewBreadcrumbs = ({ classes }) => {
  const { loading, data } = useQuery(CLUSTER_QUERIES.LIST_CLUSTERS);
  const { selectedCluster, setCluster, selectedClusterUID } = React.useContext(ClusterContext);
  const { scripts } = React.useContext(ScriptsContext);

  const {
    vis, pxl, args, id, liveViewPage, setArgs, execute, setScript, parseVisOrShowError, argsForVisOrShowError,
  } = React.useContext(ScriptContext);

  const client = useApolloClient();
  const getCompletions = React.useCallback((newInput: string, kind: EntityType) => (client.query({
    query: AUTOCOMPLETE_QUERIES.FIELD,
    fetchPolicy: 'network-only',
    variables: {
      input: newInput,
      kind,
      clusterUID: selectedClusterUID,
    },
  })
  ), [client, selectedClusterUID]);

  const scriptIds = React.useMemo(() => [...scripts.keys()], [scripts]);
  // eslint-disable-next-line react-hooks/exhaustive-deps
  const clusterIds = React.useMemo(() => data?.clusters.map((c) => c.id), [data?.clusters]);

  type MemoCrumbs = { entityBreadcrumbs: BreadcrumbOptions[]; argBreadcrumbs: BreadcrumbOptions[] };
  const { entityBreadcrumbs, argBreadcrumbs }: MemoCrumbs = React.useMemo(() => {
    // eslint-disable-next-line no-shadow
    const entityBreadcrumbs: BreadcrumbOptions[] = [];
    // eslint-disable-next-line no-shadow
    const argBreadcrumbs: BreadcrumbOptions[] = [];
    if (loading) return { entityBreadcrumbs, argBreadcrumbs };
    // Cluster always goes first in breadcrumbs.
    const clusterName = data.clusters.find((c) => c.id === selectedCluster)?.prettyClusterName || 'unknown cluster';
    const clusterNameToID: Record<string, string> = {};
    data.clusters.forEach((c) => {
      clusterNameToID[c.prettyClusterName] = c.id;
    });
    entityBreadcrumbs.push({
      title: 'cluster',
      value: clusterName,
      selectable: true,
      // eslint-disable-next-line
      getListItems: async (input) => (data.clusters.filter((c) => c.status !== CLUSTER_STATUS_DISCONNECTED
              && c.prettyClusterName.includes(input))
        .map((c) => ({ value: c.prettyClusterName, icon: <StatusCell statusGroup={clusterStatusGroup(c.status)} /> }))
      ),
      onSelect: (input) => {
        setCluster(clusterNameToID[input]);
      },
      requireCompletion: true,
    });

    // Add args to breadcrumbs.
    const argTypes = getArgTypesForVis(vis);

    const argVariableMap = getArgVariableMap(vis);

    // TODO(michelle): We may want to separate non-entity args from the entity args and put them in separate
    // breadcrumbs. For now, they will all go in the same breadcrumbs object.
    Object.entries(args).filter(([argName]) => argName !== 'script').forEach(([argName, argVal]) => {
      // Only add suggestions if validValues are specified. Otherwise, the dropdown is populated with autocomplete
      // entities or has no elements and the user must manually type in values.
      const variable: Variable = argVariableMap[argName];

      const argProps = {
        title: argName,
        value: argVal.toString(),
        selectable: true,
        allowTyping: true,
        onSelect: (newVal) => {
          const newArgs = { ...args, [argName]: newVal };
          setArgs(newArgs);
          execute({
            pxl, vis, args: newArgs, id, liveViewPage,
          });
        },
        getListItems: null,
        requireCompletion: false,
        placeholder: variable?.description,
      };

      if (variable && typeof variable.defaultValue === 'undefined') {
        argProps.title += '*';
      }

      if (variable?.validValues?.length) {
        argProps.getListItems = async (input) => (variable.validValues
          .filter((suggestion) => input === '' || suggestion.indexOf(input) >= 0)
          .map((suggestion) => ({
            value: suggestion,
            description: '',
          })));

        argProps.requireCompletion = true;
      }

      const entityType = pxTypetoEntityType(argTypes[argName]);
      if (entityType !== 'AEK_UNKNOWN') {
        argProps.getListItems = async (input) => (getCompletions(input, entityType)
          .then((results) => (results.data.autocompleteField.map((suggestion) => ({
            value: suggestion.name,
            description: suggestion.description,
            icon: <StatusCell statusGroup={entityStatusGroup(suggestion.state)} />,
          })))));
      }

      // TODO(michelle): Ideally we should just be able to use the entityType to determine whether the
      // arg appears in the argBreadcrumbs. However, some entities still don't have a corresponding entity type
      // (such as nodes), since they are not yet supported in autocomplete. Until that is fixed, this is hard-coded
      // for now.
      if (argName === 'start_time' || argName === 'start') {
        argBreadcrumbs.push(argProps);
      } else {
        entityBreadcrumbs.push(argProps);
      }
    });

    // Add script at end of breadcrumbs.
    entityBreadcrumbs.push({
      title: 'script',
      value: id,
      selectable: true,
      allowTyping: true,
      getListItems: async (input) => {
        // Turns "  px/be_spoke-  " into "px/bespoke"
        const normalize = (s: string) => s.trim().toLowerCase().replace(/[^a-z0-9/]+/gi, '');
        const normalizedInput = normalize(input);
        const normalizedScratchId = normalize(SCRATCH_SCRIPT.id);

        const ids = !input ? [...scriptIds] : scriptIds.filter((s) => {
          const ns = normalize(s);
          return ns === normalizedScratchId || ns.indexOf(normalizedInput) >= 0;
        });

        // The scratch script should always appear at the top of the list for visibility. It doesn't get auto-selected
        // unless it's the only thing in the list.
        const scratchIndex = scriptIds.indexOf(SCRATCH_SCRIPT.id);
        if (scratchIndex !== -1) {
          scriptIds.splice(scratchIndex, 1);
          scriptIds.unshift(SCRATCH_SCRIPT.id);
        }

        return ids.map((scriptId) => ({
          value: scriptId,
          description: scripts.get(scriptId).description,
          autoSelectPriority: scriptId === SCRATCH_SCRIPT.id ? -1 : 0,
        }));
      },
      onSelect: (newVal) => {
        const script = scripts.get(newVal);
        const selectedVis = parseVisOrShowError(script.vis);
        const parsedArgs = argsForVisOrShowError(selectedVis, {
          // Grab the namespace if it exists on a service or pod argument.
          namespace: optionallyGetNamespace(args),
          ...args,
        });
        if (!selectedVis && !parsedArgs) {
          return;
        }

        const execArgs = {
          liveViewPage: entityPageForScriptId(newVal),
          pxl: script.code,
          id: newVal,
          args: parsedArgs,
          vis: selectedVis,
        };
        setScript(execArgs.vis, execArgs.pxl, execArgs.args, execArgs.id, execArgs.liveViewPage);
        if (newVal === SCRATCH_SCRIPT.id) {
          // Skip executing the script, which starts empty. Executing empty scripts emits scary (harmless) errors.
          return;
        }
        if (!containsMutation(execArgs.pxl)) {
          execute(execArgs);
        }
      },
    });

    return { entityBreadcrumbs, argBreadcrumbs };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [loading, id, clusterIds?.join(','), scriptIds?.join(','), args, selectedCluster, getCompletions]);

  if (loading) {
    return (<div>Loading...</div>);
  }

  return (
    <div className={classes.root}>
      <PixieCommandIcon fontSize='large' className={classes.pixieIcon} />
      <div className={classes.verticalLine} />
      <div className={classes.breadcrumbs}>
        <Breadcrumbs
          breadcrumbs={entityBreadcrumbs}
        />
        <div className={classes.spacer} />
        <div>
          <Breadcrumbs
            breadcrumbs={argBreadcrumbs}
          />
        </div>
      </div>
      <ExecuteScriptButton />
    </div>
  );
};

export default withStyles(styles)(LiveViewBreadcrumbs);
