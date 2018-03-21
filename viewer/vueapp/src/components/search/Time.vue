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
        <!-- TODO -->
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
          <!-- TODO -->
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
            Time Range: {{ deltaTime | readableTime }}
          </span>
          <span v-if="timeError">
            <span class="fa fa-exclamation-triangle"></span>&nbsp;
            {{ timeError }}
          </span>
        </strong>
      </div>
      <!-- /human readable time range or error -->

  </div>

</template>

<script>
// TODO find date time picker that allows user input AND time
const hourMS = 3600000;
let currentTime = new Date().getTime();

export default {
  name: 'MolochTime',
  props: [ 'timezone', 'hideBounding', 'hideInterval' ],
  data: function () {
    return {
      startTime: null,
      stopTime: null,
      deltaTime: null,
      timeError: '',
      timeRange: 1, // default to last hour
      timeBounding: 'last',
      timeInterval: 'auto',
      dateTimeFormat: 'yyyy/MM/dd HH:mm:ss'
    };
  },
  created: function () {
    this.setupTimeParams(
      this.$route.query.date,
      this.$route.query.startTime,
      this.$route.query.stopTime
    );
  },
  methods: {
    /* exposed page functions ------------------------------------ */
    changeTimeRange: function () {
      this.deltaTime = null;
      this.timeError = false;

      this.$router.push({
        query: {
          ...this.$route.query,
          date: this.timeRange,
          stopTime: null,
          startTime: null
        }
      });
    },
    changeDate: function () {
      this.timeError = false;
      this.timeRange = '0'; // custom time range

      let stopSec = parseInt((this.stopTime / 1000).toFixed());
      let startSec = parseInt((this.startTime / 1000).toFixed());

      // only continue if start and stop are valid numbers
      if (!startSec || !stopSec || isNaN(startSec) || isNaN(stopSec)) {
        return;
      }

      if (stopSec < startSec) { // don't continue if stop < start
        this.timeError = 'Stop time cannot be before start time';
        return;
      }

      // update the displayed time range
      this.deltaTime = this.stopTime - this.startTime;

      this.applyDate();
    },
    changeTimeBounding: function () {
      this.$router.push({
        query: {
          ...this.$route.query,
          bounding: this.timeBounding !== 'last' ? this.timeBounding : null
        }
      });
    },
    changeTimeInterval: function () {
      this.$router.push({
        query: {
          ...this.$route.query,
          bounding: this.timeInterval !== 'auto' ? this.timeInterval : null
        }
      });
    },
    openStartPicker: function () {
      this.$refs.startTime.showCalendar();
    },
    openStopPicker: function () {
      this.$refs.stopTime.showCalendar();
    },
    /* helper functions ------------------------------------------ */
    applyDate: function () {
      this.$router.push({
        query: {
          ...this.$route.query,
          date: null,
          stopTime: parseInt((this.stopTime / 1000).toFixed()),
          startTime: parseInt((this.startTime / 1000).toFixed())
        }
      });
    },
    change: function () {
      let useDateRange = false;

      currentTime = new Date().getTime();

      // build the parameters to send to the parent controller that makes the req
      if (this.timeRange > 0) {
        // if it's not a custom time range or all, update the time
        this.stopTime = currentTime;
        this.startTime = currentTime - (hourMS * this.timeRange);
      }

      if (parseInt(this.timeRange) === -1) { // all time
        this.startTime = hourMS * 5;
        this.stopTime = currentTime;
        useDateRange = true;
      }

      // always use startTime and stopTime instead of date range (except for all)
      // querying with date range causes unexpected paging behavior
      // because there are always new sessions
      if (this.startTime && this.stopTime) {
        let args = {
          bounding: this.timeBounding,
          interval: this.timeInterval
        };

        if (useDateRange) {
          args.date = -1;
        } else {
          args.startTime = (this.startTime / 1000).toFixed();
          args.stopTime = (this.stopTime / 1000).toFixed();
        }

        // TODO
        // this.$scope.$emit('change:time:input', args);
      }
    },
    setupTimeParams: function (date, startTime, stopTime) {
      if (date) { // time range is available
        this.timeRange = date;
        if (parseInt(this.timeRange) === -1) { // all time
          this.startTime = hourMS * 5;
          this.stopTime = currentTime;
        } else if (this.timeRange > 0) {
          this.stopTime = currentTime;
          this.startTime = currentTime - (hourMS * this.timeRange);
        }
      } else if (startTime && stopTime) {
        // start and stop times available
        let stop = parseInt(stopTime * 1000, 10);
        let start = parseInt(startTime * 1000, 10);

        if (stop && start && !isNaN(stop) && !isNaN(start)) {
          // if we can parse start and stop time, set them
          this.timeRange = '0'; // custom time range
          this.stopTime = stop;
          this.startTime = start;
          if (stop < start) {
            this.timeError = 'Stop time cannot be before start time';
          }
          // update the displayed time range
          this.deltaTime = this.stopTime - this.startTime;
        } else { // if we can't parse stop or start time, set default
          this.timeRange = '1'; // default to 1 hour
        }
      } else if (!date && !startTime && !stopTime) {
        // there are no time query parameters, so set defaults
        this.timeRange = '1'; // default to 1 hour
      }
    }
  }
};
</script>

<style scoped>
.time-range-control {
  -webkit-appearance: none;
}
</style>
