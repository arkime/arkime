<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="d-flex align-center gap-1 text-start">
    <!-- single time control: shows the current range, opens the breakout popover -->
    <v-btn
      id="timeOptionsBtn"
      variant="flat"
      size="small"
      density="comfortable"
      class="time-options-btn"
      :style="secondaryBtnStyle"
      v-focus="focusTimeRange"
      @blur="onOffTimeRangeFocus">
      <template v-if="timeError">
        <v-icon icon="mdi-alert" />
      </template>
      <template v-else>
        <v-icon
          icon="mdi-clock-outline"
          v-if="!shiftKeyHold"
          class="me-1" />
        <span
          v-else
          class="time-shortcut me-1">
          T
        </span>
        {{ currentRangeLabel }}
      </template>
      <v-icon
        icon="mdi-menu-down"
        class="ms-1" />
      <v-tooltip
        activator="#timeOptionsBtn"
        location="bottom"
        :open-delay="500">
        {{ timeError || $t('search.timeOptionsTip') }}
      </v-tooltip>
      <v-menu
        activator="parent"
        location="bottom start"
        v-model="menuOpen"
        :close-on-content-click="false">
        <v-card class="arkime-time-popover pa-3">
          <!-- validation error (full text lives here + on the button tooltip) -->
          <div
            v-if="timeError"
            class="arkime-time-error mb-3">
            <v-icon
              icon="mdi-alert"
              class="me-1" />
            {{ timeError }}
          </div>

          <!-- section 1: quick-range presets -->
          <div class="d-flex flex-wrap ga-1">
            <v-chip
              v-for="p in presets"
              :key="p.value"
              size="small"
              :variant="isPresetActive(p) ? 'flat' : 'tonal'"
              :color="isPresetActive(p) ? 'primary' : undefined"
              @click="selectPreset(p.value)">
              {{ p.label }}
            </v-chip>
          </div>
          <v-divider class="my-3" />

          <!-- section 2: custom start / stop date inputs -->
          <div class="d-flex flex-wrap ga-4">
            <!-- start time -->
            <div class="arkime-time-col">
              <div class="arkime-input-group arkime-input-group--fluid mb-2">
                <span
                  id="startTimeLabel"
                  class="arkime-input-label cursor-help">
                  {{ $t('search.startTime') }}
                  <v-tooltip
                    activator="#startTimeLabel"
                    location="bottom"
                    :open-delay="500">
                    {{ $t('search.startTimeTip') }}
                  </v-tooltip>
                </span>
                <input
                  type="text"
                  id="startTime"
                  ref="startTime"
                  name="startTime"
                  class="arkime-input-control"
                  placeholder="YYYY/MM/DD HH:mm:ss"
                  @input="changeStartTime"
                  :value="typedStartTime">
                <span
                  v-if="timezone !== 'local'"
                  id="startTimeTimezone"
                  class="arkime-input-label cursor-help">
                  {{ timezone === 'gmt' ? 'UTC' : getTimezoneShort() }}
                  <v-tooltip
                    activator="#startTimeTimezone"
                    location="bottom"
                    :open-delay="500">
                    {{ timezone === 'gmt' ? 'UTC' : Intl.DateTimeFormat().resolvedOptions().timeZone }}
                    {{ timezone === 'gmt' ? new Date().getTimezoneOffset() / -60 + ':00' : '' }}
                  </v-tooltip>
                </span>
                <v-btn
                  id="prevStartTime"
                  variant="text"
                  size="small"
                  density="comfortable"
                  icon
                  class="arkime-input-append-btn"
                  @click="prevTime('start')">
                  <v-icon icon="mdi-skip-previous" />
                  <v-tooltip
                    activator="#prevStartTime"
                    location="bottom"
                    :open-delay="500">
                    {{ $t('search.prevStartTimeTip') }}
                  </v-tooltip>
                </v-btn>
                <v-btn
                  id="nextStartTime"
                  variant="text"
                  size="small"
                  density="comfortable"
                  icon
                  class="arkime-input-append-btn"
                  @click="nextTime('start')">
                  <v-icon icon="mdi-skip-next" />
                  <v-tooltip
                    activator="#nextStartTime"
                    location="bottom"
                    :open-delay="500">
                    {{ $t('search.nextStartTimeTip') }}
                  </v-tooltip>
                </v-btn>
              </div>
              <v-date-picker
                v-if="localStartTime"
                hide-header
                :model-value="localStartTime.toDate()"
                show-adjacent-months
                @update:model-value="applyStartDate" />
              <!-- start time-of-day picker -->
              <div
                v-if="localStartTime"
                class="arkime-input-group arkime-input-group--fluid mt-2">
                <span class="arkime-input-label arkime-input-label-fw">
                  <v-icon icon="mdi-clock-outline" />
                </span>
                <input
                  type="time"
                  step="1"
                  class="arkime-input-control"
                  :value="localStartTime.format('HH:mm:ss')"
                  @input="changeTimeOfDay('start', $event)">
              </div>
            </div> <!-- /start time -->

            <!-- stop time -->
            <div class="arkime-time-col">
              <div class="arkime-input-group arkime-input-group--fluid mb-2">
                <span
                  id="stopTimeLabel"
                  class="arkime-input-label cursor-help">
                  {{ $t('search.stopTime') }}
                  <v-tooltip
                    activator="#stopTimeLabel"
                    location="bottom"
                    :open-delay="500">
                    {{ $t('search.stopTimeTip') }}
                  </v-tooltip>
                </span>
                <input
                  type="text"
                  id="stopTime"
                  ref="stopTime"
                  name="stopTime"
                  class="arkime-input-control"
                  placeholder="YYYY/MM/DD HH:mm:ss"
                  @input="changeStopTime"
                  :value="typedStopTime">
                <span
                  v-if="timezone !== 'local'"
                  id="stopTimeTimezone"
                  class="arkime-input-label cursor-help">
                  {{ timezone === 'gmt' ? 'UTC' : getTimezoneShort() }}
                  <v-tooltip
                    activator="#stopTimeTimezone"
                    location="bottom"
                    :open-delay="500">
                    {{ timezone === 'gmt' ? 'UTC' : Intl.DateTimeFormat().resolvedOptions().timeZone }}
                    {{ timezone === 'gmt' ? new Date().getTimezoneOffset() / -60 + ':00' : '' }}
                  </v-tooltip>
                </span>
                <v-btn
                  id="prevStopTime"
                  variant="text"
                  size="small"
                  density="comfortable"
                  icon
                  class="arkime-input-append-btn"
                  @click="prevTime('stop')">
                  <v-icon icon="mdi-skip-previous" />
                  <v-tooltip
                    activator="#prevStopTime"
                    location="bottom"
                    :open-delay="500">
                    {{ $t('search.prevStopTimeTip') }}
                  </v-tooltip>
                </v-btn>
                <v-btn
                  id="nextStopTime"
                  variant="text"
                  size="small"
                  density="comfortable"
                  icon
                  class="arkime-input-append-btn"
                  @click="nextTime('stop')">
                  <v-icon icon="mdi-skip-next" />
                  <v-tooltip
                    activator="#nextStopTime"
                    location="bottom"
                    :open-delay="500">
                    {{ $t('search.nextStopTimeTip') }}
                  </v-tooltip>
                </v-btn>
              </div>
              <v-date-picker
                v-if="localStopTime"
                hide-header
                :model-value="localStopTime.toDate()"
                show-adjacent-months
                @update:model-value="applyStopDate" />
              <!-- stop time-of-day picker -->
              <div
                v-if="localStopTime"
                class="arkime-input-group arkime-input-group--fluid mt-2">
                <span class="arkime-input-label arkime-input-label-fw">
                  <v-icon icon="mdi-clock-outline" />
                </span>
                <input
                  type="time"
                  step="1"
                  class="arkime-input-control"
                  :value="localStopTime.format('HH:mm:ss')"
                  @input="changeTimeOfDay('stop', $event)">
              </div>
            </div> <!-- /stop time -->
          </div>

          <!-- section 3: bounding + interval selects -->
          <v-divider
            v-if="showBoundingInterval"
            class="my-3" />
          <div
            v-if="showBoundingInterval"
            class="d-flex flex-wrap ga-2">
            <!-- time bounding select -->
            <div
              class="arkime-input-group"
              v-if="!hideBounding">
              <span
                id="timeBounding"
                class="arkime-input-label cursor-help">
                {{ $t('search.timeBounding') }}
                <v-tooltip
                  activator="#timeBounding"
                  location="bottom"
                  :open-delay="500">
                  {{ $t('search.timeBoundingTip') }}
                </v-tooltip>
              </span>
              <select
                class="arkime-input-control"
                v-model="timeBounding"
                @change="changeTimeBounding">
                <option
                  value="first"
                  v-i18n-value="'search.timeBounding-'" />
                <option
                  value="last"
                  v-i18n-value="'search.timeBounding-'" />
                <option
                  value="both"
                  v-i18n-value="'search.timeBounding-'" />
                <option
                  value="either"
                  v-i18n-value="'search.timeBounding-'" />
                <option
                  value="database"
                  v-i18n-value="'search.timeBounding-'" />
              </select>
            </div> <!-- /time bounding select -->

            <!-- time interval select -->
            <div
              class="arkime-input-group"
              v-if="!hideInterval">
              <span
                id="timeInterval"
                class="arkime-input-label cursor-help">
                {{ $t('search.timeInterval') }}
                <v-tooltip
                  activator="#timeInterval"
                  location="bottom"
                  :open-delay="500">
                  {{ $t('search.timeIntervalTip') }}
                </v-tooltip>
              </span>
              <select
                class="arkime-input-control"
                v-model="timeInterval"
                @change="changeTimeInterval">
                <option
                  value="auto"
                  v-i18n-value="'search.timeInterval-'" />
                <option
                  value="second"
                  v-i18n-value="'search.timeInterval-'" />
                <option
                  value="minute"
                  v-i18n-value="'search.timeInterval-'" />
                <option
                  value="hour"
                  v-i18n-value="'search.timeInterval-'" />
                <option
                  value="day"
                  v-i18n-value="'search.timeInterval-'" />
                <option
                  value="week"
                  v-i18n-value="'search.timeInterval-'" />
              </select>
            </div> <!-- /time interval select -->
          </div>
        </v-card>
      </v-menu>
    </v-btn> <!-- /time options dropdown button -->
  </div>
