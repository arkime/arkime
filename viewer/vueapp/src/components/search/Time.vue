<template>

  <div class="form-inline time-form">

    <!-- time range select -->
    <div class="form-group">
      <div class="input-group input-group-sm">
        <span class="input-group-prepend input-group-prepend-fw cursor-help"
          placement="topright"
          v-b-tooltip.hover
          title="Time Range">
          <span class="input-group-text input-group-text-fw">
            <span v-if="!shiftKeyHold"
              class="fa fa-clock-o fa-fw">
            </span>
            <span v-else
              class="time-shortcut">
              T
            </span>
          </span>
        </span>
        <select class="form-control time-range-control"
          tabindex="3"
          v-model="timeRange"
          @change="changeTimeRange"
          @blur="onOffTimeRangeFocus"
          v-focus-input="focusTimeRange">
          <option value="1">Last hour</option>
          <option value="6">Last 6 hours</option>
          <option value="24">Last 24 hours</option>
          <option value="48">Last 48 hours</option>
          <option value="72">Last 72 hours</option>
          <option value="168">Last week</option>
          <option value="720">Last month</option>
          <option value="4380">Last 6 months</option>
          <option value="-1">All (careful)</option>
          <option value="0" disabled>Custom</option>
        </select>
      </div>
    </div> <!-- /time range select -->

    <!-- start time -->
    <div class="form-group ml-1">
      <div class="input-group input-group-sm input-group-time">
        <span class="input-group-prepend cursor-help"
          placement="topright"
          v-b-tooltip.hover
          title="Beginning Time">
          <span class="input-group-text">
            Start
          </span>
        </span>
        <date-picker v-model="localStartTime"
          :config="datePickerOptions"
          @dp-change="changeStartTime"
          @dp-hide="closePicker"
          name="startTime"
          ref="startTime"
          id="startTime"
          tabindex="4">
        </date-picker>
      </div>
    </div> <!-- /start time -->

    <!-- stop time -->
    <div class="form-group ml-1">
      <div class="input-group input-group-sm input-group-time">
        <span class="input-group-prepend cursor-help"
          placement="topright"
          v-b-tooltip.hover
          title="Stop Time">
          <span class="input-group-text">
            End
          </span>
        </span>
        <date-picker v-model="localStopTime"
          :config="datePickerOptions"
          @dp-change="changeStopTime"
          @dp-hide="closePicker"
          name="stopTime"
          ref="stopTime"
          id="stopTime"
          tabindex="5">
        </date-picker>
      </div>
    </div> <!-- /stop time -->

    <!-- time bounding select -->
    <div class="form-group ml-1"
      v-if="!hideBounding">
      <div class="input-group input-group-sm">
        <span class="input-group-prepend cursor-help"
          placement="topright"
          v-b-tooltip.hover
          title="Which time field to use for selected time window">
          <span class="input-group-text">
            Bounding
          </span>
        </span>
        <select class="form-control time-range-control"
          v-model="timeBounding"
          tabindex="6"
          @change="changeTimeBounding">
          <option value="first">First Packet</option>
          <option value="last">Last Packet</option>
          <option value="both">Bounded</option>
          <option value="either">Session Overlaps</option>
          <option value="database">Database</option>
        </select>
      </div>
    </div> <!-- /time bounding select -->

    <!-- time interval select -->
    <div class="form-group ml-1"
      v-if="!hideInterval">
      <div class="input-group input-group-sm">
        <span class="input-group-prepend cursor-help"
          tooltip-placement="topright"
          v-b-tooltip.hover
          title="Time interval bucket size for graph">
          <span class="input-group-text">
            Interval
          </span>
        </span>
        <select class="form-control time-range-control"
          v-model="timeInterval"
          tabindex="6"
          @change="changeTimeInterval">
          <option value="auto">Auto</option>
          <option value="second">Seconds</option>
          <option value="minute">Minutes</option>
          <option value="hour">Hours</option>
          <option value="day">Days</option>
        </select>
      </div>
    </div> <!-- /time interval select -->

    <!-- human readable time range or error -->
    <div class="ml-1 time-range-display">
      <strong class="text-theme-accent">
        <span v-if="deltaTime && !timeError"
          class="help-cursor"
          v-b-tooltip.hover
          title="Query time range">
          {{ deltaTime * 1000 | readableTime }}
        </span>
        <span v-if="timeError">
          <span class="fa fa-exclamation-triangle"></span>&nbsp;
          {{ timeError }}
        </span>
      </strong>
    </div> <!-- /human readable time range or error -->

  </div>

</template>

<script>
import FocusInput from '../utils/FocusInput';

import datePicker from 'vue-bootstrap-datetimepicker';
import 'pc-bootstrap4-datetimepicker/build/css/bootstrap-datetimepicker.css';
import moment from 'moment-timezone';

const hourSec = 3600;
let currentTimeSec;
let dateChanged = false;
let startDateCheck;
let stopDateCheck;
let timeRangeUpdated = false;

