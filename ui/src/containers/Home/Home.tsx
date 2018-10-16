import * as React from 'react';

export class Home extends React.Component<{}, {}> {
  constructor(props) {
    super(props);

  }

  render() {
    return (
      <div>
        Home Page!
        <br/>
        <a href='/login' id='login-link'>Login</a>
      </div>
    );
  }
}
export default Home;
