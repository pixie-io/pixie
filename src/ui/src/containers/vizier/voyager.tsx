import './vizier.scss';

import * as React from 'react';

import * as libVoyager from 'datavoyager';

import 'datavoyager/build/style.css';
import {ResultsToJSON} from '../../utils/result-data-utils';

interface VoyagerProps {
  data: string;
}

export class Voyager extends React.Component<VoyagerProps, {}> {
  private el: HTMLDivElement;
  private voyagerInstance: any;

  constructor(props) {
    super(props);

    this.el = document.createElement('div');
    this.el.classList.add('voyager-container');
  }

  componentDidMount = () => {
    const root = document.getElementById('voyager-div');
    root.appendChild(this.el);
    this.voyagerInstance = libVoyager.CreateVoyager(this.el, {
      hideHeader: true,
    }, null);
  }

  componentWillUnmount = () => {
    const root = document.getElementById('voyager-div');
    root.removeChild(this.el);
  }

  componentDidUpdate = (prevProps) => {
    if (prevProps.data !== this.props.data) {
      const data: any = {
        values: ResultsToJSON(JSON.parse(this.props.data)),
      };

      this.voyagerInstance.updateData(data);
    }
  }

  render() {
    return <div id='voyager-div'></div>;
  }
}