</template>

<script>
import Focus from '@common/Focus.vue';
import { readableTime } from '@common/vueFilters.js';

import qs from 'qs';
import moment from 'moment-timezone';

// displayed with slashes to match the rest of the UI; accept dashes too when parsing
const timeDisplayFormat = 'YYYY/MM/DD HH:mm:ss';
const timeParseFormats = [
  timeDisplayFormat, 'YYYY/MM/DD HH:mm', 'YYYY/MM/DD',
  'YYYY-MM-DD HH:mm:ss', 'YYYY-MM-DD HH:mm', 'YYYY-MM-DD',
  moment.ISO_8601
];

// maps common timezone abbreviations to utc offsets for typed/pasted times;
// ambiguous abbreviations (CST, IST, AST, ...) use the most common meaning
const TZ_ABBREVIATIONS = {
  Z: '+00:00', UT: '+00:00', UTC: '+00:00', GMT: '+00:00',
  EST: '-05:00', EDT: '-04:00', CST: '-06:00', CDT: '-05:00',
  MST: '-07:00', MDT: '-06:00', PST: '-08:00', PDT: '-07:00',
  AKST: '-09:00', AKDT: '-08:00', HST: '-10:00', HDT: '-09:00',
  AST: '-04:00', ADT: '-03:00', NST: '-03:30', NDT: '-02:30',
  WET: '+00:00', WEST: '+01:00', BST: '+01:00', CET: '+01:00', CEST: '+02:00',
  EET: '+02:00', EEST: '+03:00', MSK: '+03:00', IST: '+05:30',
  HKT: '+08:00', SGT: '+08:00', JST: '+09:00', KST: '+09:00',
  AWST: '+08:00', ACST: '+09:30', ACDT: '+10:30', AEST: '+10:00', AEDT: '+11:00',
  NZST: '+12:00', NZDT: '+13:00'
};

