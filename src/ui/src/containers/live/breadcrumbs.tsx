import * as React from 'react';

import {
  createStyles, Theme, withStyles,
} from '@material-ui/core/styles';
import gql from 'graphql-tag';
import { useQuery, useApolloClient } from '@apollo/react-hooks';

import PixieCommandIcon from 'components/icons/pixie-command';
import Breadcrumbs, { BreadcrumbOptions } from 'components/breadcrumbs/breadcrumbs';
import ClusterContext from 'common/cluster-context';
import { CLUSTER_STATUS_DISCONNECTED } from 'common/vizier-grpc-client-context';
import { argsForVis, getArgTypesForVis } from 'utils/args-utils';
import { ScriptsContext } from 'containers/App/scripts-context';
import { ScriptContext } from 'context/script-context';
import { entityPageForScriptId, optionallyGetNamespace } from 'components/live-widgets/utils/live-view-params';
import { parseVis } from 'containers/live/vis';
import { EntityType, pxTypetoEntityType, entityStatusGroup } from 'containers/new-command-input/autocomplete-utils';
import { StatusCell } from 'components/status/status';
import { clusterStatusGroup } from 'containers/admin/utils';
import { ContainsMutation } from 'utils/pxl';
import IconButton from '@material-ui/core/IconButton';
import Tooltip from '@material-ui/core/Tooltip';
import ExecuteScriptButton from './execute-button';

const LIST_CLUSTERS = gql`
{
  clusters {
    id
    clusterName
    prettyClusterName
    status
  }
}
`;

const AUTOCOMPLETE_FIELD_QUERY = gql`
query getCompletions($input: String, $kind: AutocompleteEntityKind, $clusterUID: String) {
  autocompleteField(input: $input, fieldType: $kind, clusterUID: $clusterUID) {
    name
    description
    matchedIndexes
    state
  }
}
`;

const styles = (({ shape, palette, spacing }: Theme) => createStyles({
  root: {
    display: 'flex',
    alignItems: 'center',
    height: spacing(4.5),
    paddingTop: spacing(1),
    paddingBottom: spacing(1),
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
  breadcrumbs: {
    width: '100%',
    overflow: 'hidden',
    display: 'flex',
    height: spacing(3),
  },
}));

const LiveViewBreadcrumbs = ({ classes, commandOpen, toggleCommandOpen }) => {
  const { loading, data } = useQuery(LIST_CLUSTERS);
  const { selectedCluster, setCluster, selectedClusterUID } = React.useContext(ClusterContext);
  const { scripts } = React.useContext(ScriptsContext);

  const {
    vis, pxl, args, id, liveViewPage, setArgs, execute, setScript,
  } = React.useContext(ScriptContext);

  const client = useApolloClient();
  const getCompletions = React.useCallback((newInput: string, kind: EntityType) => (client.query({
    query: AUTOCOMPLETE_FIELD_QUERY,
    fetchPolicy: 'network-only',
    variables: {
      input: newInput,
      kind,
      clusterUID: selectedClusterUID,
    },
  })
  ), [client, selectedClusterUID]);

  const scriptIds = React.useMemo(() => [...scripts.keys()], [scripts]);

  if (loading) {
    return (<div>Loading...</div>);
  }

  const entityBreadcrumbs: BreadcrumbOptions[] = [];
  const argBreadcrumbs: BreadcrumbOptions[] = [];

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
    getListItems: async () => (data.clusters.filter((c) => c.status !== CLUSTER_STATUS_DISCONNECTED)
      .map((c) => ({ value: c.prettyClusterName, icon: <StatusCell statusGroup={clusterStatusGroup(c.status)} /> }))
    ),
    onSelect: (input) => {
      setCluster(clusterNameToID[input]);
    },
    requireCompletion: false,
  });

  // Add args to breadcrumbs.
  const argTypes = getArgTypesForVis(vis);
  // TODO(michelle): We may want to separate non-entity args from the entity args and put them in separate
  // breadcrumbs. For now, they will all go in the same breadcrumbs object.
  Object.entries(args).filter(([argName]) => argName !== 'script').forEach(([argName, argVal]) => {
    const argProps = {
      title: argName,
      value: argVal,
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
    };
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
  // TODO(michelle): Make script editable.
  entityBreadcrumbs.push({
    title: 'script',
    value: id,
    selectable: true,
    allowTyping: true,
    getListItems: async (input) => {
      if (!input) {
        return scriptIds.map((scriptId) => ({ value: scriptId, description: scripts.get(scriptId).description }));
      }
      return scriptIds
        .filter((scriptId) => scriptId.indexOf(input) >= 0)
        .map((scriptId) => ({ value: scriptId, description: scripts.get(scriptId).description }));
    },
    onSelect: (newVal) => {
      const script = scripts.get(newVal);
      const selectedVis = parseVis(script.vis);
      const execArgs = {
        liveViewPage: entityPageForScriptId(newVal),
        pxl: script.code,
        id: newVal,
        args: argsForVis(selectedVis, {
          // Grab the namespace if it exists on a service or pod argument.
          namespace: optionallyGetNamespace(args),
          ...args,
        }),
        vis: selectedVis,
      };
      setScript(execArgs.vis, execArgs.pxl, execArgs.args, execArgs.id, execArgs.liveViewPage);
      if (!ContainsMutation(execArgs.pxl)) {
        execute(execArgs);
      }
    },
  });

  return (
    <div className={classes.root}>
      <Tooltip title='Pixie Command'>
        <IconButton disabled={commandOpen} onClick={toggleCommandOpen}>
          <PixieCommandIcon fontSize='large' className={classes.pixieIcon} />
        </IconButton>
      </Tooltip>
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
