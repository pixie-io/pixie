import * as React from 'react';

export interface User {
  email: string;
  orgName: string;
}

interface UserContextProps {
  user: User;
}

export default React.createContext<UserContextProps>(null);
