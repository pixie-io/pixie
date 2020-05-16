import 'highlight.js/styles/github.css';
import './code-snippet.scss';

import * as copyImage from 'images/icons/copy.svg';
import * as React from 'react';
import { OverlayTrigger, Tooltip } from 'react-bootstrap';
import Highlight from 'react-highlight';

interface CodeSnippetProps {
  showCopy: boolean;
  children: string;
  language: string;
}

interface CodeSnippetState {
  copied: boolean;
}

export class CodeSnippet extends React.Component<CodeSnippetProps, CodeSnippetState> {
  private textRef: React.RefObject<HTMLPreElement>;
  constructor(props) {
    super(props);
    this.state = {
      copied: false,
    };
    this.textRef = React.createRef();
  }

  handleCopySnippet() {
    // You can only copy from a visible text area, so create a text area containing
    // the snippet contents, copy, then delete the text area.
    const textArea = document.createElement('textarea');
    textArea.value = this.textRef.current.textContent;
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
        {this.state.copied ? 'Copied' : 'Copy to clipboard'}
      </Tooltip>);
  }

  renderCopyButton() {
    return (
      <OverlayTrigger
        placement='top'
        overlay={this.renderCopyTooltip.bind(this)}
      >
        <div className='code-snippet--copy-button' onClick={this.handleCopySnippet.bind(this)}>
          <img src={copyImage} />
        </div>
      </OverlayTrigger>
    );
  }

  render() {
    return (
      <div className='code-snippet'>
        <div className='code-snippet--content'>
          {this.props.language !== '' ?
            <Highlight className={this.props.language}>
              {this.props.children}
            </Highlight> :
            <pre ref={this.textRef}>
              {this.props.children}
            </pre>
          }
        </div>

        {this.props.showCopy ? this.renderCopyButton() : null}
      </div>
    );
  }
}
