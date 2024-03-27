<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <b-form inline class="d-flex align-items-center">
    <b-dropdown
      class="mr-1"
      :size="inputGroupSize"
      v-b-tooltip.hover="'Snap To'">
      <template v-if="currentItype === 'domain'">
        <b-dropdown-item @click="snapTo(0)">Registration Date</b-dropdown-item>
        <b-dropdown-divider></b-dropdown-divider>
      </template>
      <b-dropdown-item @click="snapTo(1)">1 Day</b-dropdown-item>
      <b-dropdown-item @click="snapTo(2)">2 Days</b-dropdown-item>
      <b-dropdown-item @click="snapTo(3)">3 Days</b-dropdown-item>
      <b-dropdown-item @click="snapTo(7)">7 Days</b-dropdown-item>
      <b-dropdown-item @click="snapTo(14)">14 Days</b-dropdown-item>
      <b-dropdown-item @click="snapTo(30)">30 Days</b-dropdown-item>
      <b-dropdown-item @click="snapTo(-1)">All</b-dropdown-item>
    </b-dropdown>
    <b-input-group
      class="mr-1"
      :size="inputGroupSize">
      <template #prepend>
        <b-input-group-text>
          <span v-if="!getShiftKeyHold">
            Start
          </span>
          <span v-else
          class="start-time-shortcut">
            T
          </span>
        </b-input-group-text>
      </template>
      <b-form-input
          type="text"
          tabindex="0"
          ref="startDate"
          v-model="localStartDate"
          :style="`width:${inputWidth}`"
          placeholder="Start Date"
          v-focus="getFocusStartDate"
          @keyup.up="startKeyUp(1)"
          @keyup.down="startKeyUp(-1)"
          @change="updateStopStart('startDate')"
      />
    </b-input-group>
    <b-input-group
        :size="inputGroupSize"
        class="mr-1">
      <template #prepend>
        <b-input-group-text>
          End
        </b-input-group-text>
      </template>
      <b-form-input
          type="text"
          tabindex="0"
          v-model="localStopDate"
          :style="`width:${inputWidth}`"
          placeholder="Stop Date"
          @keyup.up="stopKeyUp(1)"
          @keyup.down="stopKeyUp(-1)"
          @change="updateStopStart('stopDate')"
      />
    </b-input-group>
    <span class="text-nowrap">
      <span class="fa fa-lg fa-question-circle cursor-help mt-1"
            v-b-tooltip.hover.html="placeHolderTip"
      />
      <span class="pl-1">
        {{ timeRangeInfo.numDays }} days | {{ timeRangeInfo.numHours }} hours
      </span>
    </span>
  </b-form>
</template>

<script>
import { mapGetters } from 'vuex';
import Focus from '@/../../../common/vueapp/Focus';

/**
 * -- TimeRangeInput --
 * This component handles changing a start/stop date via inputs, while managing the startDate/stopDate query params
 */
