<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <!-- toggle button: teleported to body so it escapes the page
         overlay's stacking context and paints over the toolbar chrome
         (z-tie with .page-chrome, later in DOM = on top, like the
         legacy .fixed-header tie) -->
    <teleport to="body">
      <div
        class="sticky-session-btn bounce"
        :style="btnStyle"
        ref="stickyContainer"
        @click="toggleStickySessions"
        v-if="!open && sortedSessions && sortedSessions.length > 0">
        <v-icon icon="mdi-chevron-double-left" />&nbsp;
        <small>{{ sortedSessions.length }}</small>
        <v-tooltip activator="parent">
          {{ $t('sessions.sticky.toggleOpenTip') }}
        </v-tooltip>
      </div>
    </teleport> <!-- /toggle button -->

    <!-- sticky sessions content -->
    <transition name="slide">
      <div
        v-if="open"
        class="sticky-session-detail"
        :style="showToolBars ? null : { top: '35px' }">
        <!-- sticky sessions list -->
        <ul class="sticky-list">
          <li class="sticky-list-item sticky-list-header">
            <div class="d-flex align-center gap-1">
              <h4 class="mb-0 me-auto">
                {{ $t('sessions.sticky.openSessionCount', sortedSessions.length) }}
              </h4>
              <div class="arkime-input-group sort-by-select">
                <select
                  v-model="sortBy"
                  class="arkime-input-control">
                  <option
                    disabled
                    value="">
                    {{ $t('sessions.sortBy') }}
                  </option>
                  <option value="firstPacket">
                    {{ $t('sessions.startTime') }}
                  </option>
                  <option value="lastPacket">
                    {{ $t('sessions.stopTime') }}
                  </option>
                </select>
              </div>
              <v-btn
                v-if="sortBy && sortOrder === 'asc'"
                variant="outlined"
                size="small"
                density="comfortable"
                icon
                @click="toggleSortOrder">
                <v-icon icon="mdi-chevron-up" />
                <v-tooltip activator="parent">
                  {{ $t('sessions.sticky.sortDescTip') }}
                </v-tooltip>
              </v-btn>
              <v-btn
                v-if="sortBy && sortOrder === 'desc'"
                variant="outlined"
                size="small"
                density="comfortable"
                icon
                @click="toggleSortOrder">
                <v-icon icon="mdi-chevron-down" />
                <v-tooltip activator="parent">
                  {{ $t('sessions.sticky.sortAscTip') }}
                </v-tooltip>
              </v-btn>
              <v-btn
                variant="outlined"
                size="small"
                density="comfortable"
                icon
                @click="toggleStickySessions">
                <v-icon icon="mdi-chevron-double-right" />
                <v-tooltip activator="parent">
                  {{ $t('sessions.sticky.toggleOpenTip') }}
                </v-tooltip>
              </v-btn>
              <v-btn
                variant="outlined"
                size="small"
                density="comfortable"
                icon
                @click="closeAll">
                <v-icon icon="mdi-close" />
                <v-tooltip activator="parent">
                  {{ $t('sessions.sticky.closeAllTip') }}
                </v-tooltip>
              </v-btn>
            </div>
          </li>
          <transition-group
            name="slide"
            tag="span">
            <a
              class="sticky-list-item sticky-list-item-animate cursor-pointer"
              @click="scrollTo(session.id)"
              v-for="session in sortedSessions"
              :key="session.id">
              <div class="sticky-list-item-text">
                <v-btn
                  variant="text"
                  size="x-small"
                  density="comfortable"
                  icon
                  class="float-right"
                  :aria-label="$t('common.close')"
                  @click.stop="closeSessionDetail(session)">
                  <v-icon
                    icon="mdi-close"
                    size="small" />
                </v-btn>
                <small>
                  <v-icon icon="mdi-clock-outline" />
                  <em>
                    {{ timezoneDateString(session.firstPacket, timezone, ms) }} -
                    {{ timezoneDateString(session.lastPacket, timezone, ms) }}
                  </em>
                  <br>
                  <strong>{{ session['source.ip'] }}</strong>:{{ session['source.port'] }} <strong>{{ session['source.geo.country_iso_code'] }}</strong> -
                  <strong>{{ session['destination.ip'] }}</strong>:{{ session['destination.port'] }} <strong>{{ session['destination.geo.country_iso_code'] }}</strong>
                  <br>
                  <strong>{{ protocol(session.ipProtocol) }}</strong> - {{ session.node }}
                </small>
              </div>
            </a>
          </transition-group>
        </ul> <!-- /sticky sessions list -->
      </div>
    </transition> <!-- /sticky sessions content -->
  </div>
