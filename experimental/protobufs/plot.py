# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

from bisect import bisect_right
import sys

class discrete_cdf:
    def __init__(self, data):
        self._data = data # must be sorted
        self._data_len = float(len(data))

    def __call__(self, point):
        return (len(self._data[:bisect_right(self._data, point)]) /
                self._data_len)

class segment_count:
    def __init__(self, data):
        self._data = data
        self._data_len = float(len(data))

    def __call__(self, segment):
        return (len([n for n in self._data if segment[0] <= n < segment[1]]) / self._data_len)

from scipy.stats import norm
import matplotlib.pyplot as plt

if len(sys.argv) != 2:
    sys.exit("Must a have exactly one argument as the file path, got $0.".format(
        len(sys.argv)))

with open(sys.argv[1]) as f:
    your_data = [float(l) for l in f]

import numpy

cdf = discrete_cdf(your_data)
seg = segment_count(your_data)

xvalues = numpy.arange(0.05, 1.01, 0.1)
segments = [[n-0.05, n+0.05] for n in xvalues]
print segments
yvalues = [seg(s) for s in segments]
plt.bar(numpy.arange(len(yvalues)), yvalues, align='center', alpha=0.1)
plt.xticks(yvalues, xvalues)
plt.ylabel('Count')
plt.title('Sample count of aliasing probability')
plt.show()
