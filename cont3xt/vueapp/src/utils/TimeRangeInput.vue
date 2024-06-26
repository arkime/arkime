<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="d-flex align-items-center">
    <v-btn
      class="mx-1 square-btn"
      tabindex="-1"
      color="secondary"
      >
      <span class="fa fa-lg fa-caret-down" />
        <v-menu activator="parent" location="bottom right">
          <v-card>
            <v-list class="d-flex flex-column">
              <template v-if="currentItype === 'domain'">
                <v-btn @click="snapTo(0)" label="Registration Date" variant="text" />
                <v-divider/>
              </template>

              <v-btn
                v-for="nDays in [1, 2, 3, 7, 14, 30, -1]"
                :key="nDays"
                @click="snapTo(nDays)"
                variant="text"
                :label="(nDays === -1) ? 'All' : `${nDays}`"
              >
                <span v-if="nDays === -1">All</span>
                <span v-else-if="nDays === 1">1 Day</span>
                <span v-else>{{ nDays }} Days</span>
              </v-btn>
            </v-list>
          </v-card>
        </v-menu>
    </v-btn>

    <v-text-field
      density="compact"
      variant="outlined"
      label="Start"
      class="mr-1"
      hide-details
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
    <v-text-field
      density="compact"
      variant="outlined"
      label="End"
      class="mr-1"
      hide-details
      type="text"
      tabindex="0"
      placeholder="Stop Date"
      v-model="localStopDate"
      :style="`width:${inputWidth}`"
      @keyup.up="stopKeyUp(1)"
      @keyup.down="stopKeyUp(-1)"
      @change="updateStopStart('stopDate')"
    />

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
  </div>
</template>

<script setup>
import { parseSeconds } from '@common/vueFilters.js';
import Focus from '@common/Focus.vue';
import { useStore } from 'vuex';
import { useRoute, useRouter } from 'vue-router';
import { useGetters } from '@/vue3-helpers';
import { ref, computed, defineModel, defineProps, onMounted, watch } from 'vue';

/**
 * -- TimeRangeInput --
 * This component handles changing a start/stop date via inputs, while managing the startDate/stopDate query params
 */

// TODO: toby - ensure that [directives: { Focus }, ] functionality remains!

const props = defineProps({
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
});

const timeRangeInfo = defineModel();

const localStartDate = ref(timeRangeInfo.value.startDate); // TODO: toby, ensure this works!!!
const localStopDate = ref(timeRangeInfo.value.stopDate);
const startDateRef = ref(null);

const store = useStore();
const { getShiftKeyHold, getFocusStartDate, getActiveIndicator, getResults } = useGetters(store);

const route = useRoute();
const router = useRouter();

const currentItype = computed(() => getActiveIndicator.value?.itype);
const doneLoading = computed(() => store.state.loading.done);

function snapTo (days) {
  // always update the stop date to now
  const date = new Date();
  localStopDate.value = date.toISOString().slice(0, -5) + 'Z';
  updateStopStart('stopDate');
  localStorage.setItem('snapTo', days);

  if (days && days > 0) { // update start date to <days> ago
    const startMs = date.setDate(date.getDate() - days);
    localStartDate.value = new Date(startMs).toISOString().slice(0, -5) + 'Z';
    updateStopStart('startDate');
  } else if (days === -1) { // update start date to epoch
    localStartDate.value = new Date(0).toISOString().slice(0, -5) + 'Z';
    updateStopStart('startDate');
  } else { // update start date to registration date if it exists
    if (currentItype.value === 'domain' && getResults.value?.domain?.[getActiveIndicator.value?.query]?.['PT Whois']?.registered) {
      localStartDate.value = getResults.value.domain[getActiveIndicator.value.query]['PT Whois'].registered;
      updateStopStart('startDate');
    }
  }
}
function startKeyUp (days) {
  const date = new Date(localStartDate.value);
  const startMs = date.setDate(date.getDate() + days);
  localStartDate.value = new Date(startMs).toISOString().slice(0, -5) + 'Z';
  updateStopStart('startDate');
}
function stopKeyUp (days) {
  const date = new Date(localStopDate.value);
  const stopMs = date.setDate(date.getDate() + days);
  localStopDate.value = new Date(stopMs).toISOString().slice(0, -5) + 'Z';
  updateStopStart('stopDate');
}
function updateStopStart (updated) {
  let startMs = new Date(localStartDate.value).getTime();
  let stopMs = new Date(localStopDate.value).getTime();

  // test for relative times
  if (isNaN(startMs)) {
    startMs = parseSeconds(localStartDate.value) * 1000;
  }
  if (isNaN(stopMs)) {
    stopMs = parseSeconds(localStopDate.value) * 1000;
  }

  // can't do anything if we can't calculate the date ms
  if (isNaN(stopMs) || isNaN(startMs)) { return; }

  const updatedDate = updated === 'startDate' ? localStartDate.value : localStopDate.value;
  // update the query params with the updated value
  if (route.query[updated] !== updatedDate) {
    const query = { ...route.query };
    query[updated] = updatedDate;
    router.push({ query });
  }

  const days = (stopMs - startMs) / (3600000 * 24);

  switch (updated) {
  case 'stopDate':
    localStartDate.value = new Date(stopMs - (3600000 * 24 * days)).toISOString().slice(0, -5) + 'Z';
    localStopDate.value = new Date(stopMs).toISOString().slice(0, -5) + 'Z';
    break;
  case 'startDate':
    localStartDate.value = new Date(startMs).toISOString().slice(0, -5) + 'Z';
    break;
  }

  timeRangeInfo.value = { // syncs time range data with parent
    numDays: Math.round(days),
    numHours: Math.round(days * 24),
    startDate: localStartDate.value,
    stopDate: localStopDate.value,
    startMs,
    stopMs
  };
}

onMounted(() => {
  // set the stop/start date to the query parameters
  if (route.query.stopDate) {
    localStopDate.value = route.query.stopDate;
    updateStopStart('stopDate');
  }
  if (route.query.startDate) {
    localStartDate.value = route.query.startDate;
    updateStopStart('startDate');
  }
});

watch(getFocusStartDate, (val) => {
  if (val) { startDateRef.value?.select(); }
});

watch(doneLoading, (val) => { // snap to the last snapTo value if it exists
  if (val && localStorage.getItem('snapTo')) {
    snapTo(parseInt(localStorage.getItem('snapTo')));
  }
});
</script>

<style scoped>

</style>
