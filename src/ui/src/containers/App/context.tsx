import { CloudClient } from 'common/cloud-gql-client';
import * as React from 'react';

export const CloudClientContext = React.createContext<CloudClient>(null);
