/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import * as QueryString from 'query-string';
import { BehaviorSubject, Observable } from 'rxjs';
import { argsEquals, Arguments } from 'utils/args-utils';
import type { History, LocationState } from 'history';
import plHistory from './pl-history';

interface Params {
  args: Arguments;
  pathname: string;
  scriptId: string;
  scriptDiff: string;
}
// Exported for testing
export class URLParams {
  pathname: string;

  args: Arguments;

  scriptId: string;

  scriptDiff: string;

  onChange: Observable<Params>;

  private readonly subject: BehaviorSubject<Params>;

  private prevParams: Params;

  constructor(private readonly privateWindow: typeof globalThis, private readonly history: History<LocationState>) {
    this.syncWithPathname();
    this.syncWithQueryParams();
    const params = {
      args: { ...this.args },
      pathname: this.pathname,
      scriptDiff: this.scriptDiff,
      scriptId: this.scriptId,
    };
    this.prevParams = params;
    this.subject = new BehaviorSubject(params);
    this.onChange = this.subject;
  }

  // eslint-disable-next-line class-methods-use-this
  private toPathObject(pathname: string, id: string, diff: string, args: Arguments) {
    const params = {
      ...(id ? { script: id } : {}),
      ...(diff ? { diff } : {}),
      ...args,
    };
    const newQueryString = QueryString.stringify(params);
    const search = newQueryString ? `?${newQueryString}` : '';
    return {
      pathname,
      search,
    };
  }

  private syncWithQueryParams() {
    const { script, diff, ...args } = this.getQueryParams();
    this.scriptId = script;
    this.scriptDiff = diff || '';
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

  private syncWithPathname() {
    this.pathname = this.getPathname();
  }

  getPathname(): string {
    return this.privateWindow.location.pathname;
  }

  private pathEqualsPrevious(): boolean {
    return this.pathname && this.pathname === this.prevParams.pathname
    && this.scriptId === this.prevParams.scriptId
    && this.scriptDiff === this.prevParams.scriptDiff
    && argsEquals(this.args, this.prevParams.args);
  }

  private updatePathObject() {
    if (this.pathEqualsPrevious()) return;
    const newPath = this.toPathObject(this.pathname, this.scriptId, this.scriptDiff, this.args);
    this.history.replace(newPath);
  }

  private commitPathObject() {
    // Don't push the state if the params haven't changed.
    if (this.pathEqualsPrevious()) return;
    const newPath = this.toPathObject(this.pathname, this.scriptId, this.scriptDiff, this.args);
    const oldPath = this.toPathObject(this.prevParams.pathname, this.prevParams.scriptId, this.prevParams.scriptDiff,
      this.prevParams.args);
    // Restore the current history state to the previous one, otherwise we would just be pushing the same state again.
    this.history.replace(oldPath);
    this.history.push(newPath);
    this.prevParams = {
      pathname: this.pathname,
      scriptId: this.scriptId,
      scriptDiff: this.scriptDiff,
      args: this.args,
    };
  }

  setPathname(pathname: string): void {
    this.pathname = pathname;
    this.updatePathObject();
  }

  setArgs(newArgs: Arguments): void {
    // Omit the script and diff fields of newArgs.
    const { script, diff, ...args } = newArgs;
    this.args = args;
    this.updatePathObject();
  }

  setScript(id: string, diff: string): void {
    this.scriptId = id;
    this.scriptDiff = diff;
    this.updatePathObject();
  }

  commitAll(newId: string, newDiff: string, newArgs: Arguments): void {
    // Omit the script and diff fields of newArgs.
    const { script, diff, ...args } = newArgs;
    this.scriptId = newId;
    this.scriptDiff = newDiff;
    this.args = args;
    this.commitPathObject();
  }

  // TODO(nserrino): deprecate this.
  triggerOnChange(): void {
    this.syncWithPathname();
    this.syncWithQueryParams();
    this.subject.next({
      args: { ...this.args },
      pathname: this.pathname,
      scriptDiff: this.scriptDiff,
      scriptId: this.scriptId,
    });
  }
}

export default new URLParams(window, plHistory);
