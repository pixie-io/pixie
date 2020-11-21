import * as React from 'react';
import { Link } from 'react-router-dom';
import {
  createStyles, Theme, withStyles, WithStyles,
} from '@material-ui/core';
import { Arguments } from 'utils/args-utils';
import { SemanticType } from '../../../types/generated/vizier_pb';
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
