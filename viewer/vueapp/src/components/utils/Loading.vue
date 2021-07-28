<template>

  <div class="loading">
    <div class="rainbow-container">
      <div class="rainbow"></div>
    </div>
    <img
      @click="twirl"
      :class="{'twirling':twirling, 'shifty-eyes': shiftyEyes}"
      :src="shiftyEyes ? 'assets/watching.gif' : 'assets/Arkime_Logo_Mark_FullGradient.png'"
    />
    <div class="loader-section rectangle"
      :class="{'tall-rectangle':canCancel}">
      <div class="im-hootin text-center">
        <h4 :class="{'blinking':twirling}">
          I'm hootin
        </h4>
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
  data () {
    return {
      twirling: false
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
    twirl () {
      this.twirling = true;
      setTimeout(() => {
        this.twirling = false;
      }, 2400);
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
  height: 120px;
  margin-top: -50px;
  margin-left: -32px;
}

.loading img.shifty-eyes {
  height: 110px;
  margin-top: -40px;
  margin-left: -43px;
}

.loading img.twirling {
  animation: twirl 2.4s cubic-bezier(0, 0.2, 0.8, 1) infinite;
}

.loading .im-hootin {
  position: relative;
  top: calc(50% + 60px);
  color: var(--color-gray-dark);
  font-weight: bold;
}
.loading .im-hootin > h4.blinking {
  animation: blinker 0.6s linear infinite;
}

/* taller rectangle to accommodate cancel action */
.loading .tall-rectangle .im-hootin {
  top: calc(50% + 40px);
}

.loading .loader-section {
  position: fixed;
  top: 50%;
  left: 50%;
  z-index: 1000;
}

/* rectangle background */
.loading .loader-section.rectangle {
  width: 180px;
  height: 200px;
  margin: -90px 0 0 -90px;
  border-radius: 32px;
  background: rgb(200, 200, 200, 0.7);
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
  border-style: solid;
  border-top-color: #FFF;
  border-right-color: #FFF;
  border-left-color: transparent;
  border-bottom-color: transparent;
  border-radius: 50%;
  box-sizing: border-box;
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
  0%  { border-top-color: var(--color-primary); border-right-color: var(--color-primary); }
  24% { border-top-color: var(--color-primary); border-right-color: var(--color-primary); }

  25% { border-top-color: var(--color-secondary); border-right-color: var(--color-secondary); }
  49% { border-top-color: var(--color-secondary); border-right-color: var(--color-secondary); }

  50% { border-top-color: var(--color-tertiary); border-right-color: var(--color-tertiary); }
  74% { border-top-color: var(--color-tertiary); border-right-color: var(--color-tertiary); }

  75% { border-top-color: var(--color-quaternary); border-right-color: var(--color-quaternary); }
  99% { border-top-color: var(--color-quaternary); border-right-color: var(--color-quaternary); }
}
@keyframes twirl {
  0%, 50%, 100% {
    animation-timing-function: cubic-bezier(0.5, 0, 1, 0.5);
  }
  0% {
    transform: rotateY(0deg);
  }
  50% {
    transform: rotateY(1080deg);
  }
  100% {
    transform: rotateY(720deg);
  }
}
@keyframes blinker {
  50% { opacity: 0; }
}
</style>
