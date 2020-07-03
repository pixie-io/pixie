import * as QueryString from 'query-string';
import * as React from 'react';
import ActionCard from '../../components/action-card/action-card';

export const AuthComplete = ({ location }) => {
  const { err, siteName } = QueryString.parse(location.search);
  const title = err ? 'Auth Failed' : 'Auth Complete';
  return (
    <ActionCard title={title} width='33%' minWidth='480'>
      <p className='pixie-auth-complete-msg'>
        {!err ? 'Authentication successful. Please close this page.'
          : err === 'token'
            ? (
              <>
                Authentication failed. Please make sure the account has
                <br />
                permission to use the site
                {' '}
                <strong>{siteName}</strong>
                .
              </>
            )
            : 'Authentication failed. Please try again later.'}
      </p>
    </ActionCard>
  );
};
