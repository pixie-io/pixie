import './editor.scss';

import { MUTATE_DRAWER_OPENED, QUERY_DRAWER_OPENED } from 'common/local-gql';
import gql from 'graphql-tag';
// @ts-ignore : TS does not like image files.
import * as closeIcon from 'images/icons/cross.svg';
// @ts-ignore : TS does not like image files.
import * as newTabIcon from 'images/icons/new-tab.svg';
import * as React from 'react';
import { Button, Nav, Tab, Tabs } from 'react-bootstrap';
import * as uuid from 'uuid/v1';

import { useMutation, useQuery } from '@apollo/react-hooks';

import { Drawer } from '../../components/drawer/drawer';
import { saveCodeToStorage } from './code-utils';
import { EditorDrawerMenu } from './drawer-menu';
import { ConsoleTab } from './tab';

const NEW_TAB = 'new-tab';
const PIXIE_EDITOR_TABS_KEY = 'pixie-editor-tabs';
const PIXIE_EDITOR_LAST_OPEN_TAB_KEY = 'pixie-editor-last-opened';

export interface EditorTabInfo {
  title: string;
  id: string;
}

interface EditorState {
  tabs: EditorTabInfo[];
  activeTab: string;
}

export const Editor = () => {
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

  const createNewTab = React.useCallback((query?) => {
    const newTab = {
      title: (query && query.id) || 'untitled',
      id: uuid(),
    };
    if (query && query.code) {
      saveCodeToStorage(newTab.id, query.code);
    }
    setState(({ tabs }) => {
      return { tabs: [...tabs, newTab], activeTab: newTab.id };
    });
  }, []);

  const { data } = useQuery(QUERY_DRAWER_OPENED);
  const [updateDrawer] = useMutation(MUTATE_DRAWER_OPENED);
  const updateDrawerMemo = React.useCallback((opened) => {
    updateDrawer({ variables: { drawerOpened: opened } });
  }, []);

  return (
    <div className='pixie-editor-container' style={{ display: 'flex', height: '100%' }}>
      <Drawer
        openedWidth='15vw'
        defaultOpened={data && data.drawerOpened}
        onOpenedChanged={updateDrawerMemo}
      >
        <EditorDrawerMenu onSelect={createNewTab} />
      </Drawer>
      <div className='pixie-editor-tabs-container'>
        <Tab.Container
          activeKey={state.activeTab}
          onSelect={selectTab}
          mountOnEnter={true}
          id='pixie-editor-tabs'>
          <Nav variant='tabs' className='pixie-editor-tabs-nav'>
            {state.tabs.map((tab) =>
              <Nav.Item key={tab.id} as={Nav.Link} eventKey={tab.id}>
                <EditorTabTitle {...tab} onClose={(id) => deleteTab(id)} />
              </Nav.Item>,
            )}
            <Nav.Item as={Nav.Link} eventKey={NEW_TAB} className='pixie-editor-tabs-new'>
              <img src={newTabIcon} />
            </Nav.Item>
          </Nav>
          <Tab.Content className='pixie-editor-tab-content-container'>
            {state.tabs.map((tab) =>
              <Tab.Pane
                eventKey={tab.id}
                key={tab.id}
                unmountOnExit={false}
                className='pixie-editor-content-fullbleed'>
                <ConsoleTab {...tab} />
              </Tab.Pane>,
            )}
          </Tab.Content>
        </Tab.Container>
      </div>
    </div >
  );
};

const EditorTabTitle: React.FC<EditorTabInfo & { onClose: (id: string) => void }> = (props) => {
  const onClick = React.useCallback((e) => {
    e.stopPropagation(); // Stop the tab from getting selected.
    props.onClose(props.id);
  }, []);
  return (
    <div className='pixie-editor-tab-title'>
      <span className='pixie-editor-tab-title-text'>
        {props.title}
      </span>
      <img
        className='pixie-editor-tab-title-close-icon'
        src={closeIcon}
        onClick={onClick}
      />
    </div>
  );
};