export default {
  name: 'MolochTime',
  components: { datePicker },
  directives: { FocusInput },
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
        timeZone: this.timezone === 'local' || this.timezone === 'localtz' ? Intl.DateTimeFormat().resolvedOptions().timeZone : 'GMT',
        showClose: true,
        focusOnShow: false,
        showTodayButton: true,
        allowInputToggle: true
      }
    };
  },
  watch: {
    // watch for the date, startTime, stopTime, interval, and bounding
    // route parameters to change, then update the view
    '$route.query': 'updateParams',
    // watch for other components to update the start and stop time
    'time': {
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
    'updateTime': function (newVal, oldVal) {
      if (newVal) {
        // calculate new stop/start time
        this.updateStartStopTime();
        // tell the parent the time params have changed
        this.$emit('timeChange');
      }
    }
  },
  created: function () {
    this.setCurrentTime();

    let date = this.$route.query.date;
    // if no time params exist, default to last hour
    if (!this.$route.query.startTime &&
      !this.$route.query.stopTime &&
      !this.$route.query.date) {
      date = 1;
    }

    this.setupTimeParams(
      date,
      this.$route.query.startTime,
      this.$route.query.stopTime
    );
  },
  computed: {
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
    }
  },
  methods: {
    /* exposed page functions ------------------------------------ */
    /**
     * Fired when the time range value changes
     * Applies the date url parameter and removes the start/stop time url parameters
     * Updating the url parameter triggers updateParams
     */
    changeTimeRange: function () {
      this.deltaTime = null;
      this.timeError = '';
      this.focusTimeRange = false;

      this.$router.push({
        query: {
          ...this.$route.query,
          date: this.timeRange,
          stopTime: undefined,
          startTime: undefined
        }
      });

      timeRangeUpdated = true;
    },
    /**
     * Fired when a date time picker is closed
     * Sets the time range updated flag to false so
     * validate date knows that a date was changed
     * rather than the date range input
     */
    closePicker: function () {
      timeRangeUpdated = false;
    },
    /* Fired when start datetime is changed */
    changeStartTime: function (event) {
      let msDate = event.date.valueOf();
      this.time.startTime = Math.floor(msDate / 1000);
      this.validateDate();
    },
    /* Fired when stop datetime is changed */
    changeStopTime: function (event) {
      let msDate = event.date.valueOf();
      this.time.stopTime = Math.ceil(msDate / 1000);
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

      // if the time range wasn't updated, we can assume that the start/stop
      // times were udpated, so set the timerange to custom
      if (!timeRangeUpdated) { this.timeRange = '0'; }

      let stopSec = parseInt(this.time.stopTime, 10);
      let startSec = parseInt(this.time.startTime, 10);

      // only continue if start and stop are valid numbers
      if (!startSec || !stopSec || isNaN(startSec) || isNaN(stopSec)) {
        return;
      }

      if (stopSec < startSec) { // don't continue if stop < start
        this.timeError = 'Stop time cannot be before start time';
        return;
      }

      // update the displayed time range
      this.deltaTime = stopSec - startSec;
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
      } else if (startTime && stopTime) {
        // start and stop times available
        let stop = stopTime;
        let start = startTime;

        if (stop && start && !isNaN(stop) && !isNaN(start)) {
          // if we can parse start and stop time, set them
          this.timeRange = '0'; // custom time range
          this.localStopTime = moment(stop * 1000);
          this.time.stopTime = stop;
          this.localStartTime = moment(start * 1000);
          this.time.startTime = start;

          stop = parseInt(stop, 10);
          start = parseInt(start, 10);

          if (stop < start) {
            this.timeError = 'Stop time cannot be before start time';
          }

          // update the displayed time range
          this.deltaTime = stop - start;
        } else { // if we can't parse stop or start time, set default
          this.timeRange = '1'; // default to 1 hour
        }
      } else if (!date && !startTime && !stopTime) {
        // there are no time query parameters, so set defaults
        this.timeRange = '1'; // default to 1 hour
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
          this.timeRange = newParams.date || 0;
        }
        // calculate the new stop/start times because the range changed
        this.updateStartStopTime();
      }

      if (newParams.stopTime && newParams.stopTime !== oldParams.stopTime) {
        change = true;
        this.time.stopTime = newParams.stopTime;
        this.localStopTime = moment(newParams.stopTime * 1000);
      }

      if (newParams.startTime && newParams.startTime !== oldParams.startTime) {
        change = true;
        this.time.startTime = newParams.startTime;
        this.localStartTime = moment(newParams.startTime * 1000);
      }

      if (change) { this.$emit('timeChange'); }
    }
  }
};
</script>

<style scoped>
.time-form {
  flex-flow: row nowrap;
  max-height: 33px;
}

.time-range-display {
  line-height: 0.95;
  font-size: 12px;
}

select.form-control {
  font-size: var(--px-lg);
}

.time-range-control {
  -webkit-appearance: none;
}

.input-group-time input.form-control {
  font-size: 75%;
}
</style>
