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

/* eslint-disable import/no-extraneous-dependencies */
import { configure } from 'enzyme';
import Adapter from 'enzyme-adapter-react-16';
/* eslint-enable import/no-extraneous-dependencies */
import noop from 'app/utils/noop';

const globalAny: any = global;

configure({ adapter: new Adapter() });
// Jest uses jsdom, where document.createRange is not specified. This is used
// in some of our external dependencies. Mock this out so tests don't fail.
if (globalAny.document) {
  document.createRange = () => ({
    setStart: noop,
    setEnd: noop,
    commonAncestorContainer: {
      nodeName: 'BODY',
      ownerDocument: document,
    },
  } as any);
}
