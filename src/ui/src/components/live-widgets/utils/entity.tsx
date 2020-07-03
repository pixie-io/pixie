import * as React from 'react';
import { Link } from 'react-router-dom';
import {
  createStyles, Theme, withStyles, WithStyles,
} from '@material-ui/core';
import { SemanticType } from '../../../types/generated/vizier_pb';
import { toEntityPathname, toSingleEntityPage } from './live-view-params';

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
  className?: string;
}

const EntityLinkPlain = ({
  entity, semanticType, clusterName, className, classes,
}: EntityLinkProps) => {
  const page = toSingleEntityPage(entity, semanticType, clusterName);
  const pathname = toEntityPathname(page);
  return (
    <div className={className}>
      <Link to={pathname} className={classes.root}>{entity}</Link>
    </div>
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
