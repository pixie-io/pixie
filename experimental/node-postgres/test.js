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

const pg = require('pg');
const fs = require('fs');

const pool = new pg.Pool({
  user: 'postgres',
  host: '127.0.0.1',
  // database: 'mywebstore',
  // password: '123',
  port: '5432',
  ssl: {
    rejectUnauthorized: false,
    ca: fs.readFileSync('/tmp/ssl/ca.crt').toString(),
    key: fs.readFileSync('/tmp/ssl/client.key').toString(),
    cert: fs.readFileSync('/tmp/ssl/client.crt').toString(),
  },
});

pool.query("SELECT NOW()", (err, res) => {
  // console.log(err, res);
})
