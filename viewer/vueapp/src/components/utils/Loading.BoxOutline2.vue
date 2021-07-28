<template>

  <div class="loading">
    <svg width="200" height="200" viewBox="0 0 100 100">
      <polyline class="line-cornered stroke-still" points="0,0 100,0 100,100 0,100 0,0" stroke-width="10" fill="none"></polyline>
      <polyline class="line-cornered stroke-animation" points="0,0 100,0 100,100 0,100 0,0" stroke-width="10" fill="none"></polyline>
    </svg>
    <img src="assets/Arkime_Logo_Mark_FullGradient.png" />
    <div class="loader-section rectangle"
      :class="{'tall-rectangle':canCancel}">
      <div class="im-hootin text-center">
        <h4>I'm hootin</h4>
        <div v-if="canCancel"
          class="mt-1">
          <button type="button"
            class="btn btn-sm btn-theme-primary"
            @click="cancel">
            cancel
          </button>
        </div>
      </div>
    </div>
  </div>

</template>

<script>
export default {
  name: 'MolochLoading',
  props: ['canCancel'],
  methods: {
    cancel: function () {
      this.$emit('cancel');
    }
  }
};
</script>

<style scoped>
.loading img {
  display: flex;
  z-index: 1499;
  position: fixed;
  top: 50%;
  left: 50%;
  height: 150px;
  margin-top: -105px;
  margin-left: -40px;
}

.loading .im-hootin {
  position: relative;
  top: calc(50% + 45px);
  color: var(--color-gray-dark);
  font-weight: bold;
}

.loading .loader-section {
  top: 50%;
  left: 50%;
  z-index: 1000;
  position: fixed;
}

/* rectangle background */
.loading .loader-section.rectangle {
  width: 234px;
  height: 234px;
  margin: -117px 0 0 -117px;
  background: rgb(200, 200, 200, 0.7);
}

svg {
  top: 50%;
  left: 50%;
  z-index: 1000;
  position: fixed;
  transform: translate(-50%, -50%) scale(1.3);
}

.stroke-still {
  stroke: rgb(200, 200, 200, 0.7);
}

.stroke-animation {
  animation: stroke-spacing 1.5s ease-in, stroke-color 6s linear;
  animation-iteration-count: infinite;
  animation-delay: 0;
  animation-direction: normal;
  animation-fill-mode: forwards;
  animation-play-state: running;
  transform-origin: center center;
}

@keyframes stroke-spacing {
  0% {
    stroke-dashoffset: 0;
    stroke-dasharray: 100 300;
  }
  25% {
    stroke-dashoffset: 0;
    stroke-dasharray: 200 200;
  }
  50% {
    stroke-dashoffset: -100;
    stroke-dasharray: 200 200;
  }
  75% {
    stroke-dashoffset: -200;
    stroke-dasharray: 200 200;
  }
  100% {
    stroke-dashoffset: -400;
    stroke-dasharray: 100 300;
  }
}

@keyframes stroke-color {
  0%  { stroke: var(--color-primary); }
  24% { stroke: var(--color-primary); }

  25% { stroke: var(--color-secondary); }
  49% { stroke: var(--color-secondary); }

  50% { stroke: var(--color-tertiary); }
  74% { stroke: var(--color-tertiary); }

  75% { stroke: var(--color-quaternary); }
  99% { stroke: var(--color-quaternary); }
}
</style>
