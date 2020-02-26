import * as React from 'react';

import {VizierQueryResult} from './vizier-grpc-client';

interface VizierGRPCClient {
  executeScript: (script: string, args?: {}) => Promise<VizierQueryResult>;
}

const VizierGRPCClientContext = React.createContext<VizierGRPCClient>(null);

export default VizierGRPCClientContext;
