<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <transition name="slide-fade">
    <v-card
      v-if="visible"
      variant="outlined"
      class="help-notes">
      <a
        href="javascript:void(0)"
        role="button"
        @click="close"
        class="no-decoration close-btn">
        <v-icon icon="mdi-close" />
        <v-tooltip activator="parent">{{ $t('helpNotes.closeTip') }}</v-tooltip>
      </a>
      <div class="pa-3">
        <div
          v-if="message.id === 'welcome'"
          class="text-center pe-4">
          <v-icon
            icon="mdi-heart"
            class="text-theme-accent me-1" />
          <strong class="text-theme-accent">
            {{ $t('welcome.greeting', { userName: user.userName }) }}
          </strong>
          {{ $t('welcome.checkOutOur') }}
          <a
            href="javascript:void(0)"
            @click="openHelp"
            class="no-decoration">
            {{ $t('common.help').toLowerCase() }}
          </a>
          {{ $t('welcome.pageInfo') }}
        </div>
        <div
          v-else-if="message.id === 'v7'"
          class="pe-4">
          <div class="text-center">
            <v-icon
              icon="mdi-star"
              class="text-theme-accent me-1" />
            <strong class="text-theme-accent">
              {{ $t('welcome.v7Title') }}
            </strong>
          </div>
          {{ $t('welcome.v7Intro') }}
          <ul class="v7-list ms-4 mt-1">
            <li
              v-for="n in 6"
              :key="n">
              {{ $t(`welcome.v7Item${n}`) }}
            </li>
          </ul>
        </div>
        <div
          v-else
          class="text-center pe-4">
          <v-icon
            icon="mdi-lightbulb-on-outline"
            class="text-theme-accent me-1" />
          {{ $t(`helpNotes.${message.id}.text`) }}
          <a
            href="javascript:void(0)"
            @click="openHelp"
            class="no-decoration">
            {{ $t('helpNotes.learnMore') }}
          </a>
        </div>
        <div class="d-flex justify-space-between align-center mt-2">
          <span class="d-flex align-center">
            <a
              href="javascript:void(0)"
              role="button"
              @click="prev"
              class="no-decoration">
              <v-icon icon="mdi-chevron-left" />
              <v-tooltip activator="parent">{{ $t('common.previous') }}</v-tooltip>
            </a>
            <span class="pager-count mx-1">{{ index + 1 }} / {{ messages.length }}</span>
            <a
              href="javascript:void(0)"
              role="button"
              @click="next"
              class="no-decoration">
              <v-icon icon="mdi-chevron-right" />
              <v-tooltip activator="parent">{{ $t('common.next') }}</v-tooltip>
            </a>
          </span>
          <span>
            <a
              href="javascript:void(0)"
              role="button"
              @click="dismiss"
              class="no-decoration">
              {{ $t('common.dismiss') }}
              <v-tooltip activator="parent">{{ $t('helpNotes.dismissTip') }}</v-tooltip>
            </a>
            <a
              href="javascript:void(0)"
              role="button"
              @click="dismissAll"
              class="no-decoration ms-3">
              {{ $t('helpNotes.dismissAll') }}
              <v-tooltip activator="parent">{{ $t('helpNotes.dismissAllTip') }}</v-tooltip>
            </a>
          </span>
        </div>
      </div>
    </v-card>
  </transition>
</template>

<script>
import UserService from '../users/UserService';
import helpMessages, { HIDE_ALL_NOTES } from './helpNotes';

export default {
  name: 'ArkimeHelpNotes',
  data: function () {
    return {
      closed: false,
      index: 0
    };
  },
  computed: {
    user: {
      get: function () {
        return this.$store.state.user;
      },
      set: function (newValue) {
        this.$store.commit('setUser', newValue);
      }
    },
    dismissed: function () {
      const ids = new Set(this.user.dismissedHelpNotes ?? []);
      // welcomeMsgNum predates id tracking: 1 = acked the welcome, 2 = acked the v7 notice too
      if (this.user.welcomeMsgNum >= 1) { ids.add('welcome'); }
      if (this.user.welcomeMsgNum >= 2) { ids.add('v7'); }
      return ids;
    },
    messages: function () {
      if (this.dismissed.has(HIDE_ALL_NOTES)) { return []; }
      return helpMessages.filter(m => !this.dismissed.has(m.id));
    },
    message: function () {
      return this.messages[this.index];
    },
    visible: function () {
      if (this.closed || !this.messages.length) { return false; }
      // tips only show on their own page; announcements (routeless, at the head) show anywhere
      return !this.messages[0].route || this.messages.some(m => m.route === this.$route.name);
    }
  },
  watch: {
    '$route.name': function () {
      this.aimAtRoute();
    }
  },
  mounted: function () {
    this.aimAtRoute();
  },
  methods: {
    // announcements lead; otherwise point at the current page's tip
    aimAtRoute: function () {
      if (this.messages.length && !this.messages[0].route) { return; }
      const i = this.messages.findIndex(m => m.route === this.$route.name);
      if (i > -1) { this.index = i; }
    },
    close: function () {
      this.closed = true;
    },
    prev: function () {
      this.index = (this.index - 1 + this.messages.length) % this.messages.length;
    },
    next: function () {
      this.index = (this.index + 1) % this.messages.length;
    },
    dismiss: function () {
      this.persistDismiss(this.message.id);
    },
    dismissAll: function () {
      this.persistDismiss(HIDE_ALL_NOTES);
    },
    persistDismiss: function (id) {
      UserService.dismissNote(id, this.user.userId)
        .then((response) => {
          this.user = {
            ...this.user,
            dismissedHelpNotes: [...(this.user.dismissedHelpNotes ?? []), id]
          };
          if (this.index >= this.messages.length) {
            this.index = 0;
          }
        })
        .catch((error) => {
          this.closed = true;
        });
    },
    openHelp: function () {
      this.$router.push({
        path: '/help',
        hash: this.message.anchor
      });
    }
  }
};
</script>

<style scoped>
/* themed help card: positioned bottom-left as a floating banner */
.help-notes {
  position: fixed;
  bottom: 15px;
  left: 10px;
  z-index: 999;
  width: 400px;
  font-size: 1.2rem;
  background-color: rgb(var(--v-theme-neutral-lighter)) !important;
  border: 1px solid rgb(var(--v-theme-neutral-light)) !important;
  box-shadow: 4px 4px 16px -2px black;
}

.close-btn {
  position: absolute;
  top: 4px;
  right: 6px;
}

.pager-count {
  font-size: 0.9rem;
}

.v7-list {
  font-size: 1.05rem;
}

/* slide/fade help card in/out */
.slide-fade-enter-active {
  transition: all .8s ease;
}
.slide-fade-leave-active {
  transition: all .8s ease;
}
.slide-fade-enter-from, .slide-fade-leave-to {
  transform: translateX(-410px);
  opacity: 0;
}
</style>
