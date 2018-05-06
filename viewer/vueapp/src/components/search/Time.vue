<template>

  <div class="form-inline">

    <!-- time range select -->
    <div class="form-group">
      <div class="input-group input-group-sm">
        <span class="input-group-prepend cursor-help"
          placement="topright"
          v-b-tooltip.hover
          title="Time Range">
          <span class="input-group-text">
            <span class="fa fa-clock-o"></span>
          </span>
        </span>
        <select class="form-control time-range-control"
          tabindex="3"
          v-model="timeRange"
          @change="changeTimeRange()">
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
    <div class="form-group ml-1"
      @keydown.enter.esc="closeStartPicker">
      <div class="input-group input-group-sm">
        <span class="input-group-prepend cursor-help"
          placement="topright"
          v-b-tooltip.hover
          title="Beginning Time">
          <span class="input-group-text">
            Start
          </span>
        </span>
        <flat-pickr v-model="localStartTime"
          @on-change="changeStartTime"
          @on-close="validateDate"
          :config="startTimeConfig"
          class="form-control"
          name="startTime"
          ref="startTime"
          tabindex="4">
        </flat-pickr>
        <div class="input-group-append">
          <button class="btn btn-default"
            @click="toggleStartPicker"
            type="button">
            <span class="fa fa-calendar-o"></span>
          </button>
        </div>
      </div>
    </div> <!-- /start time -->

    <!-- stop time -->
    <div class="form-group ml-1"
      @keydown.enter.esc="closeStopPicker">
      <div class="input-group input-group-sm">
        <span class="input-group-prepend cursor-help"
          placement="topright"
          v-b-tooltip.hover
          title="Stop Time">
          <span class="input-group-text">
            End
          </span>
        </span>
        <flat-pickr v-model="localStopTime"
          @on-change="changeStopTime"
          @on-close="validateDate"
          :config="stopTimeConfig"
          class="form-control"
          name="stopTime"
          ref="stopTime"
          tabindex="5">
        </flat-pickr>
        <div class="input-group-append">
          <button class="btn btn-default"
            @click="toggleStopPicker"
            type="button">
            <span class="fa fa-calendar-o"></span>
          </button>
        </div>
      </div>
    </div> <!-- /stop time -->

    <!-- time bounding select -->
    <div class="form-group ml-1"
      v-if="!hideBounding">
      <div class="input-group input-group-sm">
        <span class="input-group-prepend cursor-help"
          placement="topright"
          v-b-tooltip.hover
          title="Time Bounding">
          <span class="input-group-text">
            Bounding
          </span>
        </span>
        <select class="form-control time-range-control"
          v-model="timeBounding"
          tabindex="6"
          @change="changeTimeBounding()">
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
          title="Time Interval">
          <span class="input-group-text">
            Interval
          </span>
        </span>
        <select class="form-control time-range-control"
          v-model="timeInterval"
          tabindex="6"
          @change="changeTimeInterval()">
          <option value="auto">Auto</option>
          <option value="second">Seconds</option>
          <option value="minute">Minutes</option>
          <option value="hour">Hours</option>
          <option value="day">Days</option>
        </select>
      </div>
    </div> <!-- /time interval select -->

    <!-- human readable time range or error -->
    <div class="ml-2 display-inline">
      <strong class="small text-theme-accent no-wrap">
        <span v-if="deltaTime && !timeError">
          Time Range: {{ deltaTime * 1000 | readableTime }}
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
import flatPickr from 'vue-flatpickr-component';
import 'flatpickr/dist/flatpickr.css';

const hourSec = 3600;
let currentTimeSec;
let dateChanged = false;