</template>

<script>
import { timezoneDateString, protocol } from '@common/vueFilters.js';

export default {
  name: 'ArkimeStickySessions',
  emits: ['closeSession', 'closeAllSessions'],
  props: {
    sessions: {
      type: Array,
      default: () => []
    },
    ms: {
      type: Boolean,
      default: false
    },
    vizVisible: {
      type: Boolean,
      default: false
    }
  },
  data: function () {
    return {
      open: false,
      sortBy: '', // use the order sessions are opened
      sortOrder: 'desc',
      oldLength: this.sessions.length
    };
  },
  watch: {
    sessions: {
      deep: true,
      handler (newVal, oldVal) {
        const newLength = newVal.length;

        // only sort changed, nothing to do
        if (newLength === this.oldLength) { return; }

        if (!newLength) {
          this.open = false;
          return;
        }

        if (newLength > this.oldLength) {
          const btn = this.$refs.stickyContainer;
          if (btn) {
            btn.classList.remove('bounce');
            setTimeout(() => btn.classList.add('bounce'));
          }
        }

        this.oldLength = newLength;
      }
    }
  },
  computed: {
    // store toggle (shared across pages); collapsed toolbar rides the
    // button/panel up to the top
    showToolBars: function () {
      return this.$store.state.showToolBars;
    },
    // tab position: rides to the top when the toolbar is collapsed; drops
    // below the visualization (timeline/map) when any viz is showing so it
    // clears the viz's top-right controls; otherwise sits just below the
    // toolbar chrome.
    btnStyle: function () {
      if (!this.showToolBars) { return { top: '4px', zIndex: 8 }; }
      if (this.vizVisible) { return { top: 'calc(var(--arkime-navbar-height, 36px) + 300px)' }; }
      return { top: 'calc(var(--arkime-navbar-height, 36px) + 105px)' };
    },
    /**
     * Orders the sessions by start or stop time
     * Triggered when sortBy/sortOrder is changed and when a session
     * is added to the sticky sessions list
     * Default sort order is the order in which the sessions were opened
     */
    sortedSessions () {
      if (this.sortBy) {
        return [...this.sessions].sort((a, b) => {
          let result;
          if (this.sortOrder === 'desc') {
            result = b[this.sortBy] - a[this.sortBy];
          } else {
            result = a[this.sortBy] - b[this.sortBy];
          }
          return result;
        });
      } else {
        return this.sessions;
      }
    },
    timezone: function () {
      return this.$store.state.user.settings.timezone;
    }
  },
  methods: {
    protocol,
    timezoneDateString,
    /* exposed functions --------------------------------------------------- */
    /* Opens/closes the sticky sessions panel */
    toggleStickySessions: function () {
      this.open = !this.open;
    },
    /**
     * Orders the sessions ascending or descending
     * Triggered when sortOrder is changed
     */
    toggleSortOrder: function () {
      if (this.sortOrder === 'asc') {
        this.sortOrder = 'desc';
      } else {
        this.sortOrder = 'asc';
      }
    },
    /**
     * Closes the display of the session detail for the specified session
     * @param {Object} session The session to collapse details
     */
    closeSessionDetail: function (session) {
      this.$emit('closeSession', session.id);
    },
    /* Closes all the open sessions and the panel */
    closeAll: function () {
      this.open = false;
      this.$emit('closeAllSessions');
    },
    /**
     * Scrolls to specified session
     * @param {string} id The id of the session to scroll to
     */
    scrollTo: function (id) {
      const el = document.getElementById(`session${id}`);
      if (el) { el.scrollIntoView(true); }
    }
  }
};
</script>

