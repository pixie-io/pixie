import {GQLQueryResult} from '../../../../vizier/services/api/controller/schema/schema';

export interface ChartProps {
  data: GQLQueryResult;
  height: number;
  width: number;
}
