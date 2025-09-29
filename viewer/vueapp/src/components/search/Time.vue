<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>

  <BRow gutter-x="1" class="text-start flex-nowrap" align-h="start">

    <!-- time range select -->
    <BCol cols="auto">
      <BInputGroup size="sm">
        <BInputGroupText id="timeInput" class="cursor-help input-group-text-fw">
          <span v-if="!shiftKeyHold"
            class="fa fa-clock-o fa-fw">
          </span>
          <span v-else
            class="time-shortcut">
            T
          </span>
          <BTooltip target="timeInput" placement="bottom" :delay="{show: 500, hide: 0}" noninteractive><span v-i18n-btip="'search.'" /></BTooltip>
        </BInputGroupText>
        <select
          tabindex="3"
          role="listbox"
          class="form-control"
          v-model="timeRange"
          v-focus="focusTimeRange"
          @change="changeTimeRange"
          @blur="onOffTimeRangeFocus">
          <option value="1">{{ $t('common.hourCount', 1) }}</option>
          <option value="6"
            v-if="!user.timeLimit || user.timeLimit >= 6">
            {{ $t('common.hourCount', 6) }}
          </option>
          <option value="24"
            v-if="!user.timeLimit || user.timeLimit >= 24">
            {{ $t('common.hourCount', 24) }}
          </option>
          <option value="48"
            v-if="!user.timeLimit || user.timeLimit >= 48">
            {{ $t('common.hourCount', 48) }}
          </option>
          <option value="72"
            v-if="!user.timeLimit || user.timeLimit >= 72">
            {{ $t('common.hourCount', 72) }}
          </option>
          <option value="168"
            v-if="!user.timeLimit || user.timeLimit >= 168">
            {{ $t('common.weekCount', 1) }}
          </option>
          <option value="336"
            v-if="!user.timeLimit || user.timeLimit >= 336">
            {{ $t('common.weekCount', 2) }}
          </option>
          <option value="720"
            v-if="!user.timeLimit || user.timeLimit >= 720">
            {{ $t('common.monthCount', 1) }}
          </option>
          <option value="1440"
            v-if="!user.timeLimit || user.timeLimit >= 1440">
            {{ $t('common.monthCount', 2) }}
          </option>
          <option value="4380"
            v-if="!user.timeLimit || user.timeLimit >= 4380">
            {{ $t('common.monthCount', 6) }}
          </option>
          <option value="8760"
            v-if="!user.timeLimit || user.timeLimit >= 8760">
            {{ $t('common.yearCount', 1) }}
          </option>
          <option value="-1"
            v-if="!user.timeLimit || user.timeLimit > 8760">
            {{ $t('common.allCareful') }}
          </option>
          <option value="0" disabled>{{ $t('common.custom') }}</option>
        </select>
      </BInputGroup>
    </BCol> <!-- /time range select -->

    <!-- start time -->
    <BCol cols="auto">
      <BInputGroup size="sm">
        <BInputGroupText id="startTime" class="cursor-help">
          {{ $t('search.startTime') }}
          <BTooltip target="startTime" placement="bottom" :delay="{show: 500, hide: 0}" noninteractive><span v-i18n-btip="'search.'" /></BTooltip>
        </BInputGroupText>
        <input
          type="datetime-local"
          tabindex="4"
          id="startTime"
          ref="startTime"
          name="startTime"
          class="form-control"
          @input="changeStartTime"
          :value="localStartTime.format('YYYY-MM-DDTHH:mm:ss')"
        />
        <BInputGroupText
          v-if="timezone !== 'local'"
          :id="`startTimeTimezone`"
          class="cursor-help">
          {{ timezone === 'gmt' ? 'UTC' : getTimezoneShort() }}
          <BTooltip target="startTimeTimezone" placement="bottom" :delay="{show: 500, hide: 0}" noninteractive>
            {{ timezone === 'gmt' ? 'UTC' : Intl.DateTimeFormat().resolvedOptions().timeZone }}
            {{ timezone === 'gmt' ? new Date().getTimezoneOffset() / -60 + ':00' : '' }}
          </BTooltip>
        </BInputGroupText>
        <BButton
          variant="outline-secondary"
          id="prevStartTime"
          class="cursor-pointer"
          @click="prevTime('start')">
          <span class="fa fa-step-backward"></span>
          <BTooltip target="prevStartTime" placement="bottom" :delay="{show: 500, hide: 0}" noninteractive><span v-i18n-btip="'search.'" /></BTooltip>
        </BButton>
        <BButton
          variant="outline-secondary"
          id="nextStartTime"
          class="cursor-pointer"
          @click="nextTime('start')">
          <span class="fa fa-step-forward"></span>
          <BTooltip target="nextStartTime" placement="bottom" :delay="{show: 500, hide: 0}" noninteractive><span v-i18n-btip="'search.'" /></BTooltip>
        </BButton>
      </BInputGroup>
    </BCol> <!-- /start time -->

    <!-- stop time -->
    <BCol cols="auto">
      <BInputGroup size="sm">
        <BInputGroupText id="stopTime" class="cursor-help">
          {{ $t('search.stopTime') }}
          <BTooltip target="stopTime" placement="bottom" :delay="{show: 500, hide: 0}" noninteractive><span v-i18n-btip="'search.'" /></BTooltip>
        </BInputGroupText>
        <input
          type="datetime-local"
          tabindex="5"
          id="stopTime"
          ref="stopTime"
          name="stopTime"
          class="form-control"
          @input="changeStopTime"
          :value="localStopTime.format('YYYY-MM-DDTHH:mm:ss')"
        />
        <BInputGroupText
          v-if="timezone !== 'local'"
          :id="`stopTimeTimezone`"
          class="cursor-help">
          {{ timezone === 'gmt' ? 'UTC' : getTimezoneShort() }}
          <BTooltip target="stopTimeTimezone" placement="bottom" :delay="{show: 500, hide: 0}" noninteractive>
            {{ timezone === 'gmt' ? 'UTC' : Intl.DateTimeFormat().resolvedOptions().timeZone }}
            {{ timezone === 'gmt' ? new Date().getTimezoneOffset() / -60 + ':00' : '' }}
          </BTooltip>
        </BInputGroupText>
        <BButton
          variant="outline-secondary"
          id="prevStopTime"
          class="cursor-pointer"
          @click="prevTime('stop')">
          <span class="fa fa-step-backward"></span>
          <BTooltip target="prevStopTime" placement="bottom" :delay="{show: 500, hide: 0}" noninteractive><span v-i18n-btip="'search.'" /></BTooltip>
        </BButton>
        <BButton
          variant="outline-secondary"
          id="nextStopTime"
          class="cursor-pointer"
          @click="nextTime('stop')">
          <span class="fa fa-step-forward"></span>
          <BTooltip target="nextStopTime" placement="bottom" :delay="{show: 500, hide: 0}" noninteractive><span v-i18n-btip="'search.'" /></BTooltip>
        </BButton>
      </BInputGroup>
    </BCol> <!-- /stop time -->

    <!-- time bounding select -->
    <BCol cols="auto" v-if="!hideBounding">
      <BInputGroup size="sm">
        <BInputGroupText id="timeBounding" class="cursor-help">
          Bounding
          <BTooltip target="timeBounding" placement="bottom" :delay="{show: 500, hide: 0}" noninteractive><span v-i18n-btip="'search.'" /></BTooltip>
        </BInputGroupText>
        <select class="form-control"
          v-model="timeBounding"
          tabindex="6"
          @change="changeTimeBounding">
          <option value="first" v-i18n-value="'search.timeBounding-'" />
          <option value="last" v-i18n-value="'search.timeBounding-'" />
          <option value="last" v-i18n-value="'search.timeBounding-'" />
          <option value="both" v-i18n-value="'search.timeBounding-'" />
          <option value="either" v-i18n-value="'search.timeBounding-'" />
          <option value="database" v-i18n-value="'search.timeBounding-'" />
        </select>
      </BInputGroup>
    </BCol>  <!-- /time bounding select -->

    <!-- time interval select -->
    <BCol cols="auto" v-if="!hideInterval">
      <BInputGroup size="sm">
        <BInputGroupText id="timeInterval" class="cursor-help">
        {{ $t('search.timeInterval') }}
          <BTooltip target="timeInterval" placement="bottom" :delay="{show: 500, hide: 0}" noninteractive><span v-i18n-btip="'search.'" /></BTooltip>
        </BInputGroupText>
        <select class="form-control"
          v-model="timeInterval"
          tabindex="6"
          @change="changeTimeInterval">
          <option value="auto" v-i18n-value="'search.timeInterval-'" />
          <option value="second" v-i18n-value="'search.timeInterval-'" />
          <option value="minute" v-i18n-value="'search.timeInterval-'" />
          <option value="hour" v-i18n-value="'search.timeInterval-'" />
          <option value="day" v-i18n-value="'search.timeInterval-'" />
        </select>
      </BInputGroup>
    </BCol> <!-- /time interval select -->

    <!-- human readable time range or error -->
    <BCol cols="auto" class="mt-2 time-range-display">
      <strong class="text-theme-accent">
        <template v-if="deltaTime && !timeError">
          <span id="timeRangeDisplay"
            class="help-cursor">
            {{ readableTime(deltaTime * 1000) }}
            <BTooltip target="timeRangeDisplay" placement="bottom" :delay="{show: 500, hide: 0}" noninteractive><span v-i18n-btip="'search.'" /></BTooltip>
          </span>
        </template>
        <template v-if="timeError">
          <span class="fa fa-exclamation-triangle"></span>&nbsp;
          {{ timeError }}
        </template>
      </strong>
    </BCol> <!-- /human readable time range or error -->

  </BRow>

