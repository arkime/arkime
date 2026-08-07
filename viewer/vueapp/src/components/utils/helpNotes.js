// Cyclable in-app help messages, in display order. Page tips carry the
// route they describe (used to pick the starting message) and a Help page
// anchor; their text lives at i18n key helpNotes.<id>.text. The 'welcome'
// and 'v7' entries render custom content in HelpNotes.vue.
export const HIDE_ALL_NOTES = 'all';

export default [
  { id: 'welcome' },
  { id: 'v7' },
  { id: 'sessions', route: 'Sessions', anchor: '#sessions' },
  { id: 'spiview', route: 'Spiview', anchor: '#spiview' },
  { id: 'spigraph', route: 'Spigraph', anchor: '#spigraph' },
  { id: 'hunt', route: 'Hunt', anchor: '#hunt' },
  { id: 'files', route: 'Files', anchor: '#files' },
  { id: 'stats', route: 'Stats', anchor: '#stats' },
  { id: 'history', route: 'ArkimeHistory', anchor: '#history' },
  { id: 'settings', route: 'Settings', anchor: '#settings' }
];
