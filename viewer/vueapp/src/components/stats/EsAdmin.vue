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
        ES Cluster Settings
        <span class="pull-right">
          <button type="button"
            @click="retryFailed"
            v-b-tooltip.hover
            title="Try to restart any shard migrations that have failed or paused"
            class="btn btn-theme-primary">
            Retry Failed
          </button>
          <button type="button"
            @click="flush"
            v-b-tooltip.hover
            title="Flush and refresh any data waiting in Elasticsearch to disk"
            class="btn btn-theme-secondary">
            Flush
          </button>
          <button type="button"
            @click="unflood"
            v-b-tooltip.hover
            title="Try and clear any indices marked as flooded"
            class="btn btn-theme-tertiary">
            Unflood
          </button>
          <button type="button"
            @click="clearCache"
            v-b-tooltip.hover
            title="Try and clear the cache for all indices"
            class="btn btn-theme-quaternary">
            Clear Cache
          </button>
        </span>
      </h3>

      <hr>

      <div v-for="setting in settings"
        :key="setting.key"
        class="form-group row">
        <div class="col">
          <div class="input-group">
            <div class="input-group-prepend cursor-help">
              <span class="input-group-text"
                v-b-tooltip.hover
                :title="setting.key">
                {{ setting.name }}
              </span>
            </div>
            <input type="text"
              @input="setting.changed = true"
              class="form-control"
              v-model="setting.current"
              :class="{'is-invalid':setting.error}"
            />
            <div class="input-group-append">
              <span class="input-group-text">
                {{ setting.type }}
                <small class="ml-2">
                  (<a :href="setting.url"
                    class="no-decoration"
                    target="_blank">
                    Learn more
                  </a>)
                </small>
              </span>
              <button type="button"
                :disabled="!setting.changed"
                @click="cancel(setting)"
                class="btn btn-warning">
                Cancel
              </button>
              <button type="button"
                :disabled="!setting.changed"
                @click="save(setting)"
                class="btn btn-theme-primary">
                Save
              </button>
            </div>
          </div>
          <div v-if="setting.error"
            class="form-text text-danger">
            <span class="fa fa-exclamation-triangle">
            </span>
            {{ setting.error }}
          </div>
        </div>
      </div>

      <div class="alert alert-info">
        <span class="fa fa-info-circle mr-1">
        </span>
        You can control which users see this page by setting
        <code>esAdminUsers=</code> in your <code>config.ini</code>.
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
    save: function (setting) {
      if (!setting.current.match(setting.regex)) {
        this.$set(setting, 'error', `Invalid format, this setting must be: ${setting.type}`);
        return;
      }

      const body = {
        key: setting.key,
        value: setting.current
      };

      this.$http.post('api/esadmin/set', body)
        .then((response) => {
          this.$set(setting, 'error', '');
          this.$set(setting, 'changed', false);
        }, (error) => {
          this.$set(setting, 'error', error.text || error);
        });
    },
    cancel: function (setting) {
      // update the changed value with the one that's saved
      this.$http.get('api/esadmin')
        .then((response) => {
          this.$set(setting, 'error', '');
          for (const resSetting of response.data) {
            if (resSetting.key === setting.key) {
              this.$set(setting, 'current', resSetting.current);
              this.$set(setting, 'changed', false);
            }
          }
        }, (error) => {
          this.$set(setting, 'error', error.text || error);
        });
    },
    clearCache: function () {
      this.$http.post('api/esadmin/clearcache')
        .then((response) => {
          this.interactionSuccess = response.data.text;
        })
        .catch((error) => {
          this.interactionError = error;
        });
    },
    unflood: function () {
      this.$http.post('api/esadmin/unflood')
        .then((response) => {
          this.interactionSuccess = response.data.text;
        })
        .catch((error) => {
          this.interactionError = error;
        });
    },
    flush: function () {
      this.$http.post('api/esadmin/flush')
        .then((response) => {
          this.interactionSuccess = response.data.text;
        })
        .catch((error) => {
          this.interactionError = error;
        });
    },
    retryFailed: function () {
      this.$http.post('api/esadmin/reroute')
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

      this.$http.get('api/esadmin')
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
