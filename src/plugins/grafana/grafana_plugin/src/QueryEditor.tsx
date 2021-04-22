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

import defaults from 'lodash/defaults';

import React, { ChangeEvent, PureComponent } from 'react';
import { TextArea } from '@grafana/ui';
import { QueryEditorProps } from '@grafana/data';
import { DataSource } from './datasource';
import { defaultQuery, MyDataSourceOptions, MyQuery } from './types';

type Props = QueryEditorProps<DataSource, MyQuery, MyDataSourceOptions>;

export class QueryEditor extends PureComponent<Props> {
  onQueryTextChange(event: ChangeEvent<HTMLTextAreaElement>) {
    const { onChange, query, onRunQuery } = this.props;
    onChange({ ...query, queryText: event.target.value });
    onRunQuery();
  }

  render() {
    const query = defaults(this.props.query, defaultQuery);
    const { queryText } = query;

    return (
      <div className="gf-form">
        <TextArea
          id="Query Text"
          name="Query Text"
          rows={20}
          width={4}
          value={queryText || ""}
          onChange={this.onQueryTextChange.bind(this)}
          label="Query Text"
        />
      </div>
    );
  }
}
