import {Header} from 'components/header/header';
import * as React from 'react';
import {Route} from 'react-router-dom';
import {AgentDisplay} from './agent-display';
import {QueryManager} from './query-manager';

import './vizier.scss';

const PATH_TO_HEADER_TITLE = {
  '/vizier/agents': 'Agents',
  '/vizier/query': 'Query',
};

export interface RouterInfo {
  pathname: string;
}

interface VizierProps {
  match: any;
  location: RouterInfo;
}
export class Vizier extends React.Component<VizierProps, {}> {
  constructor(props) {
    super(props);
  }

  render() {
    const matchPath = this.props.match.path;
    return (
      <div className='vizier-body'>
        <Header
           primaryHeading='Pixie Console'
           secondaryHeading={PATH_TO_HEADER_TITLE[this.props.location.pathname]}
        />
        <Route path={`${matchPath}/agents`} component={AgentDisplay} />
        <Route path={`${matchPath}/query`} component={QueryManager} />
      </div>
    );
  }
}
export default Vizier;
