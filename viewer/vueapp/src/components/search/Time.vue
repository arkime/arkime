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
    <div class="form-group ml-1">
      <div class="input-group input-group-sm">
        <span class="input-group-prepend cursor-help"
          placement="topright"
          v-b-tooltip.hover
          title="Beginning Time">
          <span class="input-group-text">
            Start
          </span>
        </span>
        <flat-pickr v-model="startTime"
          @on-change="changeStartTime"
          @on-close="validateDate"
          :config="startTimeConfig"
          class="form-control"
          name="startTime"
          ref="startTime">
        </flat-pickr>
        <div class="input-group-append">
          <button class="btn btn-default"
            @click="openStartPicker"
            type="button">
            <span class="fa fa-calendar-o"></span>
          </button>
        </div>
      </div>
    </div> <!-- /start time -->

      <!-- stop time -->
      <div class="form-group ml-1">
        <div class="input-group input-group-sm">
          <span class="input-group-prepend cursor-help"
            placement="topright"
            v-b-tooltip.hover
            title="Stop Time">
            <span class="input-group-text">
              End
            </span>
          </span>
          <flat-pickr v-model="stopTime"
            @on-change="changeStopTime"
            @on-close="validateDate"
            :config="stopTimeConfig"
            class="form-control"
            name="stopTime"
            ref="stopTime">
          </flat-pickr>
          <div class="input-group-append">
            <button class="btn btn-default"
              @click="openStopPicker"
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

export default {
  name: 'MolochTime',
  components: { flatPickr },
  props: [ 'timezone', 'hideBounding', 'hideInterval' ],
  data: function () {
    return {
      startTime: null,
      stopTime: null,
      deltaTime: null,
      timeError: '',
      timeRange: 1, // default to last hour
      timeBounding: this.$route.query.bounding || 'last',
      timeInterval: this.$route.query.interval || 'auto',
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
    '$route.query.date': 'updateParams',
    '$route.query.startTime': 'updateParams',
    '$route.query.stopTime': 'updateParams',
    '$route.query.interval': 'updateParams',
    '$route.query.bounding': 'updateParams'
  },
  created: function () {
    this.setCurrentTime();

    this.setupTimeParams(
      this.$route.query.date,
      this.$route.query.startTime,
      this.$route.query.stopTime
    );

    // TODO listen for time update events from other components

    // calculate initial startTime and stopTime
    this.updateStartStopTime();
  },
  methods: {
    /* exposed page functions ------------------------------------ */
    /* Fired when the time range value changes */
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

      this.notifyParent();
    },
    /* Fired when start datetime is changed */
    changeStartTime: function (selectedDates, dateStr, instance) {
      this.startTime = dateStr;
    },
    /* Fired when stop datetime is changed */
    changeStopTime: function (selectedDates, dateStr, instance) {
      this.stopTime = dateStr;
    },
    /**
     * Validates a date and updates delta time (stop time - start time)
     * TODO Fired when a date value is changed (with 500 ms delay)
     */
    validateDate: function () {
      this.timeError = '';
      this.timeRange = '0'; // custom time range

      let stopSec = parseInt(this.stopTime, 10);
      let startSec = parseInt(this.startTime, 10);

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

      this.applyDate();
      this.notifyParent();
    },
    /**
     * Fired when change bounded select box is changed
     * Applies the timeBounding url parameter
     * Updating the url parameter triggers $routeUpdate which notifies the parent
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
     * Updating the url parameter triggers $routeUpdate which notifies the parent
     */
    changeTimeInterval: function () {
      this.$router.push({
        query: {
          ...this.$route.query,
          interval: this.timeInterval !== 'auto' ? this.timeInterval : undefined
        }
      });
    },
    /* Fired when the start time date picker button is clicked */
    openStartPicker: function () {
      this.$refs.startTime.fp.toggle();
    },
    /* Fired when the stop time date picker button is clicked */
    openStopPicker: function () {
      this.$refs.stopTime.fp.toggle();
    },
    /**
     * Fired a date is changed or when search button or enter is clicked
     * Updates the date, stopTime, and startTime url parameters
     */
    applyDate: function () {
      this.$router.push({
        query: {
          ...this.$route.query,
          date: undefined,
          stopTime: this.stopTime,
          startTime: this.startTime
        }
      });
    },
    /* helper functions ------------------------------------------ */
    /**
     * Fired on init and when search parameters for date, stopTime, startTime,
     * bounding, and interval are changed
     * Calculates the new date or stopTime and startTime
     * Emits an event for the parent to catch
     */
    updateStartStopTime: function () {
      this.setCurrentTime();

      // build the parameters to send to the parent controller that makes the req
      if (this.timeRange > 0) {
        // if it's not a custom time range or all, update the time
        this.stopTime = currentTimeSec.toString();
        this.startTime = (currentTimeSec - (hourSec * this.timeRange)).toString();
      }

      if (parseInt(this.timeRange, 10) === -1) { // all time
        this.startTime = (hourSec * 5).toString();
        this.stopTime = (currentTimeSec).toString();
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
          this.startTime = (hourSec * 5).toString();
          this.stopTime = currentTimeSec.toString();
        } else if (this.timeRange > 0) {
          this.stopTime = currentTimeSec.toString();
          this.startTime = (currentTimeSec - (hourSec * this.timeRange)).toString();
        }
      } else if (startTime && stopTime) {
        // start and stop times available
        let stop = stopTime;
        let start = startTime;

        if (stop && start && !isNaN(stop) && !isNaN(start)) {
          // if we can parse start and stop time, set them
          this.timeRange = '0'; // custom time range
          this.stopTime = stop;
          this.startTime = start;

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
    updateParams: function () {
      let queryParams = this.$route.query;

      if (queryParams.bounding !== this.timeBounding) {
        this.timeBounding = queryParams.bounding || 'last';
      }

      if (queryParams.interval !== this.timeInterval) {
        this.timeInterval = queryParams.interval || 'auto';
      }

      this.setupTimeParams(queryParams.date, queryParams.startTime, queryParams.stopTime);
      this.updateStartStopTime();
      this.notifyParent();
    },
    // Sets the current time in seconds
    setCurrentTime: function () {
      currentTimeSec = Math.floor(new Date().getTime() / 1000);
    },
    /**
     * Emits an event for the parent to catch
     * Always emits start and stop times instead of date range (except for all [-1])
     * because querying with date range causes unexpected paging behavior as there are always new sessions
     */
    notifyParent: function () {
      let args = {
        bounding: this.timeBounding,
        interval: this.timeInterval
      };

      if (parseInt(this.timeRange, 10) === -1) { // all time
        args.date = -1;
      } else {
        args.startTime = this.startTime;
        args.stopTime = this.stopTime;
      }

      this.$emit('timeChange', args);
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
