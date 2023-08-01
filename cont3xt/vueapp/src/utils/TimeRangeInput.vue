<template>
  <b-form inline class="d-flex align-items-center">
    <b-input-group
        :size="inputGroupSize"
        class="mr-2">
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
        class="mr-2">
      <template #prepend>
        <b-input-group-text>
          Stop
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
      default: '152px'
    }
  },
  data () {
    return {
      localStartDate: this.value.startDate,
      localStopDate: this.value.stopDate
    };
  },
  computed: {
    ...mapGetters(['getShiftKeyHold', 'getFocusStartDate']),
    timeRangeInfo: {
      get () { return this.value; },
      set (newVal) {
        this.$emit('input', newVal); // v-model 'input' emitter segment
      }
    }
  },
  watch: {
    getFocusStartDate (val) {
      if (val) { this.$refs.startDate.select(); }
    }
  },
  methods: { /* component methods ------------------------------------------- */
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