export default {
  name: 'MolochTime',
  components: { flatPickr },
  props: [ 'timezone', 'hideBounding', 'hideInterval', 'updateTime' ],
  data: function () {
    return {
      deltaTime: null,
      timeError: '',
      timeBounding: this.$route.query.bounding || 'last',
      timeInterval: this.$route.query.interval || 'auto',
      // use start/stop time localized to this component so that the time
      // watcher can compare time values to local (unaffected) start/stop times
      localStartTime: undefined,
      localStopTime: undefined,
      // date configs must be separate
      startTimeConfig: {
        dateFormat: 'U', // seconds from Jan 1, 1970
        wrap: true, // for input groups
        altFormat: 'Y/m/d H:i:S', // 'yyyy/MM/dd HH:mm:ss'
        altInput: true, // input date display differs from model
        allowInput: true, // let user edit the input manually
        enableTime: true, // display time picker
        enableSeconds: true, // display seconds in time picker
        minuteIncrement: 1 // increment minutes by 1 instead of 5 (default)
      },
      stopTimeConfig: {
        dateFormat: 'U', // seconds from Jan 1, 1970
        wrap: true, // for input groups
        altFormat: 'Y/m/d H:i:S', // 'yyyy/MM/dd HH:mm:ss'
        altInput: true, // input date display differs from model
        allowInput: true, // let user edit the input manually
        enableTime: true, // display time picker
        enableSeconds: true, // display seconds in time picker
        minuteIncrement: 1 // increment minutes by 1 instead of 5 (default)
      }
    };
  },
  watch: {
    // watch for the date, startTime, stopTime, interval, and bounding
    // route parameters to change, then update the view
    '$route.query': 'updateParams',
    'updateTime': function (newVal, oldVal) {
      if (newVal) {
        // calculate new stop/start time
        this.updateStartStopTime();
        // tell the parent the time params have changed
        this.$emit('timeChange');
      }
    },
    // watch for other components to update the start and stop time
    'time': {
      deep: true,
      handler (newVal, oldVal) {
        if (newVal && oldVal &&
          (newVal.stopTime !== this.localStopTime ||
          newVal.startTime !== this.localStartTime)) {
          dateChanged = true;
          this.validateDate();
        }
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

      this.$router.push({
        query: {
          ...this.$route.query,
          date: this.timeRange,
          stopTime: undefined,
          startTime: undefined
        }
      });
    },
    /**
     * Fired when start datetime is changed
     * Notes that the date has changed so it can be validated
     */
    changeStartTime: function (selectedDates, dateStr, instance) {
      if (this.time.startTime !== dateStr) {
        dateChanged = true;
        this.time.startTime = this.localStartTime = dateStr;
      }
    },
    /**
     * Fired when stop datetime is changed
     * Notes that the date has changed so it can be validated
     */
    changeStopTime: function (selectedDates, dateStr, instance) {
      if (this.time.stopTime !== dateStr) {
        dateChanged = true;
        this.time.stopTime = this.localStopTime = dateStr;
      }
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
     * Applies the date start/stop time url parameters and removes the date url parameter
     * Updating the url parameter triggers updateParams
     */
    validateDate: function () {
      if (!dateChanged) { return; }
      dateChanged = false;

      this.timeError = '';
      this.timeRange = '0'; // custom time range

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

      this.$router.push({
        query: {
          ...this.$route.query,
          date: undefined,
          stopTime: this.time.stopTime,
          startTime: this.time.startTime
        }
      });
    },
    closeStartPicker: function () {
      this.$refs.startTime.fp.close();
    },
    toggleStartPicker: function () {
      this.$refs.startTime.fp.toggle();
    },
    closeStopPicker: function () {
      this.$refs.stopTime.fp.close();
    },
    toggleStopPicker: function () {
      this.$refs.stopTime.fp.toggle();
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
        this.time.stopTime = this.localStopTime = currentTimeSec.toString();
        this.time.startTime = this.localStartTime = (currentTimeSec - (hourSec * this.timeRange)).toString();
      }

      if (parseInt(this.timeRange, 10) === -1) { // all time
        this.time.startTime = this.localStartTime = (hourSec * 5).toString();
        this.time.stopTime = this.localStopTime = (currentTimeSec).toString();
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
          this.time.startTime = this.localStartTime = (hourSec * 5).toString();
          this.time.stopTime = this.localStopTime = currentTimeSec.toString();
        } else if (this.timeRange > 0) {
          this.time.stopTime = this.localStopTime = currentTimeSec.toString();
          this.time.startTime = this.localStartTime = (currentTimeSec - (hourSec * this.timeRange)).toString();
        }
      } else if (startTime && stopTime) {
        // start and stop times available
        let stop = stopTime;
        let start = startTime;

        if (stop && start && !isNaN(stop) && !isNaN(start)) {
          // if we can parse start and stop time, set them
          this.timeRange = '0'; // custom time range
          this.time.stopTime = this.localStopTime = stop;
          this.time.startTime = this.localStartTime = start;

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
        this.time.stopTime = this.localStopTime = newParams.stopTime;
      }

      if (newParams.startTime && newParams.startTime !== oldParams.startTime) {
        change = true;
        this.time.startTime = this.localStartTime = newParams.startTime;
      }

      if (change) { this.$emit('timeChange'); }
    }
  }
};
</script>

<style>
input.form-control.flatpickr-input {
  font-size: var(--px-lg);
}
</style>

<style scoped>
select.form-control {
  font-size: var(--px-lg);
}

.time-range-control {
  -webkit-appearance: none;
}
</style>
