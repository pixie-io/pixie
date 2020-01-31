import 'datavoyager/build/style.css';
import './vizier.scss';

import {tablesFromResults} from 'components/chart/data';
import * as libVoyager from 'datavoyager';
import {ExecuteQueryResult} from 'gql-types';
import * as React from 'react';
import {Button, Dropdown, DropdownButton} from 'react-bootstrap';

import Modal from '@material-ui/core/Modal';

import {ResultsToJSON} from '../../utils/result-data-utils';

interface VoyagerProps {
  data: string;
}

export function voyagerEnabled(): boolean {
  return localStorage.getItem('px-voyager') === 'true';
}

function parseData(data: string) {
  try {
    return { values: ResultsToJSON(JSON.parse(data)) };
  } catch (e) {
    return null;
  }
}

export class Voyager extends React.PureComponent<VoyagerProps> {
  private el = React.createRef<HTMLDivElement>();
  private voyagerInstance: any;


  componentDidMount = () => {
    this.voyagerInstance = libVoyager.CreateVoyager(this.el.current, {
      hideHeader: true,
    }, parseData(this.props.data));
  }

  componentDidUpdate = (prevProps) => {
    if (prevProps.data !== this.props.data) {
      this.voyagerInstance.updateData(parseData(this.props.data));
    }
  }

  render() {
    return <div ref={this.el} className='voyager-container'></div>;
  }
}

interface VoyagerTriggerProps {
  data?: ExecuteQueryResult;
}

export const VoyagerTrigger = React.memo<VoyagerTriggerProps>(({ data }) => {
  if (!voyagerEnabled() || !data) {
    return null;
  }
  const tables = tablesFromResults(data.ExecuteQuery);
  if (tables.length < 1) {
    return null;
  }

  const [voyagerData, setVoyagerData] = React.useState('');
  const [voyagerOpened, setVoyagerOpened] = React.useState(false);
  const openVoyager = (table) => {
    return () => {
      setVoyagerData(table.data);
      setVoyagerOpened(true);
    };
  };
  const closeVoyager = React.useCallback(() => setVoyagerOpened(false), []);

  return (
    <>
      {
        tables.length === 1 ?
          <Button onClick={openVoyager(tables[0])}>Voyager</Button> :
          <DropdownButton title='voyager' id='voyager'>
            {
              tablesFromResults(data.ExecuteQuery).map((table, i) => (
                <Dropdown.Item
                  onClick={openVoyager(table)}
                  key={`table-${table.name}-${i}`}
                >
                  {table.name}
                </Dropdown.Item>
              ))
            }
          </DropdownButton>
      }
      <Modal open={voyagerOpened} onClose={closeVoyager} className='pixie-voyager' keepMounted>
        <Voyager data={voyagerData} />
      </Modal>
    </>
  );
});
