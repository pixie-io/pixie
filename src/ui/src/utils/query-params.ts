import * as QueryString from 'query-string';

type Args = { [key: string]: string };

interface Location {
  search: string;
  protocol: string;
  host: string;
  pathname: string;
}

interface History {
  pushState(data: any, title: string, url?: string): void;
}

export interface Window {
  location: Location;
  history: History;
}

// Exported for testing
export class QueryParams {
  args: Args;
  scriptId: string;
  scriptDiff: string;

  constructor(private readonly privateWindow: Window) {
    const { script, diff, ...args } = this.getQueryParams();
    this.scriptId = script;
    this.scriptDiff = diff;
    this.args = args;
  }

  private getQueryParams(): { [key: string]: string } {
    const params = {};
    const parsed = QueryString.parse(this.privateWindow.location.search);
    for (const key of Object.keys(parsed)) {
      if (typeof parsed[key] === 'string') {
        params[key] = parsed[key];
      }
    }
    return params;
  }

  private setQueryParams(params: { [key: string]: string }) {
    const { protocol, host, pathname } = this.privateWindow.location;
    const newQueryString = QueryString.stringify(params);
    const search = newQueryString ? `?${newQueryString}` : '';
    const newurl = `${protocol}//${host}${pathname}${search}`;

    this.privateWindow.history.pushState({ path: newurl }, '', newurl);
  }

  private updateURL() {
    this.setQueryParams({
      script: this.scriptId,
      ...(this.scriptDiff ? { diff: this.scriptDiff } : {}),
      ...this.args,
    });
  }

  setArgs(newArgs: Args) {
    // Omit the script and diff fields of newArgs.
    const { script, diff, ...args } = newArgs;
    this.args = args;
    this.updateURL();
  }

  setScript(id: string, diff: string) {
    this.scriptId = id;
    this.scriptDiff = diff;
    this.updateURL();
  }

  setAll(id: string, diff: string, args: Args) {
    this.scriptId = id;
    this.scriptDiff = diff;
    this.setArgs(args);
  }
}

export default new QueryParams(window);