<style scoped>
/* viewport-fixed tab handle: sits just below the toolbar chrome at the
   top of the panel so it clears the fetch-viz gear that floats in the
   paging bar above. Body-teleported; the panel slides in beneath it
   (btn z5 > panel z4). When the toolbar collapses, :style rides both up
   to the top. Kept in sync with .sticky-session-detail's top. */
.sticky-session-btn {
  width: 100px;
  display: block;
  position: fixed;
  top: calc(var(--arkime-navbar-height, 36px) + 96px);
  right: 0;
  z-index: 5;
  margin-right: -50px;
  overflow: hidden;
  padding: 1px 0px 1px 0px;
  border-radius: 4px 0 0 4px;
  cursor: pointer;
  background-color: rgb(var(--v-theme-quaternary));
  color: rgb(var(--v-theme-button-fg));
}

.sort-by-select {
  width: 90px;
}

.sticky-session-detail {
  overflow-y: auto;
  position: fixed;
  /* navbar height + page-toolbar (search bar + paging bar) so the panel
     starts right below the chrome */
  top: calc(var(--arkime-navbar-height, 36px) + 96px);
  right: 0;
  bottom: 0;
  z-index: 4;
  width: 360px;
  border-left: 1px solid rgb(var(--v-theme-neutral-light));
  background-color: rgb(var(--v-theme-neutral-lighter));

  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

.sticky-session-detail .sticky-list {
  margin-bottom: 0;
  padding-left: 0;
  list-style: none;
}

.sticky-session-detail .sticky-list-item {
  display: block;
  border-top: 1px solid rgb(var(--v-theme-neutral-light));
  padding: 4px 8px;
  background-color: rgb(var(--v-theme-background));
  color: rgb(var(--v-theme-foreground));
}
.sticky-session-detail .sticky-list-item:first-child {
  border-top: 0;
}

.sticky-session-detail .sticky-list-item .sticky-list-item-text {
  line-height: 1.25;
}

a.sticky-list-item:hover,
a.sticky-list-item:focus {
  background-color: rgb(var(--v-theme-tertiary-lightest));
}

.sticky-session-detail .sticky-list-item.sticky-list-header {
  padding: 12px 8px;
  background-color: rgb(var(--v-theme-neutral-lighter));
}

/* ANIMATIONS ---------------------- */
/* bounce the sticky sessions button */
.sticky-session-btn.bounce {
  -webkit-animation: bounce 1000ms linear both;
     -moz-animation: bounce 1000ms linear both;
          animation: bounce 1000ms linear both;
}

/* animate sticky-session-detail and sticky-list-item slide in/out */
.slide-enter-active, .slide-leave-active {
  transition: all .5s ease;
}
.slide-enter-from, .slide-leave-to {
  transform: translateX(360px);
}
.sticky-list-item.sticky-list-item-animate {
  width: 100%;
  transition: all .5s ease;
  display: inline-block;
}

/* bounces item to the left */
@keyframes bounce {
  from, 20%, 53%, 80%, to {
    animation-timing-function: cubic-bezier(0.215, 0.61, 0.355, 1);
    transform: translate3d(0, 0, 0);
  }
  40%, 43% {
    animation-timing-function: cubic-bezier(0.755, 0.05, 0.855, 0.06);
    transform: translate3d(-50px, 0, 0);
  }
  70% {
    animation-timing-function: cubic-bezier(0.755, 0.05, 0.855, 0.06);
    transform: translate3d(-20px, 0, 0);
  }
  90% {
    transform: translate3d(-10px, 0, 0);
  }
}
</style>
