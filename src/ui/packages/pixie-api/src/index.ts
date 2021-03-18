export {
  PixieAPIClient, PixieAPIClientAbstract, ClusterConfig,
} from './api';

export { PixieAPIClientOptions } from './types/client-options';

/**
 * NOTE: Unless you are authoring a framework-specific library (such as @pixie-labs/api-react), these should be ignored.
 * They're an implementation detail from the consumer's view, but are exposed for libraries like @pixie-labs/api-react.
 */
export {
  USER_QUERIES,
  API_KEY_QUERIES,
  DEPLOYMENT_KEY_QUERIES,
  CLUSTER_QUERIES,
  AUTOCOMPLETE_QUERIES,
} from './gql-queries';

// Note: GQLUserSetting is not exposed because UserSettings is more specific.
export { DEFAULT_USER_SETTINGS, UserSettings } from './user-settings';

export { VizierQueryErrorType, VizierQueryError, GRPCStatusCode } from './vizier';

export { CloudClient } from './cloud-gql-client';

export { containsMutation, isStreaming } from './utils/pxl';

export {
  Table as VizierTable,
  BatchDataUpdate,
  ExecutionStateUpdate,
  ExecutionEvent,
  // TODO(nick): VizierGRPCClient shouldn't be exposed; remove this line once the UI code drops its direct dependency.
  VizierGRPCClient,
  VizierQueryArg,
  VizierQueryFunc,
  VizierQueryResult,
} from './vizier-grpc-client';

// TODO(nick): Create @pixie-labs/api-react/testing as its own package by doing the same trick that Apollo does.
export * from './testing';

/* Generated types begin (types are generated but these exports are manually updated) */

export {
  GQLQuery,
  GQLUserInfo,
  // Note: GQLUserSetting is not exposed because we already expose the more specific UserSettings from ./user-settings.
  // GQLUserSetting,
  GQLClusterInfo,
  GQLVizierConfig,
  GQLPodStatus,
  GQLContainerStatus,
  GQLK8sEvent,
  GQLClusterConnectionInfo,
  GQLClusterStatus,
  GQLCLIArtifact,
  GQLArtifactsInfo,
  GQLArtifact,
  GQLAutocompleteResult,
  GQLAutocompleteEntityKind,
  GQLAutocompleteEntityState,
  GQLAutocompleteActionType,
  GQLTabSuggestion,
  GQLAutocompleteSuggestion,
  GQLLiveViewMetadata,
  GQLLiveViewContents,
  GQLScriptMetadata,
  GQLScriptContents,
  GQLDeploymentKey,
  GQLAPIKey,
  GQLMutation,
  GQLResolver,
  GQLQueryTypeResolver,
  GQLUserInfoTypeResolver,
  GQLUserSettingTypeResolver,
  GQLClusterInfoTypeResolver,
  GQLVizierConfigTypeResolver,
  GQLPodStatusTypeResolver,
  GQLContainerStatusTypeResolver,
  GQLK8sEventTypeResolver,
  GQLClusterConnectionInfoTypeResolver,
  GQLCLIArtifactTypeResolver,
  GQLArtifactsInfoTypeResolver,
  GQLArtifactTypeResolver,
  GQLAutocompleteResultTypeResolver,
  GQLTabSuggestionTypeResolver,
  GQLAutocompleteSuggestionTypeResolver,
  GQLLiveViewMetadataTypeResolver,
  GQLLiveViewContentsTypeResolver,
  GQLScriptMetadataTypeResolver,
  GQLScriptContentsTypeResolver,
  GQLDeploymentKeyTypeResolver,
  GQLAPIKeyTypeResolver,
  GQLMutationTypeResolver,
} from './types/schema';

export {
  Axis,
  BarChart,
  Graph,
  RequestGraph,
  HistogramChart,
  VegaChart,
  TimeseriesChart,
  Vis,
  PXType,
  Table,
  Widget,
} from './types/generated/vis_pb';

export {
  BooleanColumn,
  Column,
  CompilerError,
  DataType,
  ErrorDetails,
  ExecuteScriptRequest,
  ExecuteScriptResponse,
  Float64Column,
  HealthCheckRequest,
  HealthCheckResponse,
  Int64Column,
  LifeCycleState,
  MutationInfo,
  QueryData,
  QueryExecutionStats,
  QueryMetadata,
  QueryTimingInfo,
  Relation,
  RowBatchData,
  ScalarValue,
  SemanticType,
  Status,
  StringColumn,
  Time64NSColumn,
  UInt128,
  UInt128Column,
} from './types/generated/vizierapi_pb';

export { VizierServiceClient } from './types/generated/VizierapiServiceClientPb';

/* Generated types end */
