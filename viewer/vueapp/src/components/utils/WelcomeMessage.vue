<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <transition name="slide-fade">
    <div
      v-if="!dismissed"
      class="card welcome-msg">
      <div class="card-body">
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
        <a
          href="#"
          role="button"
          @click="dismissMsg"
          id="dismissWelcomeMsg"
          class="no-decoration pull-left ms-1">
          <span class="fa fa-close" />
          {{ $t('common.dismiss') }}
          <BTooltip target="dismissWelcomeMsg">{{ $t('welcome.dismissTip') }}</BTooltip>
        </a>
        <a
          href="#"
          role="button"
          @click="acknowledgeMsg"
          id="acknowledgeWelcomeMsg"
          class="no-decoration pull-right me-1">
          {{ $t('welcome.gotIt') }}
          <span class="fa fa-thumbs-up" />
          <BTooltip target="acknowledgeWelcomeMsg">{{ $t('welcome.gotItTip') }}</BTooltip>
        </a>
      </div>
    </div>
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
.welcome-msg {
  position: fixed;
  bottom: 15px;
  left: 10px;
  z-index: 999;
  width: 333px;
  text-align: center;
  font-size: 1.2rem;
}

/* apply theme color to welcome card */
.card {
  background-color: var(--color-gray-lighter);
  border: 1px solid var(--color-gray-light);
  -webkit-box-shadow: 4px 4px 16px -2px black;
     -moz-box-shadow: 4px 4px 16px -2px black;
          box-shadow: 4px 4px 16px -2px black;
}
.card > .card-body {
  padding: 0.8rem;
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