</template>

<script>
import Focus from '@common/Focus.vue';
import { readableTime } from '@common/vueFilters.js';

import qs from 'qs';
import moment from 'moment-timezone';

const hourSec = 3600;
let currentTimeSec;
let dateChanged = false;
let startDateCheck;
let stopDateCheck;

export default {
  name: 'ArkimeTime',
  // components: { datePicker },
  directives: { Focus },
  props: [
    'timezone',
    'hideBounding',
    'hideInterval',
    'updateTime'
  ],
  data: function () {
    return {
      deltaTime: null,
      timeError: '',
      timeBounding: this.$route.query.bounding || 'last',
      timeInterval: this.$route.query.interval || 'auto',
      // use start/stop time localized to this component so that the time
      // watcher can compare time values to local (unaffected) start/stop times
      localStopTime: undefined,
      localStartTime: undefined,
      datePickerOptions: {
        useCurrent: false,
        format: 'YYYY/MM/DD HH:mm:ss',
        timeZone: this.timezone === 'local' || this.timezone === 'localtz' ? Intl.DateTimeFormat().resolvedOptions().timeZone : 'UTC',
        showClose: true,
        focusOnShow: false,
        showTodayButton: true,
        allowInputToggle: true,
        minDate: moment(0),
        keyBinds: null // disable all key binds and manually monitor enter and escape
      }
    };
  },
  computed: {
    user: function () {
      return this.$store.state.user;
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
      if (newVal) {
        // calculate new stop/start time
        this.updateStartStopTime();
        // tell the parent the time params have changed, which will issue a query
        if (newVal === 'query') { this.$emit('timeChange'); }
      }
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
      this.deltaTime = null;
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
    /* Fired when start datetime is changed */
    changeStartTime: function (e) {
      const msDate = moment(e.target.value).valueOf();
      this.localStartTime = moment(msDate);
      this.time.startTime = Math.floor(msDate / 1000);
      this.timeRange = '0'; // custom time range
      this.validateDate();
    },
    /* Fired when stop datetime is changed */
    changeStopTime: function (e) {
      const msDate = moment(e.target.value).valueOf();
      this.localStopTime = moment(msDate);
      this.time.stopTime = Math.floor(msDate / 1000);
      this.timeRange = '0'; // custom time range
      this.validateDate();
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
     * Validates a date and updates delta time (stop time - start time)
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

      // update the displayed time range
      const deltaTime = stopSec - startSec;

      // make sure the time range does not exceed the user setting
      if (this.user.timeLimit) {
        const deltaTimeHrs = deltaTime / 3600;
        if (deltaTimeHrs > this.user.timeLimit) {
          this.timeError = `Your query cannot exceed ${this.user.timeLimit} hours`;
          return;
        }
      }

      this.deltaTime = deltaTime;
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

      if (parseInt(this.timeRange, 10) === -1) { // all time
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
        if (parseInt(this.timeRange, 10) === -1) { // all time
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

          // update the displayed time range
          let deltaTime = stop - start;

          // make sure the time range does not exceed the user setting
          const deltaTimeHrs = deltaTime / 3600;
          if (this.user.timeLimit) {
            if (deltaTimeHrs > this.user.timeLimit) {
              start = stop - (this.user.timeLimit * 3600);
              deltaTime = stop - start;
            }
          }

          this.deltaTime = deltaTime;

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
.time-range-display {
  font-size: 0.85rem;
}
</style>
