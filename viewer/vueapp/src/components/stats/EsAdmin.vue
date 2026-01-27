<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="container-fluid mt-3">
    <arkime-loading v-if="loading && !error" />

    <arkime-error
      v-if="error"
      :message="error" />

    <div v-if="!error">
      <h5 class="alert alert-warning">
        <span class="fa fa-exclamation-triangle me-1" />
        <span v-html="$t('stats.esAdmin.warningHtml')" />
      </h5>

      <div
        class="alert alert-danger"
        v-if="interactionError">
        <span class="fa fa-exclamation-triangle me-1" />
        <strong>{{ $t('common.error') }}:</strong>
        {{ interactionError }}
        <button
          type="button"
          class="btn-close pull-right"
          @click="interactionError = ''"
          data-dismiss="alert" />
      </div>

      <div
        class="alert alert-success"
        v-if="interactionSuccess">
        <span class="fa fa-check me-1" />
        <strong>{{ $t('common.success') }}:</strong>
        {{ interactionSuccess }}
        <button
          type="button"
          class="btn-close pull-right"
          @click="interactionSuccess = ''"
          data-dismiss="alert" />
      </div>

      <h3>
        {{ $t('stats.esAdmin.esClusterSettings') }}
        <span class="pull-right">
          <button
            type="button"
            @click="retryFailed"
            id="retryFailed"
            class="btn btn-theme-primary ms-1">
            {{ $t('stats.esAdmin.retryFailed') }}
            <BTooltip target="retryFailed"><span v-i18n-btip="'stats.esAdmin.'" /></BTooltip>
          </button>
          <button
            type="button"
            @click="flush"
            id="flush"
            class="btn btn-theme-secondary ms-1">
            {{ $t('stats.esAdmin.flush') }}
            <BTooltip target="flush"><span v-i18n-btip="'stats.esAdmin.'" /></BTooltip>
          </button>
          <button
            type="button"
            @click="unflood"
            id="unflood"
            class="btn btn-theme-tertiary ms-1">
            {{ $t('stats.esAdmin.unflood') }}
            <BTooltip target="unflood"><span v-i18n-btip="'stats.esAdmin.'" /></BTooltip>
          </button>
          <button
            type="button"
            @click="clearCache"
            id="clearCache"
            class="btn btn-theme-quaternary ms-1">
            {{ $t('stats.esAdmin.clearCache') }}
            <BTooltip target="clearCache"><span v-i18n-btip="'stats.esAdmin.'" /></BTooltip>
          </button>
        </span>
      </h3>

      <hr>

      <BRow
        v-for="setting in settings"
        :key="setting.key"
        class="mt-2">
        <BCol>
          <BInputGroup>
            <BInputGroupText :id="`setting-${setting.key}`">
              {{ setting.name }}
              <BTooltip :target="`setting-${setting.key}`">
                {{ setting.key }}
              </BTooltip>
            </BInputGroupText>
            <input
              type="text"
              @input="setting.changed = true"
              class="form-control"
              v-model="setting.current"
              :class="{'is-invalid':setting.error || (setting.key === 'cluster.routing.allocation.enable' && setting.current !== 'all')}">
            <BInputGroupText>
              {{ setting.type }}
              <small class="ms-2">
                (<a
                  :href="setting.url"
                  class="no-decoration"
                  target="_blank">
                  Learn more
                </a>)
              </small>
            </BInputGroupText>
            <button
              v-if="setting.key === 'cluster.routing.allocation.enable' && setting.current !== 'all'"
              type="button"
              @click="restoreToAll(setting)"
              :id="`restore-${setting.key}`"
              class="btn btn-warning">
              <span class="fa fa-undo" />
              <BTooltip
                :target="`restore-${setting.key}`">
                {{ $t('stats.esAdmin.restoreAllocationTip') }}
              </BTooltip>
            </button>
            <button
              type="button"
              :disabled="!setting.changed"
              @click="cancel(setting)"
              class="btn btn-warning">
              {{ $t('common.cancel') }}
            </button>
            <button
              type="button"
              :disabled="!setting.changed"
              @click="save(setting)"
              class="btn btn-theme-primary">
              {{ $t('common.save') }}
            </button>
          </BInputGroup>
          <div
            v-if="setting.error"
            class="form-text text-danger">
            <span class="fa fa-exclamation-triangle" />
            {{ setting.error }}
          </div>
        </BCol>
      </BRow>

      <div class="alert alert-info mt-1">
        <span class="fa fa-info-circle me-1" />
        <span v-html="$t('stats.esAdmin.controlHtml')" />
      </div>
    </div>
  </div>
