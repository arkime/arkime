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
          <option value="6"
            v-if="!user.timeLimit || user.timeLimit >= 6">
            Last 6 hours
          </option>
          <option value="24"
            v-if="!user.timeLimit || user.timeLimit >= 24">
            Last 24 hours
          </option>
          <option value="48"
            v-if="!user.timeLimit || user.timeLimit >= 48">
            Last 48 hours
          </option>
          <option value="72"
            v-if="!user.timeLimit || user.timeLimit >= 72">
            Last 72 hours
          </option>
          <option value="168"
            v-if="!user.timeLimit || user.timeLimit >= 168">
            Last week
          </option>
          <option value="336"
            v-if="!user.timeLimit || user.timeLimit >= 336">
            Last 2 weeks
          </option>
          <option value="720"
            v-if="!user.timeLimit || user.timeLimit >= 720">
            Last month
          </option>
          <option value="1440"
            v-if="!user.timeLimit || user.timeLimit >= 1440">
            Last 2 months
          </option>
          <option value="4380"
            v-if="!user.timeLimit || user.timeLimit >= 4380">
            Last 6 months
          </option>
          <option value="8760"
            v-if="!user.timeLimit || user.timeLimit >= 8760">
            Last year
          </option>
          <option value="-1"
            v-if="!user.timeLimit || user.timeLimit > 8760">
            All (careful)
          </option>
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
        <span class="input-group-append cursor-pointer"
          id="prevStartTime"
          @click="prevTime('start')">
          <div class="input-group-text">
            <span class="fa fa-step-backward">
            </span>
          </div>
        </span>
        <b-tooltip
          v-if="isStartOfDay(time.startTime)"
          target="prevStartTime">
          Beginning of previous day
        </b-tooltip>
        <b-tooltip
          v-else
          target="prevStartTime">
          Beginning of this day
        </b-tooltip>
        <span class="input-group-append cursor-pointer"
          placement="topright"
          v-b-tooltip.hover
          title="Beginning of next day"
          @click="nextTime('start')">
          <div class="input-group-text">
            <span class="fa fa-step-forward">
            </span>
          </div>
        </span>
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
        <span class="input-group-append cursor-pointer"
          placement="topright"
          v-b-tooltip.hover
          title="End of previous day"
          @click="prevTime('stop')">
          <div class="input-group-text">
            <span class="fa fa-step-backward">
            </span>
          </div>
        </span>
        <span class="input-group-append cursor-pointer"
          id="nextStopTime"
          @click="nextTime('stop')">
          <div class="input-group-text">
            <span class="fa fa-step-forward">
            </span>
          </div>
        </span>
        <b-tooltip
          v-if="isEndOfDay(time.stopTime)"
          target="prevStartTime">
          End of next day
        </b-tooltip>
        <b-tooltip
          v-else
          target="nextStopTime">
          End of this day
        </b-tooltip>
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

    // register key up event listeners on start and stop time
    // to close the datetimepickers because keyBinds for this
    // component have been removed because of usability issues
    setTimeout(() => { // wait for datetimepicker to load
      $('#stopTime').on('keyup', this.stopDatePickerClose);
      $('#startTime').on('keyup', this.startDatePickerClose);
    });
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

      if (this.user.timeLimit) {
        if (this.timeRange > this.user.timeLimit ||
          (this.timeRange === '-1' && this.user.timeLimit)) {
          this.timeRange = this.user.timeLimit;
        }
      }

      let routeQuery = this.$route.query;
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
     * Fired when a date time picker is closed
     * Sets the time range updated flag to false so
     * validate date knows that a date was changed
     * rather than the date range input
     */
    closePicker: function () {
      // start or stop time was udpated, so set the timerange to custom
      this.timeRange = '0';
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
      this.time.stopTime = Math.floor(msDate / 1000);
      this.validateDate();
    },
    /**
     * Determines whether the supplied time is the start of a day
     * Fired from the previous start time button to determine tooltip text
     * @param {number} time date in seconds from 1970
     */
    isStartOfDay: function (time) {
      const currentTime = moment(time * 1000);
      const startOfDayTime = moment(time * 1000).startOf('day');
      return startOfDayTime.isSame(currentTime, 'seconds');
    },
    /**
     * Determines whether the supplied time is the end of a day
     * Fired from the next end time button to determine tooltip text
     * @param {number} time date in seconds from 1970
     */
    isEndOfDay: function (time) {
      const currentTime = moment(time * 1000);
      const endOfDayTime = moment(time * 1000).endOf('day');
      return endOfDayTime.isSame(currentTime, 'seconds');
    },
    /**
     * Fired when clicking the previous time button on a time input
     * @param {string} startOrStop whether to update the start time or stop time
     */
    prevTime: function (startOrStop) {
      if (startOrStop === 'start') {
        let newTime = moment(this.time.startTime * 1000).startOf('day');
        if (this.isStartOfDay(this.time.startTime)) {
          // it's the beginning of the day, so go to the beginning of the PREV day
          newTime = newTime.subtract(1, 'days');
        }
        this.localStartTime = newTime;
        this.time.startTime = Math.floor(this.localStartTime.valueOf() / 1000);
      } else {
        // stop time always goes to end of day of the previous day
        let newTime = moment(this.time.stopTime * 1000).endOf('day');
        newTime = newTime.subtract(1, 'days');
        this.localStopTime = newTime;
        this.time.stopTime = Math.floor(this.localStopTime.valueOf() / 1000);
      }
      this.validateDate();
    },
    /**
     * Fired when clicking the next time button on a time input
     * @param {string} startOrStop whether to update the start time or stop time
     */
    nextTime: function (startOrStop) {
      if (startOrStop === 'start') {
        // start time always goes to the beginning of the next day
        let newTime = moment(this.time.startTime * 1000).startOf('day');
        newTime = newTime.add(1, 'days');
        this.localStartTime = newTime;
        this.time.startTime = Math.floor(this.localStartTime.valueOf() / 1000);
      } else {
        let newTime = moment(this.time.stopTime * 1000).endOf('day');
        if (this.isEndOfDay(this.time.stopTime)) {
          // it's the end of the day, so go to the end of the NEXT day
          newTime = newTime.add(1, 'days');
        }
        this.localStopTime = newTime;
        this.time.stopTime = Math.floor(this.localStopTime.valueOf() / 1000);
      }
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
      let deltaTime = stopSec - startSec;

      // make sure the time range does not exceed the user setting
      if (this.user.timeLimit) {
        let deltaTimeHrs = deltaTime / 3600;
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
    /**
     * Fired when a key is released from the start time input
     * Closes the start datetimepicker if the key pressed is enter or escape
     * @param {object} key The keyup event
     */
    startDatePickerClose: function (key) {
      if (key.keyCode === 13 || key.keyCode === 27) {
        this.$refs.startTime.dp.hide();
      }
    },
    /**
     * Fired when a key is released from the stop time input
     * Closes the stop datetimepicker if the key pressed is enter or escape
     * @param {object} key The keyup event
     */
    stopDatePickerClose: function (key) {
      if (key.keyCode === 13 || key.keyCode === 27) {
        this.$refs.stopTime.dp.hide();
      }
    },
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

        if (stop !== undefined && start !== undefined && !isNaN(stop) && !isNaN(start)) {
          stop = parseInt(stop, 10);
          start = parseInt(start, 10);

          if (stop < start) {
            this.timeError = 'Stop time cannot be before start time';
          }

          // update the displayed time range
          let deltaTime = stop - start;

          // make sure the time range does not exceed the user setting
          let deltaTimeHrs = deltaTime / 3600;
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
          this.timeRange = '1'; // default to 1 hour
        }
      } else if (!date) {
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
        let deltaTime = newParams.stopTime - newParams.startTime;

        let deltaTimeHrs = deltaTime / 3600;
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
  },
  beforeDestroy: function () {
    $('#stopTime').off('keyup', this.stopDatePickerClose);
    $('#startTime').off('keyup', this.startDatePickerClose);
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

.time-range-control {
  -webkit-appearance: none;
}
</style>
