<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <form id="banner">
    <h3>{{ $t('settings.banner.title') }}</h3>
    <p>{{ $t('settings.banner.info') }}</p>

    <hr>

    <!-- enabled -->
    <v-row>
      <v-col
        tag="label"
        cols="12"
        sm="3"
        class="text-end font-weight-bold align-self-center">
        {{ $t('settings.banner.enabled') }}
      </v-col>
      <v-col
        cols="12"
        sm="9"
        class="align-self-center">
        <v-switch
          v-model="banner.enabled"
          color="primary"
          density="compact"
          hide-details
          :label="banner.enabled ? $t('settings.banner.on') : $t('settings.banner.off')" />
      </v-col>
    </v-row> <!-- /enabled -->

    <!-- type -->
    <v-row>
      <v-col
        tag="label"
        cols="12"
        sm="3"
        class="text-end font-weight-bold align-self-center">
        {{ $t('settings.banner.type') }}
      </v-col>
      <v-col
        cols="12"
        sm="9">
        <v-btn-toggle
          v-model="banner.type"
          density="compact"
          divided
          variant="outlined"
          color="secondary"
          class="d-inline-flex"
          mandatory>
          <v-btn value="info">
            {{ $t('settings.banner.typeInfo') }}
          </v-btn>
          <v-btn value="warning">
            {{ $t('settings.banner.typeWarning') }}
          </v-btn>
          <v-btn value="error">
            {{ $t('settings.banner.typeError') }}
          </v-btn>
        </v-btn-toggle>
      </v-col>
    </v-row> <!-- /type -->

    <!-- effects (combinable) -->
    <v-row>
      <v-col
        tag="label"
        cols="12"
        sm="3"
        class="text-end font-weight-bold align-self-center">
        {{ $t('settings.banner.effect') }}
      </v-col>
      <v-col
        cols="12"
        sm="9"
        class="align-self-center">
        <div class="d-flex flex-wrap align-center gap-4">
          <v-checkbox
            v-model="banner.effects"
            value="marquee"
            density="compact"
            hide-details
            :label="$t('settings.banner.effectMarquee')" />
          <v-checkbox
            v-model="banner.effects"
            value="blink"
            density="compact"
            hide-details
            :label="$t('settings.banner.effectBlink')" />
          <v-checkbox
            v-model="banner.effects"
            value="rainbow"
            density="compact"
            hide-details
            :label="$t('settings.banner.effectRainbow')" />
        </div>
      </v-col>
    </v-row> <!-- /effects -->

    <!-- message -->
    <v-row>
      <v-col
        tag="label"
        cols="12"
        sm="3"
        class="text-end font-weight-bold align-self-center">
        {{ $t('settings.banner.message') }}
      </v-col>
      <v-col
        cols="12"
        sm="9">
        <v-textarea
          v-model="banner.message"
          variant="outlined"
          density="compact"
          rows="3"
          auto-grow
          counter="1000"
          :maxlength="1000"
          :placeholder="$t('settings.banner.messagePlaceholder')" />
      </v-col>
    </v-row> <!-- /message -->

    <!-- show until -->
    <v-row>
      <v-col
        tag="label"
        cols="12"
        sm="3"
        class="text-end font-weight-bold align-self-center">
        {{ $t('settings.banner.showUntil') }}
      </v-col>
      <v-col
        cols="12"
        sm="9"
        class="align-self-center">
        <v-text-field
          :model-value="expiryDisplay"
          readonly
          clearable
          density="compact"
          variant="outlined"
          hide-details
          prepend-inner-icon="mdi-calendar-clock"
          :placeholder="$t('settings.banner.showUntilPlaceholder')"
          class="banner-datetime"
          @click:clear="banner.expires = 0">
          <v-menu
            activator="parent"
            :close-on-content-click="false">
            <v-card class="pb-2">
              <v-date-picker
                :model-value="expiryDate"
                show-adjacent-months
                @update:model-value="onPickDate" />
              <div class="d-flex align-center px-4 pt-1">
                <v-text-field
                  :model-value="expiryTime"
                  type="time"
                  density="compact"
                  variant="outlined"
                  hide-details
                  :label="$t('settings.banner.timeLabel')"
                  style="max-width: 160px;"
                  @update:model-value="onPickTime" />
              </div>
            </v-card>
          </v-menu>
        </v-text-field>
        <div class="text-medium-emphasis small mt-1">
          {{ $t('settings.banner.showUntilTip') }}
        </div>
      </v-col>
    </v-row> <!-- /show until -->

    <!-- live preview -->
    <v-row v-if="banner.message">
      <v-col
        cols="12"
        sm="3"
        class="text-end font-weight-bold align-self-center">
        {{ $t('settings.banner.preview') }}
      </v-col>
      <v-col
        cols="12"
        sm="9">
        <v-alert
          :type="banner.type"
          variant="flat"
          density="compact"
          class="rounded-0">
          <banner-message
            :message="banner.message"
            :effects="banner.effects" />
        </v-alert>
      </v-col>
    </v-row> <!-- /live preview -->

    <!-- save -->
    <v-row>
      <v-col
        cols="12"
        sm="3" />
      <v-col
        cols="12"
        sm="9">
        <v-btn
          variant="flat"
          size="large"
          color="primary"
          :loading="saving"
          @click="save">
          <v-icon
            icon="mdi-content-save"
            class="me-2" />
          {{ $t('common.save') }}
        </v-btn>
      </v-col>
    </v-row> <!-- /save -->
  </form>
