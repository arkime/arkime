<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="w-100 d-flex mw-100" ref="tagContainer">
    <v-btn
        size="x-small"
        variant="tonal"
        tabindex="0"
        @click="clearTags"
        title="Clear tags"
        class="border-0 px-1 py-0 ma-0 btn-revert-size"
        id="clear-tags"
        v-if="tags.length > 0"
    >
      <v-tooltip activator="#clear-tags" location="top">
        Clear tags
      </v-tooltip>
      <span class="fa fa-trash"/>
    </v-btn>
    <!--    tag display    -->
    <div class="d-flex">
      <span ref="tagRefs" v-for="(tag, index) in tags" :key="index"
            class="bg-error rounded pl-1 ml-1 bold tag no-wrap"
            :class="{ 'd-none': index >= (tags.length - tagsOffScreen) }"
      >
        {{tag}}
        <v-btn
            tabindex="0"
            variant="text"
            size="x-small"
            @click="removeTag(index)"
            title="Remove tag"
            class="border-0 px-1 py-0 ma-0 h-100 btn-revert-width"
        >
          <span class="fa fa-close"/>
        </v-btn>
      </span>
      <span ref="tagOffScreenCounter" id="off-screen-counter"
            class="rounded pl-1 ml-1 bold no-wrap cursor-help"
            :class="{ invisible: tagsOffScreen <= 0 }"
      >
        <span>+ {{ tagsOffScreen }} more</span>
        <interactive-tooltip v-if="!(tagsOffScreen <= 0 && !checkInProgress)"
          target="off-screen-counter" location="bottom left">
          <div class="d-flex flex-row flex-wrap justify-start ma-2 ml-1">
            <div v-for="(tag, index) in tags" :key="index" class="d-flex">
              <span class="bg-error rounded ml-1 pl-1 bold tag no-wrap" v-if="index >= (tags.length - tagsOffScreen)">
                {{tag}}
                <v-btn
                    size="x-small"
                    variant="text"
                    tabindex="0"
                    @click="removeTag(index)"
                    title="Remove tag"
                    class="bg-error border-0 px-1 py-0 ma-0 square-btn-xs"
                >
                  <span class="fa fa-close"/>
                </v-btn>
              </span>
            </div>
          </div>
        </interactive-tooltip>
      </span>
    </div>
    <!--    /tag display    -->
  </div>
</template>

<script>
import InteractiveTooltip from '@/utils/InteractiveTooltip.vue';

export default {
  name: 'TagDisplayLine',
  components: { InteractiveTooltip },
  props: {
    tags: Array,
    removeTag: Function,
    clearTags: Function
  },
  data () {
    return {
      tagsOffScreen: 0,
      resizeObserver: undefined, // ResizeObserver that checks for tags off-screen on resize events
      checkSafeWidthRange: undefined, // object of shape { start: number, end: number }
      checkInProgress: false
    };
  },
  watch: {
    tags () {
      this.checkTagsOffScreen();
    }
  },
  methods: {
    checkTagsOffScreen () {
      // ensure all tags are rendered for a frame (since their widths are needed) before culling
      this.tagsOffScreen = 0;
      this.checkInProgress = true;

      // nextTick for $refs to update
      this.$nextTick(() => {
        const containerRect = this.$refs.tagContainer?.getBoundingClientRect();
        const offScreenCounterRect = this.$refs.tagOffScreenCounter?.getBoundingClientRect();
        const refArr = this.$refs.tagRefs;

        if (!containerRect || !offScreenCounterRect || refArr == null) {
          this.tagsOffScreen = 0;
          this.checkSafeWidthRange = undefined;
          this.checkInProgress = false;
          return; // bail if any of the required elements do not exist
        }

        const counterWidth = offScreenCounterRect.width;
        const maxUsableWidth = containerRect.width - counterWidth;
        let previousFunctionalWidth;
        for (let i = 0; i < refArr.length; i++) {
          const boundRect = refArr[i]?.getBoundingClientRect();
          if (boundRect == null) {
            this.tagsOffScreen = 0;
            this.checkSafeWidthRange = undefined;
            this.checkInProgress = false;
            return; // bail
          }
          const functionalWidth = boundRect.right - containerRect.left;
          if (functionalWidth > maxUsableWidth) { // element does not fit
            this.tagsOffScreen = refArr.length - i;
            this.checkSafeWidthRange = {
              start: i === 0 ? 0 : (previousFunctionalWidth + counterWidth), // border where width will cut off previous
              end: functionalWidth + counterWidth // border where width will show current
            };
            this.checkInProgress = false;
            return;
          }
          previousFunctionalWidth = functionalWidth;
        }
        this.tagsOffScreen = 0;
        this.checkSafeWidthRange = {
          start: refArr.length === 0 ? 0 : (previousFunctionalWidth + counterWidth), // border where width will cut off previous
          end: Number.MAX_VALUE // border where width will show current
        };
        this.checkInProgress = false;
      });
    },
    widthWithinSafeRange (width) {
      return this.checkSafeWidthRange != null &&
          width > this.checkSafeWidthRange.start && width < this.checkSafeWidthRange.end;
    }
  },
  mounted () {
    this.checkTagsOffScreen();
    /**
     * ResizeObserver fires off events very fast during resizes
     * so, the widthWithinSafeRange is used to only run the full check
     * when it is guaranteed to result in changes
     */
    this.$nextTick(() => {
      this.resizeObserver = new ResizeObserver(resizeEntries => {
        // some browsers return a single entry rather than an array
        const resizeEntry = Array.isArray(resizeEntries) ? resizeEntries[0] : resizeEntries;
        const rectWidth = resizeEntry?.contentRect?.width;
        // if invalid rectWidth or failed safe range test, execute check
        if (this.tags.length > 0 && (rectWidth == null || !this.widthWithinSafeRange(rectWidth))) {
          this.checkTagsOffScreen();
        }
      });
      this.resizeObserver.observe(this.$refs.tagContainer);
    });
  },
  beforeUnmount () {
    this.resizeObserver.disconnect();
  }
};
</script>