export default {
  name: 'TimeRangeInput',
  directives: { Focus },
  props: {
    value: { // v-model 'value' segment
      type: Object,
      required: true
    },
    placeHolderTip: { // (Question mark hover text) -- shape of { title: String }
      type: Object,
      required: true
    },
    inputGroupSize: {
      type: String,
      default: 'xs'
    },
    inputWidth: {
      type: String,
      default: '138px'
    }
  },
  data () {
    return {
      localStartDate: this.value.startDate,
      localStopDate: this.value.stopDate
    };
  },
  computed: {
    ...mapGetters(['getShiftKeyHold', 'getFocusStartDate', 'getActiveIndicator', 'getResults']),
    timeRangeInfo: {
      get () { return this.value; },
      set (newVal) {
        this.$emit('input', newVal); // v-model 'input' emitter segment
      }
    },
    currentItype () {
      return this.getActiveIndicator?.itype;
    },
    doneLoading () {
      return this.$store.state.loading.done;
    }
  },
  watch: {
    getFocusStartDate (val) {
      if (val) { this.$refs.startDate.select(); }
    },
    doneLoading (val) { // snap to the last snapTo value if it exists
      if (val && localStorage.getItem('snapTo')) {
        this.snapTo(parseInt(localStorage.getItem('snapTo')));
      }
    }
  },
  methods: { /* component methods ------------------------------------------- */
    snapTo (days) {
      // always update the stop date to now
      const date = new Date();
      this.localStopDate = date.toISOString().slice(0, -5) + 'Z';
      this.updateStopStart('stopDate');
      localStorage.setItem('snapTo', days);

      if (days && days > 0) { // update start date to <days> ago
        const startMs = date.setDate(date.getDate() - days);
        this.localStartDate = new Date(startMs).toISOString().slice(0, -5) + 'Z';
        this.updateStopStart('startDate');
      } else if (days === -1) { // update start date to epoch
        this.localStartDate = new Date(0).toISOString().slice(0, -5) + 'Z';
        this.updateStopStart('startDate');
      } else { // update start date to registration date if it exists
        if (this.currentItype === 'domain' && this.getResults?.domain?.[this.getActiveIndicator?.query]?.['PT Whois']?.registered) {
          this.localStartDate = this.getResults.domain[this.getActiveIndicator.query]['PT Whois'].registered;
          this.updateStopStart('startDate');
        }
      }
    },
    startKeyUp (days) {
      const date = new Date(this.localStartDate);
      const startMs = date.setDate(date.getDate() + days);
      this.localStartDate = new Date(startMs).toISOString().slice(0, -5) + 'Z';
      this.updateStopStart('startDate');
    },
    stopKeyUp (days) {
      const date = new Date(this.localStopDate);
      const stopMs = date.setDate(date.getDate() + days);
      this.localStopDate = new Date(stopMs).toISOString().slice(0, -5) + 'Z';
      this.updateStopStart('stopDate');
    },
    updateStopStart (updated) {
      let startMs = new Date(this.localStartDate).getTime();
      let stopMs = new Date(this.localStopDate).getTime();

      // test for relative times
      if (isNaN(startMs)) {
        startMs = this.$options.filters.parseSeconds(this.localStartDate) * 1000;
      }
      if (isNaN(stopMs)) {
        stopMs = this.$options.filters.parseSeconds(this.localStopDate) * 1000;
      }

      // can't do anything if we can't calculate the date ms
      if (isNaN(stopMs) || isNaN(startMs)) { return; }

      const updatedDate = updated === 'startDate' ? this.localStartDate : this.localStopDate;
      // update the query params with the updated value
      if (this.$route.query[updated] !== updatedDate) {
        const query = { ...this.$route.query };
        query[updated] = updatedDate;
        this.$router.push({ query });
      }

      const days = (stopMs - startMs) / (3600000 * 24);

      switch (updated) {
      case 'stopDate':
        this.localStartDate = new Date(stopMs - (3600000 * 24 * days)).toISOString().slice(0, -5) + 'Z';
        this.localStopDate = new Date(stopMs).toISOString().slice(0, -5) + 'Z';
        break;
      case 'startDate':
        this.localStartDate = new Date(startMs).toISOString().slice(0, -5) + 'Z';
        break;
      }

      this.timeRangeInfo = { // syncs time range data with parent
        numDays: Math.round(days),
        numHours: Math.round(days * 24),
        startDate: this.localStartDate,
        stopDate: this.localStopDate,
        startMs,
        stopMs
      };
    }
  },
  mounted () {
    // set the stop/start date to the query parameters
    if (this.$route.query.stopDate) {
      this.localStopDate = this.$route.query.stopDate;
      this.updateStopStart('stopDate');
    }
    if (this.$route.query.startDate) {
      this.localStartDate = this.$route.query.startDate;
      this.updateStopStart('startDate');
    }
  }
};
</script>

<style scoped>

</style>
