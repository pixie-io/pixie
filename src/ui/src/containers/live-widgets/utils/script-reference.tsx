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
import { Link } from 'react-router-dom';
import {
  createStyles, Theme, withStyles, WithStyles,
} from '@material-ui/core';
import { Arguments } from 'utils/args-utils';
import { SemanticType } from 'types/generated/vizierapi_pb';
import { scriptToEntityURL, toEntityURL, toSingleEntityPage } from './live-view-params';

const styles = ({ palette }: Theme) => createStyles({
  root: {
    color: palette.secondary.main,
    textDecoration: 'underline',
    backgroundColor: 'transparent',
    opacity: 0.7,
  },
});

// EntityLink is used when we are creating a deep link to an entity's script
// based on its semantic type.
export interface EntityLinkProps extends WithStyles<typeof styles>{
  entity: string;
  semanticType: SemanticType;
  clusterName: string;
  propagatedParams?: Arguments;
}

const EntityLinkPlain = ({
  entity, semanticType, clusterName, classes, propagatedParams,
}: EntityLinkProps) => {
  const page = toSingleEntityPage(entity, semanticType, clusterName);
  const path = toEntityURL(page, propagatedParams);
  return (
    <Link to={path} className={classes.root}>{entity}</Link>
  );
};

export const EntityLink = withStyles(styles)(EntityLinkPlain);

export function isEntityType(semanticType: SemanticType): boolean {
  switch (semanticType) {
    case SemanticType.ST_SERVICE_NAME:
    case SemanticType.ST_POD_NAME:
    case SemanticType.ST_NODE_NAME:
    case SemanticType.ST_NAMESPACE_NAME:
      return true;
    default:
      return false;
  }
}

// ScriptReference is used when we are creating a deep link from a script name.
export interface ScriptReferenceProps extends WithStyles<typeof styles>{
  label: string;
  script: string;
  clusterName: string;
  args: Arguments;
}

const ScriptReferencePlain = ({
  label, script, args, clusterName, classes,
}: ScriptReferenceProps) => {
  const path = scriptToEntityURL(script, clusterName, args);
  return (
    <Link to={path} className={classes.root}>{label}</Link>
  );
};

export const ScriptReference = withStyles(styles)(ScriptReferencePlain);
