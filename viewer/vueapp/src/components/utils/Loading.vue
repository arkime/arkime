<template>

  <div class="loading">
    <img src="../../assets/watching.gif" />
    <div class="loader circles"></div>
    <div class="loader-section rectangle"
      :class="{'tall-rectangle':canCancel}">
      <div class="im-hootin text-center">
        I'm hootin
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
  props: [ 'canCancel' ],
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
  height: 70px;
  margin-top: -35px;
  margin-left: -30px;
  opacity: .8;
}

.loading .im-hootin {
  position: relative;
  top: calc(50% + 70px);
  color: var(--color-gray-dark);
  font-weight: bold;
}
/* taller rectangle to accommodate cancel action */
.loading .tall-rectangle .im-hootin {
  top: calc(50% + 50px);
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
  background: rgb(200, 200, 200, 0.5);
}
.loading .loader-section.rectangle.tall-rectangle {
  height: 245px;
}

/* spinning circles animation */
.loading .loader.circles {
  position: fixed;
  width: 150px;
  height: 150px;
  margin: -100px 0 0 -75px;
  border: var(--px-md) solid transparent;
  border-top-color: var(--color-primary-light);

          animation: spin 2s linear infinite;
  -webkit-animation: spin 2s linear infinite;
}
.loading .loader.circles:before {
  top: 3px;
  left: 3px;
  right: 3px;
  bottom: 3px;
  border: var(--px-md) solid transparent;
  border-top-color: var(--color-tertiary-light);

          animation: spin 3s linear infinite;
  -webkit-animation: spin 3s linear infinite;
}
.loading .loader.circles:after {
  border: var(--px-md) solid transparent;
  border-top-color : var(--color-quaternary-light);

          animation: spin 1.5s linear infinite;
  -webkit-animation: spin 1.5s linear infinite;
}

/* keyframes */
@-webkit-keyframes spin {
  0% {
    -webkit-transform: rotate(0deg);
            transform: rotate(0deg);
  }
  100% {
    -webkit-transform: rotate(360deg);
            transform: rotate(360deg);
  }
}
@keyframes spin {
  0% {
    -webkit-transform: rotate(0deg);
            transform: rotate(0deg);
  }
  100% {
    -webkit-transform: rotate(360deg);
            transform: rotate(360deg);
  }
}
</style>
