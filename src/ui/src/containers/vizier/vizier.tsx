import * as React from 'react';
import {Route} from 'react-router-dom';
import {AgentDisplay} from './agent-display';
import {QueryManager} from './query-manager';
import './vizier.scss';

interface VizierProps {
  match: any;
}
export class Vizier extends React.Component<VizierProps, {}> {
  constructor(props) {
    super(props);
  }

  render() {
    const matchPath = this.props.match.path;
    return (
      <div className='vizier-body'>
        <Route path={`${matchPath}/agents`} component={AgentDisplay} />
        <Route path={`${matchPath}/query`} component={QueryManager} />
      </div>
    );
  }
}
export default Vizier;
