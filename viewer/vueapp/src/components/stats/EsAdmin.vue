<template>

  <div class="container-fluid mt-3">

    <moloch-loading v-if="loading && !error">
    </moloch-loading>

    <moloch-error v-if="error"
      :message="error">
    </moloch-error>

    <div v-if="!error">

      <h5 class="alert alert-warning">
        <span class="fa fa-exclamation-triangle mr-1">
        </span>
        <strong>Warning!</strong>
        This stuff is dangerous and you can destroy your ES cluster.
        <strong>Be very careful.</strong>
      </h5>

      <div class="alert alert-info">
        <span class="fa fa-info-circle mr-1">
        </span>
        You can control which users see this page by setting
        <code>esAdminUsers=</code> in your <code>config.ini</code>.
      </div>

      <div class="alert alert-danger"
        v-if="interactionError">
        <span class="fa fa-exclamation-triangle mr-1">
        </span>
        <strong>Error:</strong>
        {{ interactionError }}
        <button type="button"
          class="close"
          @click="interactionError = ''"
          data-dismiss="alert">
          <span>&times;</span>
        </button>
      </div>

      <div class="alert alert-success"
        v-if="interactionSuccess">
        <span class="fa fa-check mr-1">
        </span>
        <strong>Success:</strong>
        {{ interactionSuccess }}
        <button type="button"
          class="close"
          @click="interactionSuccess = ''"
          data-dismiss="alert">
          <span class="fa fa-check">
          </span>
        </button>
      </div>

      <h3>
        ES Settings
        <span class="pull-right">
          <button type="button"
            @click="cancel"
            class="btn btn-warning">
            Cancel
          </button>
          <button type="button"
            @click="save"
            class="btn btn-theme-primary">
            Save
          </button>
          <button type="button"
            @click="retryFailed"
            class="btn btn-theme-tertiary">
            Retry Failed
          </button>
          <button type="button"
            @click="flush"
            class="btn btn-theme-secondary">
            Flush
          </button>
        </span>
      </h3>

      <hr>

      <div v-for="setting in settings"
        :key="setting.key"
        class="form-group row">
        <div class="col">
          <div class="input-group">
            <div class="input-group-prepend">
              <span class="input-group-text">
                {{ setting.name }}
              </span>
            </div>
            <input type="text"
              class="form-control"
              v-model="setting.current"
            />
            <div class="input-group-append">
              <span class="input-group-text">
                Default =&nbsp;
                {{ setting.default }}
              </span>
            </div>
          </div>
          <small class="form-text text-muted">
            <span class="fa fa-info-circle">
            </span>
            <strong>
              {{ setting.key }}:
            </strong>
            <a :href="setting.url"
              class="no-decoration"
              target="_blank">
              Learn more
            </a>
          </small>
        </div>
      </div>

    </div>

  </div>

</template>

<script>
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';

export default {
  name: 'EsAdmin',
  components: { MolochError, MolochLoading },
  data: function () {
    return {
      loading: true,
      error: '',
      interactionError: '',
      interactionSuccess: '',
      settings: {}
    };
  },
  created: function () {
    this.loadData();
  },
  methods: {
    /* exposed page functions ------------------------------------ */
    save: function () {
      // TODO
    },
    cancel: function () {
      // TODO
    },
    flush: function () {
      this.$http.post('esadmin/flush')
        .then((response) => {
          this.interactionSuccess = response.data.text;
        })
        .catch((error) => {
          this.interactionError = error;
        });
    },
    retryFailed: function () {
      this.$http.post('esadmin/reroute')
        .then((response) => {
          this.interactionSuccess = response.data.text;
        })
        .catch((error) => {
          this.interactionError = error;
        });
    },
    /* helper functions ------------------------------------------ */
    loadData: function () {
      this.loading = true;

      this.$http.get('esadmin/list')
        .then((response) => {
          this.error = '';
          this.loading = false;
          this.settings = response.data;
        }, (error) => {
          this.error = error.text || error;
          this.loading = false;
        });
    }
  }
};
</script>
