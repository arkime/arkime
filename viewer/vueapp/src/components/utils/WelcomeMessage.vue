<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>

  <transition name="slide-fade">
    <div v-if="!dismissed"
      class="card welcome-msg">
      <div class="card-body">
        <span class="fa fa-heart text-theme-accent">
        </span>
        <strong class="text-theme-accent">
          Welcome {{ user.userName }}!
        </strong>
        Check out our
        <a href="javascript:void(0)"
          @click="openHelp"
          class="no-decoration">
          help
        </a>
        page for more information,
        or click the owl on the top left.
        <br>
        <a href="#"
          role="button"
          @click="dismissMsg"
          class="no-decoration pull-left"
          title="But see this message again next time"
          v-b-tooltip.hover="'But see this message again next time'">
          <span class="fa fa-close" />
          Dismiss
        </a>
        <a href="#"
          role="button"
          @click="acknowledgeMsg"
          class="no-decoration pull-right"
          title="Don't show me this message again"
          v-b-tooltip.hover="'Don\'t show me this message again'">
          Got it!
          <span class="fa fa-thumbs-up" />
        </a>
      </div>
    </div>
  </transition>

</template>

<script>
import UserService from '../users/UserService';

export default {
  name: 'MolochWelcomeMessage',
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
.slide-fade-enter, .slide-fade-leave-to {
  transform: translateX(-343px);
  opacity: 0;
}
</style>
