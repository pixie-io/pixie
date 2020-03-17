import 'datavoyager/build/style.css';
import './vizier.scss';

import {Table} from 'common/vizier-grpc-client';
import * as React from 'react';
import {Button, Dropdown, DropdownButton} from 'react-bootstrap';
import {dataFromProto} from 'utils/result-data-utils';

import Modal from '@material-ui/core/Modal';

import {ResultsToJSON} from '../../utils/result-data-utils';

interface VoyagerProps {
  data: any[];
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
  private createVoyagerInstance: any;

  componentDidMount = () => {
    this.createVoyagerInstance = import(/* webpackChunkName: "datavoyager" */ 'datavoyager').then((module) => {
      this.voyagerInstance = module.CreateVoyager(this.el.current, {
        hideHeader: true,
      }, { values: this.props.data });
    });
  }

  componentDidUpdate = (prevProps) => {
    this.createVoyagerInstance.then(() => {
      if (prevProps.data !== this.props.data) {
        this.voyagerInstance.updateData({ values: this.props.data });
      }
    });
  }

  render() {
    return <div ref={this.el} className='voyager-container'></div>;
  }
}

interface VoyagerTriggerProps {
  data?: Table[];
}

export const VoyagerTrigger = React.memo<VoyagerTriggerProps>(({ data }) => {
  if (!voyagerEnabled() || !data) {
    return null;
  }
  if (data.length < 1) {
    return null;
  }

  const [voyagerData, setVoyagerData] = React.useState([]);
  const [voyagerOpened, setVoyagerOpened] = React.useState(false);
  const openVoyager = (table) => {
    return () => {
      setVoyagerData(dataFromProto(table.relation, table.data));
      setVoyagerOpened(true);
    };
  };
  const closeVoyager = React.useCallback(() => setVoyagerOpened(false), []);

  return (
    <>
      {
        data.length === 1 ?
          <Button onClick={openVoyager(data[0])}>Voyager</Button> :
          <DropdownButton title='voyager' id='voyager'>
            {
              data.map((table, i) => (
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
