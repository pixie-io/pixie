import * as React from 'react';
import { Link } from 'react-router-dom';
import {
  createStyles, Theme, withStyles, WithStyles,
} from '@material-ui/core';
import { Arguments } from 'utils/args-utils';
import { SemanticType } from '../../../types/generated/vizier_pb';
import { toEntityURL, toSingleEntityPage } from './live-view-params';

const styles = ({ palette }: Theme) => createStyles({
  root: {
    color: palette.secondary.main,
    textDecoration: 'none',
    backgroundColor: 'transparent',
    opacity: 0.7,
    '&:hover': {
      textDecoration: 'underline',
    },
  },
});

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
