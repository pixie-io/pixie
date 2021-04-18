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

import * as React from 'react';
import { ColumnProps, DataTable } from './data-table';

type Sample = [string, number, number, number, number];

const sample: Sample[] = [
  ['Frozen yoghurt', 159, 6.0, 24, 4.0],
  ['Ice cream sandwich', 237, 9.0, 37, 4.3],
  ['Eclair', 262, 16.0, 24, 6.0],
  ['Cupcake', 305, 3.7, 67, 4.3],
  ['Gingerbread', 356, 16.0, 49, 3.9],
];

function createData(
  id: number,
  dessert: string,
  calories: number,
  fat: number,
  carbs: number,
  protein: number,
) {
  return {
    id,
    dessert,
    calories,
    fat,
    carbs,
    protein,
  };
}

const rows = [];

for (let i = 0; i < 200; i += 1) {
  const s = sample[Math.floor(Math.random() * sample.length)];
  rows.push(createData(i, ...s));
}

function getRow(i: number) {
  return rows[i];
}
const columns: ColumnProps[] = [
  {
    dataKey: 'id',
    label: 'ID',
    align: 'center',
  },
  {
    dataKey: 'dessert',
    label: 'Dessert Name',
  },
  {
    dataKey: 'calories',
    label: 'calories',
    align: 'end',
  },
  {
    dataKey: 'fat',
    label: 'Fat(g)',
    align: 'end',
  },
  {
    dataKey: 'carbs',
    label: 'Carbs(g)',
    align: 'end',
  },
  {
    dataKey: 'protein',
    label: 'Protein(g)',
    align: 'end',
  },
];

export default {
  title: 'DataTable',
  component: DataTable,
  decorators: [
    (Story) => (
      <div style={{ height: '500px' }}>
        <Story />
      </div>
    ),
  ],
};

export const Basic = () => (
  <DataTable columns={columns} rowGetter={getRow} rowCount={rows.length} />
);

export const Compact = () => (
  <DataTable
    compact
    columns={columns}
    rowGetter={getRow}
    rowCount={rows.length}
  />
);
