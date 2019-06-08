# Pixie Customer Docs

## 🚀 Quickstart

To run in development mode run the following commands:

```
$ yarn install
$ yarn start
```

Visit `http://localhost:8000/` to view the app.

To generate production version run:
```
$ yarn install
$ yarn build
```

## Notes
- For sub nesting in left sidebar, create a folder with the same name as the top level `.md` filename and the sub navigation is auto-generated. Currently it supports only one level of nesting. The sub navigation is alphabetically ordered.

## Live Code Editor

To render react components for live editing, add the `react-live=true` to the code section. For example:

```javascript react-live=true
<button>Edit my text</button>
```

In the above code, just add `javascript react-live=true` after the triple quote ``` to start rendering react components that can be edited by users.

## 🤖 SEO friendly

This is a static site and comes with all the SEO benefits. Configure meta tags like title and description for each markdown file using MDX Frontmatter

```markdown
---
title: "Title of the page"
metaTitle: "Meta Title Tag for this page"
metaDescription: "Meta Description Tag for this page"
---
```

Canonical URLs are generated automatically.
