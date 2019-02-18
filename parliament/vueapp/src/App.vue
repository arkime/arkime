<template>
  <div id="app">
    <div v-if="compatibleBrowser">
      <parliament-navbar />
      <router-view class="parliament-content" />
    </div>
    <div v-else>
      <parliament-upgrade-browser>
      </parliament-upgrade-browser>
    </div>
  </div>
</template>

<script>
import ParliamentNavbar from './components/Navbar';
import ParliamentUpgradeBrowser from './components/UpgradeBrowser';

export default {
  name: 'App',
  components: {
    ParliamentNavbar,
    ParliamentUpgradeBrowser
  },
  data: function () {
    return {
      compatibleBrowser: true
    };
  },
  mounted: function () {
    this.compatibleBrowser = (typeof Object['__defineSetter__'] === 'function') &&
      !!String.prototype.includes;

    if (!this.compatibleBrowser) {
      console.log('Incompatible browser, please upgrade!');
    }
  }
};
</script>

<style>
/* app styles -------------------------------- */
#app {
  font-family: 'Helvetica Neue', Helvetica, Arial, sans-serif !important;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
}

body { background-color: #F0F0F0; }

body {
  --px-none   : 0;          /* 0px */
  --px-xs     : 0.125rem;   /* 2px */
  --px-sm     : .25rem;     /* 4px */
  --px-md     : .5rem;      /* 8px */
  --px-lg     : .75rem;     /* 12px */
  --px-xlg    : 1rem;       /* 16px */
  --px-xxlg   : 1.5rem;     /* 24px */
  --px-xxxlg  : 2rem;       /* 32px */
  --px-xxxxlg : 3rem;       /* 48px */
  --px-xxxxxlg: 3.5rem;     /* 56px */
}

.parliament-content {
  margin-top: 70px;
}

a.no-href { color: #007bff !important; }
a.no-href:hover { color: #0056b3 !important; }

.no-decoration { text-decoration: none !important; }

.text-muted-more { color: #DDDDDD; }

/* small, condensed styles ------------------- */
.alert.alert-sm  {
  font-size: .85rem;
  padding: .25rem .4rem;
  margin-bottom: .5rem;
}
.alert.alert-sm > .close {
  line-height: .75;
}

.btn-xs {
  padding: .1rem .2rem;
  font-size: .9rem;
  border-radius: .2rem;
}
.dropdown-btn-xs > button.dropdown-toggle {
  padding: .1rem .2rem;
  font-size: .9rem;
  border-radius: .2rem;
}

/* cursors ----------------------------------- */
.cursor-help, .help-cursor { cursor: help; }
.cursor-text, .text-cursor { cursor: text; }
.cursor-pointer, .pointer-cursor { cursor: pointer; }
.cursor-move, .move-cursor { cursor: move; }

/* info display ------------------------------ */
/* displays large text for important information
 * note: must contain an inner div with the text
 * example:
 * <div class="info-area">
 *   <div>Some important text!</div>
 * </div>
 */
.info-area {
  font-size: 1.5rem;
  color: #777777;
}

.info-area div {
  padding: 2rem;
  border-radius: .25rem;
  background-color: #FFFFFF;
}

.info-area span.fa {
  display: flex;
  justify-content: center;
}

.vertical-center {
  min-height: 30vh;
  display: flex;
  align-items: center;
  justify-content: center;
  flex-direction: column;
}

/* info page (404 & upgrade) */
.parliament-info {
  margin-top: 20px;
}
.parliament-info .center-area > img {
  z-index: 99;
}
.parliament-info .center-area {
  display: flex;
  align-items: center;
  justify-content: center;
  text-align: center;
  flex-direction: column;
  min-height: 75vh;
}
.parliament-info .well {
  margin-top: -6px;
  min-width: 25%;
  padding: 12px;
  background-color: #F6F6F6;
  border: 1px solid #EEEEEE;
  border-radius: 3px;
  box-shadow: 4px 4px 10px 0 rgba(0,0,0,0.5);
}
.parliament-info .well > h1 {
  margin-top: 0;
  color: #DB0A65;
}

/* media queries ----------------------------- */
@media (min-width: 1600px) {
  .col-xxl-2 {
    -ms-flex: 0 0 16.666667%;
    flex: 0 0 16.666667%;
    max-width: 16.666667%;
  }
}

/* DARK THEME -------------------------------- */
/* darken background/lighten text */
body.dark {
  background-color: #222;
  color: #EEE;
}

/* darken navbar */
body.dark .navbar.bg-dark {
  background-color: #131313 !important;
}

/* darken cards */
body.dark .card {
  background-color: #303030;
  border: 1px solid rgba(0, 0, 0, 0.125);
}
body.dark .card .card-header,
body.dark .card .card-footer {
  background-color: #383838;
}
body.dark .cluster-handle {
  background: #303030;
}

/* darken tables */
body.dark .table-danger,
body.dark .table-danger > th,
body.dark .table-danger > td {
  background-color: #871D45;
}
body.dark .table-hover .table-danger:hover,
body.dark .table-hover .table-danger:hover > th,
body.dark .table-hover .table-danger:hover > td {
  background-color: #A32252;
}
body.dark .table-warning,
body.dark .table-warning > th,
body.dark .table-warning > td {
  background-color: #895A10;
}
body.dark .table-hover .table-warning:hover,
body.dark .table-hover .table-warning:hover > th,
body.dark .table-hover .table-warning:hover > td {
  background-color: #9B6611;
}
body.dark .table-secondary,
body.dark .table-secondary > th,
body.dark .table-secondary > td {
  background-color: #494949;
}
body.dark .table-hover .table-secondary:hover,
body.dark .table-hover .table-secondary:hover > th,
body.dark .table-hover .table-secondary:hover > td {
  background-color: #4F4F4F;
}

/* darken info aread */
body.dark .info-area > div {
  background-color: #444;
  color: #CCC !important;
}

/* darken alerts */
body.dark .alert.alert-danger {
  color: #FCABCA;
  background-color: #8E2049;
  border-color: #871D45;
}
body.dark .alert .issue-date {
  color: #9F9F9F;
}

/* darken warnings */
body.dark .alert.alert-warning {
  color: #FFF3CD;
  background-color: #916012;
  border-color: #895A10;
}

/* darken dropdown menus */
body.dark .dropdown-menu {
  background-color: #222;
  border: 1px solid #333;
}
body.dark .dropdown-menu .dropdown-item {
  color: #EEE;
}
body.dark .dropdown-menu .dropdown-item:hover,
body.dark .dropdown-menu .dropdown-item:focus {
  color: #EEE;
  text-decoration: none;
  background-color: #444;
}

/* lighten dark buttons */
body.dark .btn-outline-dark {
  color: #ADB5BD;
  border-color: #ADB5BD;
}
body.dark .btn-outline-dark:hover {
  color: #222;
  background-color: #ADB5BD;
  border-color: #ADB5BD;
}
</style>
