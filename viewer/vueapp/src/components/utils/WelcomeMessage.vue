<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <transition name="slide-fade">
    <v-card
      v-if="!dismissed"
      variant="outlined"
      class="welcome-msg">
      <div class="pa-3 text-center">
        <span class="fa fa-heart text-theme-accent me-1" />
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
        <br>
        <div class="d-flex justify-space-between align-center mt-1">
          <a
            href="#"
            role="button"
            @click="dismissMsg"
            class="no-decoration ms-1">
            <span class="fa fa-close" />
            {{ $t('common.dismiss') }}
            <v-tooltip activator="parent">{{ $t('welcome.dismissTip') }}</v-tooltip>
          </a>
          <a
            href="#"
            role="button"
            @click="acknowledgeMsg"
            class="no-decoration me-1">
            {{ $t('welcome.gotIt') }}
            <span class="fa fa-thumbs-up" />
            <v-tooltip activator="parent">{{ $t('welcome.gotItTip') }}</v-tooltip>
          </a>
        </div>
      </div>
    </v-card>
  </transition>
</template>

<script>
import UserService from '../users/UserService';

export default {
  name: 'ArkimeWelcomeMessage',
  data: function () {
    return {
      dismissed: false
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
    }
  },
  methods: {
    dismissMsg: function (e) {
      this.dismissed = true;
      e.preventDefault();
    },
    acknowledgeMsg: function (e) {
      UserService.acknowledgeMsg(1, this.user.userId)
        .then((response) => {
          this.user.msgNum = 1;
          this.dismissed = true;
        })
        .catch((error) => {
          this.dismissed = true;
        });

      e.preventDefault();
    },
    openHelp: function () {
      this.$router.push({
        path: '/help'
      });
    }
  }
};
</script>

<style scoped>
/* themed welcome card: positioned bottom-left as a floating banner */
.welcome-msg {
  position: fixed;
  bottom: 15px;
  left: 10px;
  z-index: 999;
  width: 333px;
  font-size: 1.2rem;
  background-color: rgb(var(--v-theme-neutral-lighter)) !important;
  border: 1px solid rgb(var(--v-theme-neutral-light)) !important;
  box-shadow: 4px 4px 16px -2px black;
}

/* slide/fade welcome message in/out */
.slide-fade-enter-active {
  transition: all .8s ease;
}
.slide-fade-leave-active {
  transition: all .8s ease;
}
.slide-fade-enter-from, .slide-fade-leave-to {
  transform: translateX(-343px);
  opacity: 0;
}
</style>