const hourSec = 3600;
let currentTimeSec;
let dateChanged = false;
let startDateCheck;
let stopDateCheck;

export default {
  name: 'ArkimeTime',
  emits: ['timeChange'],
  directives: { Focus },
  props: {
    timezone: {
      type: String,
      default: 'local'
    },
    hideBounding: {
      type: Boolean,
      default: false
    },
    hideInterval: {
      type: Boolean,
      default: false
    },
    updateTime: {
      type: String,
      default: 'false'
    }
  },
  data: function () {
    return {
      timeError: '',
      timeBounding: this.$route.query.bounding || 'last',
      timeInterval: this.$route.query.interval || 'auto',
      // use start/stop time localized to this component so that the time
      // watcher can compare time values to local (unaffected) start/stop times
      localStopTime: undefined,
      localStartTime: undefined,
      // typedStart/typedStop hold the raw input string so cursor position is
      // preserved as the analyst types. Re-binding :value to a fresh format()
      // result on every keystroke would otherwise reset the caret to the end.
      typedStartTime: '',
      typedStopTime: '',
      // controls the time-options popover so picking a preset can close it
      menuOpen: false,
      // Arkime theme-color v-btn style for the time-options button. Vuetify
      // :color can't take CSS vars, so apply the secondary token inline to
      // match the other search-row buttons (Views, etc.).
      secondaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-secondary))',
        color: 'rgb(var(--v-theme-button-fg))'
      }
    };
  },
  computed: {
    user: function () {
      return this.$store.state.user;
    },
    // true when the time range is a custom start/stop window (not a preset)
    isCustomRange: function () {
      return parseFloat(this.timeRange) === 0;
    },
    // readable duration of the current window, e.g. "6 hours"
    rangeLabel: function () {
      const start = parseInt(this.time.startTime, 10);
      const stop = parseInt(this.time.stopTime, 10);
      if (isNaN(start) || isNaN(stop)) { return ''; }
      return readableTime((stop - start) * 1000);
    },
    // quick-pick time ranges shown as chips in the popover. Single source of
    // truth for both the chips and the button's current-range label. `min` is
    // the hours the user's timeLimit must allow; gated like the old <option>s.
    presets: function () {
      const limit = this.user.timeLimit;
      const ranges = [
        { value: '0.25', label: this.$t('common.minuteCount', 15), min: 0 },
        { value: '0.5', label: this.$t('common.minuteCount', 30), min: 0 },
        { value: '1', label: this.$t('common.hourCount', 1), min: 0 },
        { value: '6', label: this.$t('common.hourCount', 6), min: 6 },
        { value: '24', label: this.$t('common.hourCount', 24), min: 24 },
        { value: '48', label: this.$t('common.hourCount', 48), min: 48 },
        { value: '72', label: this.$t('common.hourCount', 72), min: 72 },
        { value: '168', label: this.$t('common.weekCount', 1), min: 168 },
        { value: '336', label: this.$t('common.weekCount', 2), min: 336 },
        { value: '720', label: this.$t('common.monthCount', 1), min: 720 },
        { value: '1440', label: this.$t('common.monthCount', 2), min: 1440 },
        { value: '4380', label: this.$t('common.monthCount', 6), min: 4380 },
        { value: '8760', label: this.$t('common.yearCount', 1), min: 8760 }
      ].filter(p => !limit || limit >= p.min);
      // "All" only when there's no limit or it exceeds a year
      if (!limit || limit > 8760) {
        ranges.push({ value: '-1', label: this.$t('common.allCareful'), min: 0 });
      }
      return ranges;
    },
    // label shown on the single time button. A custom window shows its absolute
    // duration as-is ("6 days 5 hours"); a preset is a rolling window from now,
    // so it's prefaced with "Last" ("Last 6 hours") to distinguish the two.
    // "All" isn't a duration, so it's shown as-is.
    currentRangeLabel: function () {
      if (this.isCustomRange) {
        return this.rangeLabel || this.$t('common.custom');
      }
      const match = this.presets.find(p => this.isPresetActive(p));
      if (!match) { return this.$t('search.timeOptions'); }
      if (match.value === '-1') { return match.label; }
      return this.$t('search.lastRange', { range: match.label });
    },
    // whether the bounding/interval section (and its divider) should render
    showBoundingInterval: function () {
      return !this.hideBounding || !this.hideInterval;
    },
    time: {
      get: function () {
        return this.$store.state.time;
      },
      set: function (newValue) {
        this.$store.commit('setTime', newValue);
      }
    },
    timeRange: {
      get: function () {
        return this.$store.state.timeRange;
      },
      set: function (newValue) {
        this.$store.commit('setTimeRange', newValue);
      }
    },
    focusTimeRange: {
      get: function () {
        return this.$store.state.focusTimeRange;
      },
      set: function (newValue) {
        this.$store.commit('setFocusTimeRange', newValue);
      }
    },
    shiftKeyHold: function () {
      return this.$store.state.shiftKeyHold;
    },
    expression: {
      get: function () {
        return this.$store.state.expression;
      },
      set: function (newValue) {
        this.$store.commit('setExpression', newValue);
      }
    }
  },
  watch: {
    // watch for the date, startTime, stopTime, interval, and bounding
    // route parameters to change, then update the view
    '$route.query': 'updateParams',
    // watch for other components to update the start and stop time
    time: {
      deep: true,
      handler (newVal, oldVal) {
        if (newVal && oldVal) {
          if (newVal.stopTime !== stopDateCheck) {
            dateChanged = true;
            stopDateCheck = newVal.stopTime;
            this.localStopTime = moment(newVal.stopTime * 1000);
          }

          if (newVal.startTime !== startDateCheck) {
            dateChanged = true;
            startDateCheck = newVal.startTime;
            this.localStartTime = moment(newVal.startTime * 1000);
          }
        }
      }
    },
    updateTime: function (newVal, oldVal) {
      if (newVal && newVal !== 'false') {
        // calculate new stop/start time
        this.updateStartStopTime();
        // tell the parent the time params have changed, which will issue a query
        if (newVal === 'query') { this.$emit('timeChange'); }
      }
    },
    // Mirror localStart/Stop into the displayed string, but skip the update
    // when the user is the source (the typed string already parses to the
    // same moment). That avoids stomping on the in-progress edit and
    // preserves caret position.
    localStartTime: function (newVal) {
      if (!newVal) { return; }
      const parsed = this.parseTimeInput(this.typedStartTime);
      if (parsed && parsed.valueOf() === newVal.valueOf()) { return; }
      this.typedStartTime = this.findTimeInTimezone(newVal.valueOf()).format(timeDisplayFormat);
    },
    localStopTime: function (newVal) {
      if (!newVal) { return; }
      const parsed = this.parseTimeInput(this.typedStopTime);
      if (parsed && parsed.valueOf() === newVal.valueOf()) { return; }
      this.typedStopTime = this.findTimeInTimezone(newVal.valueOf()).format(timeDisplayFormat);
    }
  },
  created () {
    this.setCurrentTime();

    let date = this.$route.query.date;
    // if no time params exist, default to config default or last hour
    if (!this.$route.query.startTime &&
      !this.$route.query.stopTime &&
      !this.$route.query.date) {
      date = this.$constants.DEFAULT_TIME_RANGE ?? 1;

      // SILENTLY update the url to reflect the default time range
      let path = this.$constants.PATH ?? '/';
      if (this.$route.path) {
        path += this.$route.path.slice(1);
      }
      const params = qs.stringify({ ...this.$route.query, date });
      window.history.replaceState(window.history.state, '', `${path}?${params}`);
    }

    this.setupTimeParams(
      date,
      this.$route.query.startTime,
      this.$route.query.stopTime
    );
  },
  methods: {
    readableTime,
    /* exposed page functions ------------------------------------ */
    /**
     * Returns the short name of the timezone
     * @returns {string} The short name of the timezone
     */
    getTimezoneShort () {
      if (this.timezone !== 'localtz') { return ''; }
      // get the timezone short name based on the user's locale
      const timeZone = Intl.DateTimeFormat().resolvedOptions().timeZone;
      return new Intl.DateTimeFormat('en-US', {
        timeZone,
        timeZoneName: 'short'
      }).formatToParts(new Date())
        .find(part => part.type === 'timeZoneName')
        .value;
    },
    /**
     * Fired when the time range value changes
     * Applies the date url parameter and removes the start/stop time url parameters
     * Updating the url parameter triggers updateParams
     */
    changeTimeRange: function () {
      this.timeError = '';
      this.focusTimeRange = false;

      if (this.user.timeLimit) {
        if (this.timeRange > this.user.timeLimit ||
          (this.timeRange === '-1' && this.user.timeLimit)) {
          this.timeRange = this.user.timeLimit;
        }
      }

      const routeQuery = this.$route.query;
      routeQuery.expression = this.expression;

      this.$router.push({
        query: {
          ...routeQuery,
          date: this.timeRange,
          stopTime: undefined,
          startTime: undefined
        }
      });
    },
    /**
     * Fired when a quick-range chip is clicked in the popover. Applies the
     * preset (reusing changeTimeRange) and closes the popover.
     * @param {string} value the preset's time range value (hours, or -1 for all)
     */
    selectPreset: function (value) {
      this.timeRange = value;
      this.changeTimeRange();
      this.menuOpen = false;
    },
    /* Fired when start datetime is typed. Keep the raw string in
     * typedStartTime so the input doesn't re-render mid-edit (which would
     * jump the cursor). Only sync to localStartTime if the string parses
     * strictly -- partial strings like "2025/05/20 1" shouldn't move the
     * canonical state. */
    changeStartTime: function (e) {
      this.typedStartTime = e.target.value;
      const parsed = this.parseTimeInput(e.target.value);
      if (!parsed) { return; }
      this.localStartTime = moment(parsed.valueOf());
      this.time.startTime = Math.floor(parsed.valueOf() / 1000);
      this.timeRange = '0'; // custom time range
      this.validateDate();
    },
    /* Fired when stop datetime is typed -- see changeStartTime. */
    changeStopTime: function (e) {
      this.typedStopTime = e.target.value;
      const parsed = this.parseTimeInput(e.target.value);
      if (!parsed) { return; }
      this.localStopTime = moment(parsed.valueOf());
      this.time.stopTime = Math.floor(parsed.valueOf() / 1000);
      this.timeRange = '0'; // custom time range
      this.validateDate();
    },
    /**
     * Parses typed or pasted datetime text in several common formats.
     * Bare timestamps are interpreted in the timezone the inputs display;
     * a trailing timezone (Z, +02:00, GMT+2, EDT, America/New_York) overrides.
     * @param {string} value the input text
     * @returns {object|undefined} a valid moment or undefined
     */
    parseTimeInput: function (value) {
      value = value.trim();

      let zone = this.timezone === 'gmt' ? 'UTC' : Intl.DateTimeFormat().resolvedOptions().timeZone;
      let offset;

      // detect and strip a trailing timezone: Z or a numeric offset (+02:00,
      // GMT+2), optionally attached (13:09:54Z), an abbreviation (UTC, EDT),
      // or an IANA name (America/New_York)
      const match = value.match(/^(.*\d)(?:\s*(?:(?:GMT|UTC)?([+-]\d{1,2}(?::?\d{2})?)|Z)|\s+(?:([A-Za-z]+(?:\/[A-Za-z_+-]+)+)|([A-Za-z]{1,5})))$/);
      if (match) {
        const [, text, numOffset, ianaName, abbreviation] = match;
        if (numOffset !== undefined) {
          const parts = numOffset.match(/^([+-])(\d{1,2}):?(\d{2})?$/);
          offset = (parts[1] === '-' ? -1 : 1) * ((parseInt(parts[2]) * 60) + parseInt(parts[3] || '0'));
        } else if (ianaName !== undefined) {
          if (!moment.tz.zone(ianaName)) { return undefined; } // unrecognized timezone
          zone = ianaName;
        } else if (abbreviation !== undefined) {
          offset = TZ_ABBREVIATIONS[abbreviation.toUpperCase()];
          if (offset === undefined) { return undefined; } // unrecognized timezone
        } else { // trailing Z
          offset = 0;
        }
        value = text;
      }

      let parsed = moment.tz(value, timeParseFormats, true, zone);
      if (offset !== undefined) { parsed = parsed.utcOffset(offset, true); }
      return parsed.isValid() ? parsed : undefined;
    },
    /* Fired when a date is picked from the v-date-picker popup. Only
     * overrides the date portion -- HH:mm:ss carry over from the current
     * value so the picker doesn't silently zero out the time. */
    applyStartDate: function (date) {
      if (!date) { return; }
      const newMoment = moment(this.localStartTime)
        .year(date.getFullYear()).month(date.getMonth()).date(date.getDate());
      this.localStartTime = newMoment;
      this.time.startTime = Math.floor(newMoment.valueOf() / 1000);
      this.timeRange = '0';
      this.validateDate();
    },
    applyStopDate: function (date) {
      if (!date) { return; }
      const newMoment = moment(this.localStopTime)
        .year(date.getFullYear()).month(date.getMonth()).date(date.getDate());
      this.localStopTime = newMoment;
      this.time.stopTime = Math.floor(newMoment.valueOf() / 1000);
      this.timeRange = '0';
      this.validateDate();
    },
    /* Fired when a native time field changes. Mirrors applyStartDate/applyStopDate
     * but for the HH:mm:ss portion -- the date carries over from the current
     * value. e.target.value is "HH:mm" or "HH:mm:ss" (24h).
     * @param {string} startOrStop whether to update the start time or stop time */
    changeTimeOfDay: function (startOrStop, e) {
      const local = startOrStop === 'start' ? this.localStartTime : this.localStopTime;
      if (!e.target.value || !local) { return; }
      const [h, m, s] = e.target.value.split(':');
      const newMoment = moment(local)
        .hour(parseInt(h, 10)).minute(parseInt(m, 10)).second(s ? parseInt(s, 10) : 0);
      const secondsPastEpoch = Math.floor(newMoment.valueOf() / 1000);
      if (startOrStop === 'start') {
        this.localStartTime = newMoment;
        this.time.startTime = secondsPastEpoch;
      } else {
        this.localStopTime = newMoment;
        this.time.stopTime = secondsPastEpoch;
      }
      this.timeRange = '0';
      this.validateDate();
    },
    /* whether a preset matches the active time range -- used for chip styling
     * and the button's current-range label */
    isPresetActive: function (p) {
      return parseFloat(this.timeRange) === parseFloat(p.value);
    },
    /**
     * Determines whether the supplied time is the start of a day
     * Fired from the previous start time button to determine tooltip text
     * @param {number} time date in seconds from 1970
     */
    isStartOfDay: function (time) {
      const currTime = this.findTimeInTimezone(time * 1000);
      const startOfDayTime = this.findTimeInTimezone(time * 1000).startOf('day');
      return startOfDayTime.isSame(currTime, 'seconds');
    },
    /**
     * Determines whether the supplied time is the end of a day
     * Fired from the next end time button to determine tooltip text
     * @param {number} time date in seconds from 1970
     */
    isEndOfDay: function (time) {
      const currTime = this.findTimeInTimezone(time * 1000);
      const endOfDayTime = this.findTimeInTimezone(time * 1000).endOf('day');
      return endOfDayTime.isSame(currTime, 'seconds');
    },
    /**
     * Fired when clicking the previous time button on a time input
     * @param {string} startOrStop whether to update the start time or stop time
     */
    prevTime: function (startOrStop) {
      let newTime = (startOrStop === 'start')
        ? this.findTimeInTimezone(this.time.startTime * 1000).startOf('day')
        : this.findTimeInTimezone(this.time.stopTime * 1000).endOf('day');

      // start time goes to beginning of PREV day when when its at the beginning
      // stop time always goes to end of day of the previous day
      if ((startOrStop === 'start' && this.isStartOfDay(this.time.startTime)) ||
        (startOrStop === 'stop')) {
        newTime = newTime.subtract(1, 'days');
      }

      const secondsPastEpoch = Math.floor(newTime.valueOf() / 1000);
      // If range value falls above epoch time, update existing time data
      if (secondsPastEpoch >= 0) {
        if (startOrStop === 'start') {
          this.localStartTime = newTime;
          this.time.startTime = secondsPastEpoch;
        } else {
          this.localStopTime = newTime;
          this.time.stopTime = secondsPastEpoch;
        }

        this.timeRange = '0';
        this.validateDate();
      }
    },
    /**
     * Fired when clicking the next time button on a time input
     * @param {string} startOrStop whether to update the start time or stop time
     */
    nextTime: function (startOrStop) {
      if (startOrStop === 'start') {
        // start time always goes to the beginning of the next day
        let newTime = this.findTimeInTimezone(this.time.startTime * 1000).startOf('day');
        newTime = newTime.add(1, 'days');
        this.localStartTime = newTime;
        this.time.startTime = Math.floor(this.localStartTime.valueOf() / 1000);
      } else {
        let newTime = this.findTimeInTimezone(this.time.stopTime * 1000).endOf('day');
        if (this.isEndOfDay(this.time.stopTime)) {
          // it's the end of the day, so go to the end of the NEXT day
          newTime = newTime.add(1, 'days');
        }
        this.localStopTime = newTime;
        this.time.stopTime = Math.floor(this.localStopTime.valueOf() / 1000);
      }
      this.timeRange = '0';
      this.validateDate();
    },
    /**
     * Fired when change bounded select box is changed
     * Applies the timeBounding url parameter
     * Updating the url parameter triggers updateParams
     */
    changeTimeBounding: function () {
      this.$router.push({
        query: {
          ...this.$route.query,
          bounding: this.timeBounding !== 'last' ? this.timeBounding : undefined
        }
      });
    },
    /**
     * Fired when change interval select box is changed
     * Applies the timeBounding url parameter
     * Updating the url parameter triggers updateParams
     */
    changeTimeInterval: function () {
      this.$router.push({
        query: {
          ...this.$route.query,
          interval: this.timeInterval !== 'auto' ? this.timeInterval : undefined
        }
      });
    },
    /**
     * Fired when a date value is changed manually or the datepicker is closed
     * Validates the selected start/stop time window, setting timeError if invalid
     * start/stop url parameters are updated in Search.vue timeUpdate function
     */
    validateDate: function () {
      if (!dateChanged) { return; }
      dateChanged = false;

      this.timeError = '';

      const stopSec = parseInt(this.time.stopTime, 10);
      const startSec = parseInt(this.time.startTime, 10);

      // only continue if start and stop are valid numbers
      if (!startSec || !stopSec || isNaN(startSec) || isNaN(stopSec)) {
        return;
      }

      if (stopSec < startSec) { // don't continue if stop < start
        this.timeError = 'Stop time cannot be before start time';
        return;
      }

      // make sure the time range does not exceed the user setting
      if (this.user.timeLimit) {
        const deltaTimeHrs = (stopSec - startSec) / 3600;
        if (deltaTimeHrs > this.user.timeLimit) {
          this.timeError = `Your query cannot exceed ${this.user.timeLimit} hours`;
        }
      }
    },
    onOffTimeRangeFocus: function () {
      this.focusTimeRange = false;
    },
    /* helper functions ------------------------------------------ */
    /* Sets the current time in seconds */
    setCurrentTime: function () {
      currentTimeSec = Math.floor(new Date().getTime() / 1000);
    },
    /**
     * Find the time relative to the user specified timezone setting
     * @param {number} time The time in ms
     */
    findTimeInTimezone: function (time) {
      let newTime = moment(time);

      if (this.timezone === 'gmt') {
        newTime = moment.tz(time, 'utc');
      } else if (this.timezone === 'localtz') {
        newTime = moment.tz(
          time,
          Intl.DateTimeFormat().resolvedOptions().timeZone
        );
      }

      return newTime;
    },
    /**
     * Fired when time component search params are changed
     * Calculates the new stopTime and startTime
     * Emits an event for the parent to catch
     */
    updateStartStopTime: function () {
      this.setCurrentTime();

      if (this.timeRange > 0) {
        // if it's not a custom time range or all, update the time
        this.localStopTime = moment(currentTimeSec * 1000);
        this.time.stopTime = currentTimeSec.toString();
        this.localStartTime = moment((currentTimeSec - (hourSec * this.timeRange)) * 1000);
        this.time.startTime = (currentTimeSec - (hourSec * this.timeRange)).toString();
      }

      if (parseFloat(this.timeRange) === -1) { // all time
        this.localStartTime = moment(0);
        this.time.startTime = '0';
        this.localStopTime = moment(currentTimeSec * 1000);
        this.time.stopTime = currentTimeSec.toString();
      }
    },
    /**
     * Sets up time query parameters
     * @param {string} date       The time range to query within
     * @param {string} startTime  The start time for a custom time range
     * @param {string} stopTime   The stop time for a custom time range
     */
    setupTimeParams: function (date, startTime, stopTime) {
      if (date) { // time range is available
        // make sure the time range does not exceed the user setting
        if (this.user.timeLimit) {
          if (date > this.user.timeLimit ||
            (date === '-1' && this.user.timeLimit)) {
            date = this.user.timeLimit;
          }
        }
        this.timeRange = date;
        if (parseFloat(this.timeRange) === -1) { // all time
          this.localStartTime = moment(0);
          this.time.startTime = '0';
          this.localStopTime = moment(currentTimeSec * 1000);
          this.time.stopTime = currentTimeSec.toString();
        } else if (this.timeRange > 0) {
          this.localStopTime = moment(currentTimeSec * 1000);
          this.time.stopTime = currentTimeSec.toString();
          this.localStartTime = moment((currentTimeSec - (hourSec * this.timeRange)) * 1000);
          this.time.startTime = (currentTimeSec - (hourSec * this.timeRange)).toString();
        }
      } else if (startTime !== undefined && stopTime !== undefined) {
        // start and stop times available
        let stop = stopTime;
        let start = startTime;

        if (!isNaN(stop) && !isNaN(start)) {
          stop = parseInt(stop, 10);
          start = parseInt(start, 10);

          if (stop < start) {
            this.timeError = 'Stop time cannot be before start time';
          }

          // clamp the window to the user's time limit
          if (this.user.timeLimit && (stop - start) / 3600 > this.user.timeLimit) {
            start = stop - (this.user.timeLimit * 3600);
          }

          // if we can parse start and stop time, set them
          this.timeRange = '0'; // custom time range
          this.localStopTime = moment(stop * 1000);
          this.time.stopTime = stop;
          this.localStartTime = moment(start * 1000);
          this.time.startTime = start;
        } else { // if we can't parse stop or start time, set default
          this.timeRange = this.$constants.DEFAULT_TIME_RANGE ?? '1'; // default to config or 1 hour
        }
      } else {
        // there are no time query parameters, so set defaults
        this.timeRange = this.$constants.DEFAULT_TIME_RANGE ?? '1'; // default to config or 1 hour
      }
    },
    /* watch for the url parameters to change and update the page */
    updateParams: function (newParams, oldParams) {
      let change = false;

      if (newParams.bounding !== oldParams.bounding) {
        change = true;
        this.timeBounding = newParams.bounding || 'last';
      }

      if (newParams.interval !== oldParams.interval) {
        change = true;
        this.timeInterval = newParams.interval || 'auto';
      }

      if (newParams.date !== oldParams.date) {
        change = true;
        if (newParams.date !== this.timeRange) {
          // don't allow the time range to exceed the user setting
          if (this.user.timeLimit) {
            if (newParams.date > this.user.timeLimit ||
              (newParams.date === '-1' && this.user.timeLimit)) {
              newParams.date = this.user.timeLimit;
            }
          }
          this.timeRange = newParams.date || 0;
        }
        // calculate the new stop/start times because the range changed
        this.updateStartStopTime();
      }

      let stopTimeChanged = false;
      let startTimeChanged = false;
      if (newParams.stopTime && newParams.stopTime !== oldParams.stopTime) {
        change = true;
        stopTimeChanged = true;
      }

      if (newParams.startTime && newParams.startTime !== oldParams.startTime) {
        change = true;
        startTimeChanged = true;
      }

      if (stopTimeChanged || startTimeChanged) {
        // make sure the time window does not exceed the user setting
        const deltaTime = newParams.stopTime - newParams.startTime;

        const deltaTimeHrs = deltaTime / 3600;
        if (!this.user.timeLimit || deltaTimeHrs <= this.user.timeLimit) {
          if (startTimeChanged) {
            this.time.startTime = newParams.startTime;
            this.localStartTime = moment(newParams.startTime * 1000);
          }
          if (stopTimeChanged) {
            this.time.stopTime = newParams.stopTime;
            this.localStopTime = moment(newParams.stopTime * 1000);
          }
        }
      }

      if (change) { this.$emit('timeChange'); }
    }
  }
};
</script>

<style scoped>
/* match the 32px height of the search-row buttons */
.time-options-btn {
  height: 32px;
}
</style>

<style>
/* Time-options breakout popover. Its content is teleported to the overlay
   root, so Time.vue's scoped styles don't reach it -- these rules are
   intentionally global (class names namespaced to avoid leakage). Width is
   capped to fit the two date columns side-by-side; without the cap the card
   grows to fit all preset chips on a single line and reads as too wide. The
   chips wrap to this width instead; columns stack on narrow viewports. */
.arkime-time-popover {
  max-width: min(720px, 95vw);
}
.arkime-time-popover .arkime-time-col {
  /* size each column to the date picker's intrinsic width so the fluid
     start/stop input above it matches the calendar width exactly (a growing
     column would stretch the input wider than the fixed-width calendar) */
  flex: 0 0 auto;
}
/* Vuetify dividers default to --v-border-opacity ~0.12, which is nearly
   invisible on light themes. Bump it so the section separators read clearly. */
.arkime-time-popover .v-divider {
  --v-border-opacity: 0.32;
}
.arkime-time-popover .arkime-time-error {
  display: flex;
  align-items: center;
  color: rgb(var(--v-theme-secondary));
  font-size: 0.875rem;
  font-weight: 600;
}
</style>
