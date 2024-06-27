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

// These aren't quite the Material UI colors, but they follow the same principles.
export const COLORS = {
  PRIMARY: {
    '100': '#D0FBFB',
    '200': '#A1F7F7',
    '300': '#6DF3F3',
    '400': '#35EEEE',
    '500': { // Brand primary color!
      dark: '#12D6D6',
      light: '#12D6D6',
    },
    '600': '#0DA0A0',
    '700': '#096C6C',
    '800': '#053838',
    '900': '#031C1C',
  },
  SECONDARY: {
    '100': '#E0F4FF',
    '200': '#C2EAFF',
    '300': '#99DBFF',
    '400': '#61C8FF',
    '500': '#24B2FF',
    '600': '#0095E5',
    '700': '#006EA8',
    '800': '#004266',
    '900': '#002133',
  },
  NEUTRAL: {
    '100': '#FFFFFF',
    '200': '#E6E6EA',
    '300': '#C3C3CB',
    '400': '#686875',
    '500': '#4E4E4E',
    '600': '#3B3B3B',
    '700': '#333333',
    '800': '#232323',
    '850': '#1E1E1E',
    '900': '#121212',
    '1000': '#0A0A0A',
  },
  SUCCESS: {
    '300': '#50F6CE',
    '400': '#20F3C1',
    '500': '#0BD3A3',
    '600': '#0AC296',
  },
  ERROR: {
    '300': '#FFA8B0',
    '400': '#FF7582',
    '500': '#FF5E6D',
    '600': '#FF4C5D',
  },
  WARNING: {
    '300': '#FACA6B',
    '400': '#F8B83A',
    '500': '#F6A609',
    '600': '#E79C08',
  },
  INFO: {
    '300': '#B5E4FD',
    '400': '#83D2FB',
    '500': '#53C0FA',
    '600': '#20ADF9',
  },
  // Note: these two colors are similar enough that RGB interpolation doesn't create a noticeable grey zone.
  // If it did, we'd need to manually compute a few middle points in a colorspace like HCL or LAB to get a better one.
  GRADIENT: 'linear-gradient(to right, #00DBA6 0%, #24B2FF 100%)',
};