</template>

<script>
import SettingsService from './SettingsService';
import BannerMessage from '../utils/BannerMessage.vue';

const pad = (n) => String(n).padStart(2, '0');

export default {
  name: 'BannerSettings',
  components: { BannerMessage },
  emits: ['display-message'],
  data () {
    return {
      saving: false,
      banner: { enabled: false, message: '', type: 'info', effects: [], expires: 0 }
    };
  },
  computed: {
    expiryDate () {
      return this.banner.expires ? new Date(this.banner.expires) : null;
    },
    expiryTime () {
      if (!this.banner.expires) { return ''; }
      const d = new Date(this.banner.expires);
      return `${pad(d.getHours())}:${pad(d.getMinutes())}`;
    },
    expiryDisplay () {
      if (!this.banner.expires) { return ''; }
      const d = new Date(this.banner.expires);
      return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}`;
    }
  },
  created () {
    // seed from the store, then refresh from the server
    this.seed(this.$store.state.banner || {});
    SettingsService.getBanner()
      .then((banner) => { this.seed(banner); })
      .catch(() => { /* keep store-seeded values */ });
  },
  methods: {
    seed (src) {
      this.banner = {
        enabled: !!src.enabled,
        message: src.message || '',
        type: src.type || 'info',
        effects: Array.isArray(src.effects) ? [...src.effects] : [],
        expires: src.expires || 0
      };
    },
    applyExpiry (date, time) {
      if (!date) { this.banner.expires = 0; return; }
      const d = new Date(date);
      const [h, m] = (time || '00:00').split(':');
      d.setHours(Number(h) || 0, Number(m) || 0, 0, 0);
      this.banner.expires = d.getTime();
    },
    onPickDate (date) {
      // default to end-of-day when only a date is chosen
      this.applyExpiry(date, this.expiryTime || '23:59');
    },
    onPickTime (time) {
      this.applyExpiry(this.expiryDate || new Date(), time);
    },
    save () {
      this.saving = true;
      SettingsService.updateBanner(this.banner).then((response) => {
        this.$emit('display-message', { msg: response.text });
      }).catch((error) => {
        this.$emit('display-message', { msg: error.text, type: 'danger' });
      }).finally(() => {
        this.saving = false;
      });
    }
  }
};
</script>

<style scoped>
.banner-datetime {
  max-width: 260px;
}
</style>