</template>

<script>
import Utils from '../utils/utils';
import ArkimeError from '../utils/Error.vue';
import ArkimeLoading from '../utils/Loading.vue';
import StatsService from './StatsService.js';

export default {
  name: 'EsAdmin',
  props: {
    cluster: {
      type: String,
      default: ''
    }
  },
  components: {
    ArkimeError,
    ArkimeLoading
  },
  data: function () {
    return {
      loading: true,
      error: '',
      interactionError: '',
      interactionSuccess: '',
      settings: {},
      query: {
        cluster: this.cluster || undefined
      }
    };
  },
  created: function () {
    this.loadData();
  },
  watch: {
    cluster: function () {
      this.query.cluster = this.cluster;
      this.loadData();
    }
  },
  methods: {
    /* exposed page functions ------------------------------------ */
    async save (setting) {
      if (!setting.current.match(setting.regex)) {
        setting.error = `Invalid format, this setting must be: ${setting.type}`;
        return;
      }

      const selection = Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this);
      if (!selection.valid) {
        setting.error = selection.error;
        return;
      }

      const data = {
        key: setting.key,
        value: setting.current
      };

      try {
        await StatsService.setAdmin({ data, params: this.query });
        setting.error = '';
        setting.changed = false;
      } catch (error) {
        setting.error = error.text || String(error);
      }
    },
    async restoreToAll (setting) {
      setting.current = 'all';
      setting.changed = true;
      await this.save(setting);
    },
    async cancel (setting) {
      const selection = Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active);
      if (!selection.valid) {
        setting.error = selection.error;
        return;
      }

      try { // update the changed value with the one that's saved
        const response = await StatsService.getAdmin(this.query);
        setting.error = '';
        for (const resSetting of response) {
          if (resSetting.key === setting.key) {
            setting.current = resSetting.current;
            setting.changed = false;
          }
        }
      } catch (error) {
        setting.error = error.text || String(error);
      }
    },
    async clearCache () {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this, 'interactionError').valid) {
        return;
      }

      try {
        const response = await StatsService.clearCacheAdmin(this.query);
        this.interactionSuccess = response.text;
      } catch (error) {
        this.interactionError = error.text || String(error);
      }
    },
    async unflood () {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this, 'interactionError').valid) {
        return;
      }

      try {
        const response = await StatsService.unFloodAdmin(this.query);
        this.interactionSuccess = response.text;
      } catch (error) {
        this.interactionError = error.text || String(error);
      }
    },
    async flush () {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this, 'interactionError').valid) {
        return;
      }

      try {
        const response = await StatsService.flushAdmin(this.query);
        this.interactionSuccess = response.text;
      } catch (error) {
        this.interactionError = error.text || String(error);
      }
    },
    async retryFailed () {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this, 'interactionError').valid) {
        return;
      }

      try {
        const response = await StatsService.rerouteAdmin(this.query);
        this.interactionSuccess = response.text;
      } catch (error) {
        this.interactionError = error.text || String(error);
      }
    },
    /* helper functions ------------------------------------------ */
    async loadData () {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        return;
      }

      this.loading = true;

      try {
        const response = await StatsService.getAdmin(this.query);
        this.error = '';
        this.loading = false;
        this.settings = response;
      } catch (error) {
        this.error = error.text || String(error);
        this.loading = false;
      }
    }
  }
};
</script>
