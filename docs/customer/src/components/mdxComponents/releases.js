import * as React from 'react';
import gql from 'graphql-tag';
import { useQuery } from '@apollo/react-hooks';
import ReactMarkdown from 'react-markdown';

const dateOptions = { year: 'numeric', month: 'long', day: 'numeric' };

export const GET_ARTIFACTS = gql`
  query artifacts($artifactName: String!) {
    artifacts(artifactName: $artifactName) {
      items {
        version
        changelog
        timestampMs
      }
    }
  }
`;

const Releases = (props) => {
  const { artifactName } = props;
  const { loading, error, data } = useQuery(GET_ARTIFACTS, {
    variables: { artifactName },
  });

  if (loading) {
    return 'Fetching releases...';
  }
  if (error) {
    return 'Error: Could not fetch releases.';
  }
  return (
    <div>
      {data.artifacts.items.map((item, idx) => (
        <div>
          <Release
            version={item.version}
            changelog={item.changelog}
            timestampMs={item.timestampMs}
          />
          {idx === data.artifacts.items.length - 1 ? null : <hr />}
        </div>
      ))}
    </div>
  );
};

const Release = (props) => {
  const { timestampMs, version, changelog } = props;
  const d = new Date(parseInt(timestampMs, 10));
  const ds = d.toLocaleDateString('en-US', dateOptions);

  // TODO(michelle): We currently aren't handling the markdown levels correctly.
  // The content rendered in ReactMarkdown should be at level 3.
  return (
    <div>
      <ReactMarkdown source={`#### v${version} (${ds})`} />
      <div>
        <ReactMarkdown source={changelog.replace(/\\n/g, '\n')} />
      </div>
    </div>
  );
};

export default Releases;
