import { matchPath } from 'react-router';

export class LiveViewURLParams {
  constructor(public clusterName?: string) { }

  matchLiveViewParams(path: string): boolean {
    let changed = false;
    const clusterMatch = matchPath<{ cluster: string }>(path, {
      path: '/live/clusters/:cluster',
    });

    if (clusterMatch && this.clusterName !== clusterMatch.params.cluster) {
      changed = true;
      this.clusterName = clusterMatch.params.cluster;
    }

    return changed;
  }

  toURL(): string {
    return `/live/clusters/${this.clusterName}`;
  }
}
