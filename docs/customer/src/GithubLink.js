import * as React from 'react';
import './components/styles.css';

const githubIcon = require('./components/images/github.svg');

const GithubLink = ({ link, text }) => (
  <a href={link} className="githubSection">
    <img className="githubIcon" src={githubIcon} alt="github" />
    {text}
  </a>
);

export default GithubLink;
