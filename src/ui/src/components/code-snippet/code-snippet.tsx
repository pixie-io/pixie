import * as _ from 'lodash';
import * as React from 'react';
import * as ReactDOM from 'react-dom';
import {OverlayTrigger, Tooltip} from 'react-bootstrap';
import Highlight from 'react-highlight';

import 'highlight.js/styles/github.css';
import './code-snippet.scss';

// @ts-ignore : TS does not like image files.
import * as copyImage from 'images/icons/copy.svg';

interface CodeSnippetProps {
    showCopy: boolean;
    children: string;
    language: string;
}

interface CodeSnippetState {
  copied: boolean;
}

export class CodeSnippet extends React.Component<CodeSnippetProps, CodeSnippetState> {
  constructor(props) {
    super(props);
    this.state = {
      copied: false,
    };
  }

  handleCopySnippet() {
    const domEl = ReactDOM.findDOMNode(this) as Element;
    const copyText = domEl.querySelector('.code-snippet pre');

    // You can only copy from a visible text area, so create a text area containing
    // the snippet contents, copy, then delete the text area.
    const textArea = document.createElement('textarea');
    textArea.value = copyText.textContent;
    document.body.appendChild(textArea);
    textArea.select();
    document.execCommand('Copy');
    textArea.remove();

    // Change tooltip text to say "copied".
    this.setState({
      copied: true,
    });
    const timer = setInterval(() => {
      this.setState({
        copied: false,
      });
      clearInterval(timer);
    }, 2000);
  }

  renderCopyTooltip(props) {
    props.scheduleUpdate();
    return (
      <Tooltip className='code-snippet--tooltip' {...props}>
        { this.state.copied ? 'Copied' : 'Copy to clipboard' }
      </Tooltip>);
  }

  renderCopyButton() {
    return (
      <OverlayTrigger
        placement='top'
        overlay={this.renderCopyTooltip.bind(this)}
      >
        <div className='code-snippet--copy-button' onClick={this.handleCopySnippet.bind(this)}>
          <img src={copyImage}/>
        </div>
      </OverlayTrigger>
    );
  }

  render() {
    return (
      <div className='code-snippet'>
        <div className='code-snippet--content'>
          {this.props.language !== '' ?
            <Highlight language={this.props.language}>
              {this.props.children}
            </Highlight> :
            <pre>
              {this.props.children}
            </pre>
          }
        </div>

        { this.props.showCopy ? this.renderCopyButton() : null}
      </div>
    );
  }
}
