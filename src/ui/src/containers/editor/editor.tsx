import {CodeEditor} from 'components/code-editor';
// @ts-ignore : TS does not like image files.
import * as closeIcon from 'images/icons/cross.svg';
// @ts-ignore : TS does not like image files.
import * as newTabIcon from 'images/icons/new-tab.svg';
import * as React from 'react';
import {Tab, Tabs} from 'react-bootstrap';
import * as uuid from 'uuid/v1';

const NEW_TAB = 'new-tab';
const PIXIE_EDITOR_TABS_KEY = 'pixie-editor-tabs';
const PIXIE_EDITOR_LAST_OPEN_TAB_KEY = 'pixie-editor-last-opened';
const PIXIE_EDITOR_CODE_KEY_PREFIX = 'px-';
const DEFAULT_CODE = '# Enter Query Here\n';

interface EditorTabInfo {
  title: string;
  id: string;
}

interface EditorState {
  tabs: EditorTabInfo[];
  activeTab: string;
}

export const Editor: React.FC = () => {
  let savedTabs: EditorTabInfo[] = [];
  try {
    const saved = JSON.parse(localStorage.getItem(PIXIE_EDITOR_TABS_KEY));
    if (saved && Array.isArray(saved)) {
      savedTabs = saved.filter((t) => !!t.id && !!t.title);
    }
  } catch (e) {
    //
  }

  if (!savedTabs.length) {
    savedTabs = [{ title: 'untitled', id: uuid() }];
  }

  let lastOpenTab = localStorage.getItem(PIXIE_EDITOR_LAST_OPEN_TAB_KEY);
  if (!lastOpenTab || savedTabs.findIndex((t) => t.id === lastOpenTab) === -1) {
    lastOpenTab = savedTabs[0].id;
  }

  const [state, setState] = React.useState<EditorState>({ tabs: savedTabs, activeTab: lastOpenTab });

  React.useEffect(() => {
    localStorage.setItem(PIXIE_EDITOR_TABS_KEY, JSON.stringify(state.tabs));
  }, [state.tabs]);

  React.useEffect(() => {
    localStorage.setItem(PIXIE_EDITOR_LAST_OPEN_TAB_KEY, state.activeTab);
  }, [state.activeTab]);

  const selectTab = (id: string) => {
    if (id === NEW_TAB) {
      createNewTab();
      return;
    }
    setState(({ tabs }) => ({ tabs, activeTab: id }));
  };

  const deleteTab = (id) => {
    setState(({ tabs, activeTab }) => {
      const newTabs = tabs.filter((t) => t.id !== id);
      if (newTabs.length === 0) {
        const newTab = {
          title: 'untitled',
          id: uuid(),
        };
        return { tabs: [newTab], activeTab: newTab.id };
      }
      if (activeTab !== id) {
        return { tabs: newTabs, activeTab };
      }
      const removeIdx = tabs.findIndex((t) => t.id === id);
      const nextIdx = Math.min(removeIdx, newTabs.length - 1);
      const nextActiveTab = newTabs[nextIdx].id;
      return { tabs: newTabs, activeTab: nextActiveTab };
    });
  };

  const createNewTab = () => {
    setState(({ tabs }) => {
      const newTab = {
        title: 'untitled',
        id: uuid(),
      };
      return { tabs: [...tabs, newTab], activeTab: newTab.id };
    });
  };

  return (
    <Tabs
      activeKey={state.activeTab}
      onSelect={selectTab}
      mountOnEnter={true}
      id='pixie-editor-tabs'>
      {state.tabs.map((tab) =>
        <Tab
          eventKey={tab.id}
          key={tab.id}
          title={<EditorTabTitle {...tab} onClose={(id) => deleteTab(id)} />}>
          <EditorTabContent {...tab} />
        </Tab>,
      )}
      <Tab eventKey={NEW_TAB} title={<img src={newTabIcon} />}></Tab>
    </Tabs>
  );
};

const EditorTabTitle: React.FC<EditorTabInfo & { onClose: (id: string) => void }> = (props) => {
  return (
    <div style={{ display: 'flex', alignItems: 'center' }}>
      {props.title}
      <img
        style={{ marginLeft: '8px', width: '16px', height: '16px' }}
        src={closeIcon}
        onClick={(e) => {
          e.stopPropagation(); // Stop the tab from getting selected.
          props.onClose(props.id);
        }}
      />
    </div>
  );
};

export const EditorTabContent: React.FC<EditorTabInfo> = (props) => {
  const initialCode = getCodeFromStorage(props.id) || DEFAULT_CODE;
  const [code, setCode] = React.useState<string>(initialCode);
  return (
    <CodeEditor
      code={code}
      onChange={(c) => {
        setCode(c);
        saveCodeToStorage(props.id, c);
      }}
    />
  );
};

function codeIdToKey(id: string): string {
  return `${PIXIE_EDITOR_CODE_KEY_PREFIX}${id}`;
}

function saveCodeToStorage(id: string, code: string) {
  localStorage.setItem(codeIdToKey(id), code);
}

function getCodeFromStorage(id: string): string {
  return localStorage.getItem(codeIdToKey(id)) || '';
}
