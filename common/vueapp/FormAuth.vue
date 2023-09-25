<template>
  <div class="d-flex justify-content-center mt-4">
    <div class="form-auth text-center">

      <img src="assets/watching.gif" />

      <div class="well well-lg">

        <h1 class="text-theme-accent">Login!</h1>

        <b-form
          @keyup.enter="login">
          <b-input-group
            class="mb-2"
            prepend="Username">
            <b-form-input
              v-model="username"
              @keyup.enter="login"
              placeholder="Username"
              :state="!username ? false : true"
              required>
            </b-form-input>
          </b-input-group>
          <b-input-group
            class="mb-2"
            prepend="Password">
            <b-form-input
              type="password"
              v-model="password"
              @keyup.enter="login"
              placeholder="Password"
              :state="!password ? false : true"
              required>
            </b-form-input>
          </b-input-group>

          <b-overlay
            block
            rounded
            opacity="0.6"
            spinner-small
            :show="loading">
            <b-button
              block
              :disabled="loading"
              variant="primary"
              @click="login">
              Login
            </b-button>
          </b-overlay>

        </b-form>

      </div>

      <b-alert
        class="mt-2"
        dismissible
        :show="!!error">
        <span class="fa fa-exclamation-triangle mr-1" />
        {{ error }}
      </b-alert>

    </div>
  </div>
</template>

<script>
import UserService from './UserService';

export default {
  name: 'FormAuth',
  data () {
    return {
      error: '',
      username: '',
      password: '',
      loading: false
    };
  },
  methods: {
    login () {
      if (!this.username || !this.password) {
        this.error = 'Please enter a username and password.';
        return;
      }

      this.error = '';
      this.loading = true;

      UserService.login({ username: this.username, password: this.password }).then(() => {
        this.loading = false;
        this.$router.push({ path: '/' });
      }).catch((err) => {
        this.loading = false;
        this.error = err.text || err;
      });
    }
  }
};
</script>

<style scoped>
.form-auth {
  width: 34%;
}

img {
  margin-bottom: -6px;
}

.well {
  padding: 12px;
  border-radius: 3px;
  box-shadow: 4px 4px 10px 0 rgba(0,0,0,0.5);
}
</style>
