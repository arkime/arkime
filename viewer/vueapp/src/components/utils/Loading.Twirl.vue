<template>

  <div class="loading">
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
  margin-top: -75px;
  margin-left: -40px;
  animation: twirl 2.4s cubic-bezier(0, 0.2, 0.8, 1) infinite;
}

.loading .im-hootin {
  position: relative;
  top: calc(50% + 60px);
  color: var(--color-gray-dark);
  font-weight: bold;
}
/* taller rectangle to accommodate cancel action */
.loading .tall-rectangle .im-hootin {
  top: calc(50% + 40px);
}

.loading .loader {
  display: flex;
  z-index: 1500;
  position: relative;
  top: calc(50% + 24px);
  left: 50%;
  border-radius: 50%;
}

.loading .loader:before {
  content: "";
  border-radius: 50%;
  position: fixed;
}

.loading .loader:after {
  content: "";
  position: fixed;
  top: 15px;
  left: 15px;
  right: 15px;
  bottom: 15px;
  border-radius : 50%;
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

@keyframes twirl {
  0%, 100% {
    animation-timing-function: cubic-bezier(0.5, 0, 1, 0.5);
  }
  0% {
    transform: rotateY(0deg);
  }
  50% {
    transform: rotateY(900deg);
    animation-timing-function: cubic-bezier(0, 0.5, 0.5, 1);
  }
  100% {
    transform: rotateY(1800deg);
  }
}
</style>
