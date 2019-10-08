import * as _ from 'lodash';
import * as React from 'react';
import { DraggableCore } from 'react-draggable';
import './content-box.scss';

// @ts-ignore : TS does not like image files.
import * as resizerSvg from 'images/icons/ResizePanel.svg';

export interface ContentBoxProps {
    headerText: string | JSX.Element;
    subheaderText?: string | JSX.Element;
    secondaryText?: string | JSX.Element;
    children: any;
    resizable?: boolean;
    initialHeight?: number;
}

export interface ContentBoxState {
  height: number;
}

export class ContentBox extends React.Component<ContentBoxProps, ContentBoxState> {
  constructor(props) {
      super(props);
      this.state = {
        height: props.initialHeight,
      };
  }

  renderResizer() {
    return (
      <DraggableCore
        onDrag={(event, { deltaX, deltaY }) => {
              this.setState({
                height: this.state.height + deltaY,
              });
          }
        }
      >
        <div
          className='content-box--resizer'
          >
          <img draggable={false} src={resizerSvg}/>
        </div>
      </DraggableCore>
      );
  }

  render() {
    return (<div className='content-box--wrapper'>
      <div className='content-box' data-resizable={this.props.resizable}>
        <div className='content-box--header'>
          <div className='content-box--header-text'>
            {_.toUpper(this.props.headerText)}
          </div>
          <div className='content-box--subheader-text'>
            {this.props.subheaderText ? '| ' : ''}
            {this.props.subheaderText}
          </div>
          <div className='spacer'/>
          <div className='content-box--secondary-text'>
            {this.props.secondaryText}
          </div>
        </div>
        <div className='content-box--content' style={{height: this.state.height }}>
          {this.props.children}
        </div>
      </div>
      { this.props.resizable ? this.renderResizer() : null }
    </div>);
  }
}
//
