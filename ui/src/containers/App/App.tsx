import * as React from 'react';

export interface AppProps {
  name: string;
}

export class App extends React.Component<AppProps, any> {
  render() {
    return (<div>{this.props.name}</div>);
  }
}
