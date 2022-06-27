<template>
  <span>
    <template v-if="highlights != null">
      <span
          v-for="{start, end, highlighted} in spans"
          :class="{highlight: highlighted}"
          :key="`span-${start}-${end}`">{{text.substring(start, end)}}</span>
    </template>
    <template v-else>{{content}}</template>
  </span>
</template>

<script>
export default {
  name: 'HighlightableText',
  props: {
    content: { // content can be any type with a valid .toString() -- or null/undefined
      validator (prop) {
        if (prop == null ||
            typeof prop === 'string' ||
            typeof prop === 'number' ||
            typeof prop === 'boolean' ||
            typeof prop === 'object') return true;
        console.log(`WARNING - unsupported type ${typeof prop} in highlight-text`);
        return false;
      },
      required: true
    },
    // an array of spans (ordered by start, non-overlapping), ex. [{start: 0, end: 2}, {start: 3, end: 7}]
    highlights: {
      type: Array,
      default () {
        return null;
      }
    }
  },
  computed: {
    text () {
      return this.content ? this.content.toString() : undefined;
    },
    spans () {
      if (this.highlights == null) {
        return [{ start: 0, end: this.text.length, highlighted: false }];
      }

      const spanList = [];
      const expandedHighlights = [{ start: -1, end: 0 }, ...this.highlights];
      const correctHighlightStart = (span) => {
        if (span.end > this.text.length) {
          return Math.min(span.start, this.text.length - 3);
        }
        return span.start;
      };

      for (const [i, highlightedSpan] of expandedHighlights.entries()) {
        if (i !== 0) {
          if (highlightedSpan.end > this.text.length) {
            // snaps highlight onto '...' if it surpasses the length of the string -- then stops highlighting
            spanList.push({ highlighted: true, start: correctHighlightStart(highlightedSpan), end: this.text.length });
            break;
          }

          spanList.push({ highlighted: true, ...highlightedSpan });
        }

        const nonHighlightedStart = highlightedSpan.end;
        const nonHighlightedEnd = (i === expandedHighlights.length - 1) ? this.text.length : correctHighlightStart(expandedHighlights[i + 1]);
        if (nonHighlightedStart !== nonHighlightedEnd) {
          spanList.push({ highlighted: false, start: nonHighlightedStart, end: nonHighlightedEnd });
        }
      }
      return spanList;
    }
  }
};
</script>

<style scoped>
.highlight {
  background-color: #ffff00;
  color: black;
}
</style>
