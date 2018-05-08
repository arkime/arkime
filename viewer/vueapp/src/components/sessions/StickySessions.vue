<template>

  <div class="bounce"
    ref="stickyContainer">

    <!-- toggle button -->
    <div class="sticky-session-btn"
      @click="toggleStickySessions()"
      v-if="sessions && sessions.length > 0"
      v-b-tooltip.hover
      title="Toggle view of expanded sessions">
      <span v-if="!open"
        class="fa fa-angle-double-left">
      </span><span v-else
        class="fa fa-angle-double-right">
      </span>&nbsp;
      <small>{{ sessions.length }}</small>
    </div> <!-- /toggle button -->

    <!-- sticky sessions content -->
    <transition name="slide">
      <div v-if="open"
        class="sticky-session-detail">

        <!-- sticky sessions list -->
        <ul class="list-group">
          <li class="list-group-item list-group-header">
            <a v-b-tooltip.hover
              @click="closeAll()"
              title="Close all open sessions"
              class="btn btn-default btn-sm pull-right ml-1">
              <span class="fa fa-close">
              </span>
            </a>
            <span v-if="sortBy">
              <a v-if="sortOrder === 'asc'"
                v-b-tooltip.hover
                @click="toggleSortOrder()"
                title="Sorting ascending, click to sort descending"
                class="btn btn-default btn-sm pull-right ml-1">
                <span class="fa fa-sort-asc">
                </span>
              </a>
              <a v-if="sortOrder === 'desc'"
                v-b-tooltip.hover
                @click="toggleSortOrder()"
                title="Sorting descending, click to sort ascending"
                class="btn btn-default btn-sm pull-right ml-1">
                <span class="fa fa-sort-desc">
                </span>
              </a>
            </span>
            <select v-model="sortBy"
              @change="sort()"
              class="form-control form-control-sm pull-right sort-by-select">
              <option disabled value="">
                Sort by...
              </option>
              <option value="firstPacket">
                Start Time
              </option>
              <option value="lastPacket">
                Stop Time
              </option>
            </select>
            <h4>
              {{ sessions.length }} Open
              Session<span v-if="sessions.length > 1">s</span>
            </h4>
          </li>
          <transition-group name="slide">
            <a class="list-group-item list-group-item-animate cursor-pointer"
              @click="scrollTo(session.id)"
              v-for="session in sessions"
              :key="session.id">
              <div class="list-group-item-text">
                <button class="btn btn-xs btn-link pull-right"
                  @click.stop="closeSessionDetail(session)">
                  <span class="fa fa-close fa-lg">
                  </span>
                </button>
                <small>
                  <span class="fa fa-clock-o fa-fw">
                  </span>
                  <em>
                    {{ session.firstPacket / 1000 | timezoneDateString(timezone, 'YYYY/MM/DD HH:mm:ss z') }} -
                    {{ session.lastPacket / 1000 | timezoneDateString(timezone, 'YYYY/MM/DD HH:mm:ss z') }}
                  </em>
                  <br>
                  <strong>{{ session.srcIp }}</strong>:{{ session.srcPort }} <strong>{{ session.srcGEO }}</strong> -
                  <strong>{{ session.dstIp }}</strong>:{{ session.dstPort }} <strong>{{ session.dstGEO }}</strong>
                  <br>
                  <strong>{{ session.ipProtocol | protocol }}</strong> - {{ session.node }}
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
let stickyContainer;
let oldLength = 1;

export default {
  name: 'MolochStickySessions',
  props: [ 'sessions', 'timezone' ],
  data: function () {
    return {
      open: false,
      sortBy: '', // use the order sessions are opened
      sortOrder: 'desc'
    };
  },
  watch: {
    sessions: function (newVal, oldVal) {
      let newLength = newVal.length;

      // only sort changed, nothing to do
      if (newLength === oldLength) { return; }

      if (!newLength) {
        this.open = false;
        return;
      }

      if (newLength > oldLength) {
        if (!stickyContainer) {
          stickyContainer = this.$refs.stickyContainer;
        }

        stickyContainer.classList.remove('bounce');

        setTimeout(() => {
          stickyContainer.classList.add('bounce');
        });

        this.sort();
      }

      oldLength = newLength;
    }
  },
  methods: {
    /* exposed functions --------------------------------------------------- */
    /* Opens/closes the sticky sessions panel */
    toggleStickySessions: function () {
      this.open = !this.open;
    },
    /**
     * Orders the sessions by start or stop time
     * Triggered when sortBy/sortOrder is changed and when a session
     * is added to the sticky sessions list
     * Default sort order is the order in which the sessions were opened
     */
    sort: function () {
      if (this.sortBy) {
        this.sessions = this.sessions.sort((a, b) => {
          let result;
          if (this.sortOrder === 'desc') {
            result = b[this.sortBy] - a[this.sortBy];
          } else {
            result = a[this.sortBy] - b[this.sortBy];
          }
          return result;
        });
      }
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

      this.sort();
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
      window.location.hash = 'session' + id;
    }
  }
};
</script>

<style scoped>
.sticky-session-btn {
  width: 60px;
  display: block;
  position: fixed;
  top: 76px;
  right: 0;
  z-index: 5;
  margin-right: -30px;
  overflow: hidden;
  padding: 1px 10px 2px 12px;
  border-radius: 4px 0 0 4px;
  cursor: pointer;
  background-color: var(--color-quaternary);
  color: #FFFFFF;
}

.sort-by-select {
  width: 90px;
}

.sticky-session-detail {
  overflow-y: auto;
  position: fixed;
  top: 110px;
  right: 0;
  bottom: 0;
  z-index: 4;
  width: 360px;
  border-left: 1px solid var(--color-gray-light);
  background-color: var(--color-gray-lighter);

  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

.sticky-session-detail ul {
  margin-bottom: 0;
}

.sticky-session-detail .list-group-item {
  border-left : none;
  border-right: none;
  padding: 4px 8px;
  background-color: var(--color-background, #FFF);
  color: var(--color-foreground, #333);
}

.sticky-session-detail .list-group-item .list-group-item-text {
  line-height: 1.25;
}

a.list-group-item:hover,
a.list-group-item:focus {
  background-color: var(--color-tertiary-lightest);
}

.sticky-session-detail .list-group-item:last-child {
  border-bottom-right-radius: 0;
  border-bottom-left-radius: 0;
}

.sticky-session-detail .list-group-item.list-group-header {
  border-top-right-radius: 0;
  border-top-left-radius: 0;
  padding: 12px 8px;
  background-color: var(--color-gray-lighter);
}

/* ANIMATIONS ---------------------- */
/* bounce the sticky sessions button */
.bounce .sticky-session-btn {
  -webkit-animation: bounce 1000ms linear both;
     -moz-animation: bounce 1000ms linear both;
          animation: bounce 1000ms linear both;
}

/* animate sticky-session-detail and list-group-item slide in/out */
.slide-enter-active, .slide-leave-active {
  transition: all .5s ease;
}
.slide-enter, .slide-leave-to {
  transform: translateX(360px);
}
.list-group-item.list-group-item-animate {
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
    transform: translate3d(-20px, 0, 0);
  }
  70% {
    animation-timing-function: cubic-bezier(0.755, 0.05, 0.855, 0.06);
    transform: translate3d(-8px, 0, 0);
  }
  90% {
    transform: translate3d(-2px, 0, 0);
  }
}
</style>
