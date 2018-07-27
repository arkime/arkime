<template>
  <div id="app">
    <parliament-navbar />
    <router-view />
  </div>
</template>

<script>
import ParliamentNavbar from './components/Navbar';

export default {
  name: 'App',
  components: {
    ParliamentNavbar
  }
};
</script>

<style>
/* app styles -------------------------------- */
#app {
  font-family: 'Avenir', Helvetica, Arial, sans-serif;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
}

html, body { background-color: #F0F0F0; }

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

/* media queries ----------------------------- */
@media (min-width: 1600px) {
  .col-xxl-2 {
    -ms-flex: 0 0 16.666667%;
    flex: 0 0 16.666667%;
    max-width: 16.666667%;
  }
}
</style>
