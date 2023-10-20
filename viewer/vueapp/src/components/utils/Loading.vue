<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="loading">
    <div class="rainbow-container">
      <div class="rainbow"></div>
    </div>
    <img
      v-if="!shiftyEyes"
      @click="bounce"
      :class="{'bouncing':bouncing, 'shifty-eyes': shiftyEyes}"
      :title="shiftyEyes ? 'I\'m watching you' : 'Arkime Logo'"
      src="../../../../../assets/Arkime_Logo_Mark_FullGradient.png"
    />
    <img
      v-else
      @click="bounce"
      src="../../../../../assets/watching.gif"
      :class="{'bouncing':bouncing, 'shifty-eyes': shiftyEyes}"
      :title="shiftyEyes ? 'I\'m watching you' : 'Arkime Logo'"
    />
    <div class="loader-section rectangle"
      :class="{'tall-rectangle':canCancel}">
      <div class="im-hootin text-center">
        <h4 :class="{'blinking':bouncing}">
          I'm hootin
        </h4>
        <div v-if="canCancel"
          class="mt-1">
          <button
            type="button"
            @click="cancel"
            class="btn btn-sm btn-theme-primary">
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
  data () {
    return {
      bouncing: false
    };
  },
  computed: {
    shiftyEyes () {
      return this.$store.state.user.settings.shiftyEyes;
    }
  },
  methods: {
    cancel () {
      this.$emit('cancel');
    },
    bounce () {
      if (this.bouncing) { return; }
      this.bouncing = true;
      setTimeout(() => {
        this.bouncing = false;
      }, 1500);
    }
  }
};
</script>

<style scoped>
.loading img {
  top: 50%;
  left: 50%;
  height: 120px;
  display: flex;
  z-index: 10002;
  position: fixed;
  margin-top: -50px;
  margin-left: -32px;
  animation-duration: 2s;
  transform-origin: bottom;
  animation-iteration-count: infinite;
}

.loading img.shifty-eyes {
  height: 110px;
  margin-top: -40px;
  margin-left: -43px;
}

.loading img.bouncing {
  animation-name: bounce;
  animation-timing-function: cubic-bezier(0.280, 0.840, 0.420, 1);
}

.loading .im-hootin {
  font-weight: bold;
  position: relative;
  top: calc(50% + 60px);
  color: var(--color-foreground-accent);
}
.loading .im-hootin > h4.blinking {
  animation: blinker 0.5s linear infinite;
}

/* taller rectangle to accommodate cancel action */
.loading .tall-rectangle .im-hootin {
  top: calc(50% + 40px);
}

.loading .loader-section {
  top: 50%;
  left: 50%;
  z-index: 1000;
  position: fixed;
}

/* rectangle background */
.loading .loader-section.rectangle {
  width: 180px;
  height: 200px;
  border-radius: 32px;
  margin: -90px 0 0 -90px;
}
.loading .loader-section.rectangle:before {
  left: 0;
  content: "";
  width: 100%;
  height: 100%;
  opacity: 0.5;
  position: absolute;
  border-radius: 32px;
  background-color: var(--color-gray);
}
.loading .loader-section.rectangle.tall-rectangle {
  height: 245px;
}

.loading .rainbow-container {
  width: 110px;
  height: 50px;
  z-index: 10001;
  overflow: hidden;
  position: fixed;
  top: calc(50% - 80px);
  left: calc(50% - 50px);
}
.loading .rainbow {
  width: 100px;
  height: 100px;
  z-index: 10001;
  border-radius: 50%;
  box-sizing: border-box;
  border-style: solid;
  border-top-color: #FFF;
  border-right-color: #FFF;
  border-left-color: transparent;
  border-bottom-color: transparent;
  animation: rainbow 3s ease-in-out infinite, rainbow-color 6s linear infinite;
  transform: rotate(-200deg)
}

@keyframes rainbow {
  0% {
    border-width: 10px;
  }
  25% {
    border-width: 3px;
  }
  50% {
    transform: rotate(115deg);
    border-width: 10px;
  }
  75% {
    border-width: 3px;
  }
  100% {
    border-width: 10px;
  }
}
@keyframes rainbow-color {
  0%  { border-top-color: var(--color-primary-light); border-right-color: var(--color-primary-light); }
  24% { border-top-color: var(--color-primary-light); border-right-color: var(--color-primary-light); }

  25% { border-top-color: var(--color-secondary-light); border-right-color: var(--color-secondary-light); }
  49% { border-top-color: var(--color-secondary-light); border-right-color: var(--color-secondary-light); }

  50% { border-top-color: var(--color-tertiary-light); border-right-color: var(--color-tertiary-light); }
  74% { border-top-color: var(--color-tertiary-light); border-right-color: var(--color-tertiary-light); }

  75% { border-top-color: var(--color-quaternary-light); border-right-color: var(--color-quaternary-light); }
  100% { border-top-color: var(--color-quaternary-light); border-right-color: var(--color-quaternary-light); }
}
@keyframes bounce {
  0%   { transform: scale(1,1)      translateY(0); }
  10%  { transform: scale(1.1,.9)   translateY(0); }
  30%  { transform: scale(.9,1.1)   translateY(-100px); }
  50%  { transform: scale(1.05,.95) translateY(0); }
  57%  { transform: scale(1,1)      translateY(-7px); }
  64%  { transform: scale(1,1)      translateY(0); }
  100% { transform: scale(1,1)      translateY(0); }
}
@keyframes blinker {
  50% { opacity: 0; }
}
</style>
