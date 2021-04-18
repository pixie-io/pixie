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

import { useTheme } from '@material-ui/core/styles';

import {
  QuantilesBoxWhisker,
  SelectedPercentile,
} from './quantiles-box-whisker';

export default {
  title: 'QuantilesBoxWhisker',
  component: QuantilesBoxWhisker,
};

export const Basic = () => {
  const [selectedPercentile, setSelectedPercentile] = React.useState<
  SelectedPercentile
  >('p99');
  const theme = useTheme();

  return (
    <div style={{ background: 'black' }}>
      <QuantilesBoxWhisker
        p50={100.123}
        p90={200.234}
        p99={500.789}
        max={600}
        p50HoverFill={theme.palette.success.main}
        p90HoverFill={theme.palette.warning.main}
        p99HoverFill={theme.palette.error.main}
        p50Display='100 ms'
        p90Display='200 ms'
        p99Display='500 ms'
        selectedPercentile={selectedPercentile}
        onChangePercentile={setSelectedPercentile}
      />
    </div>
  );
};
