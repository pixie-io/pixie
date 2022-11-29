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

import { LoginButton } from 'app/components';

interface SocialProps {
  buttonText: string;
  extraParams: Record<string, any>;
}

export interface OIDCButtonsProps {
  configs: Array<SocialProps>;
  onButtonClick: (params: Record<string, any>) => void;
}

/* eslint-disable react-memo/require-memo, react-memo/require-usememo */
export const OIDCButtons: React.FC<OIDCButtonsProps> = ({
  configs,
  onButtonClick,
}) => (
  <>
    {configs.map((config, i) => <LoginButton key={i} text={config.buttonText} onClick={() => {
      onButtonClick(config.extraParams);
    }} />)}
  </>
);
/* eslint-enable react-memo/require-memo, react-memo/require-usememo */
OIDCButtons.displayName = 'OIDCButtons';
