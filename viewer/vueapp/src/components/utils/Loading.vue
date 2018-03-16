<template>

  <div class="loading">
    <img src="../../assets/watching.gif" />
    <div class="loader circles"></div>
    <div class="loader-section square"></div>
    <div class="im-hootin text-center">I'm hootin</div>
  </div>

</template>

<script>
export default {
  name: 'MolochLoading'
};
</script>

<style scoped>
.loading img {
  display     : flex;
  z-index     : 1499;
  position    : fixed;
  top         : 50%;
  left        : 50%;
  height      : 70px;
  margin-top  : -35px;
  margin-left : -30px;
  opacity     : .7;
}

.loading .im-hootin {
  display     : block;
  width       : 100%;
  z-index     : 1499;
  position    : fixed;
  margin-left : -15px;
  top         : calc(50% + 80px);
  color       : var(--color-gray-dark);
  font-weight : bold;
  opacity     : .7;
}

.loading .loader {
  display       : flex;
  z-index       : 1500;
  position      : relative;
  top           : calc(50% + 24px);
  left          : 50%;
  border-radius : 50%;
}

.loading .loader:before {
  content       : "";
  border-radius : 50%;
  position      : fixed;
}

.loading .loader:after {
  content       : "";
  position      : fixed;
  top           : 15px;
  left          : 15px;
  right         : 15px;
  bottom        : 15px;
  border-radius : 50%;
}

.loading .loader-section {
  position: fixed;
  top     : 50%;
  left    : 50%;
  z-index : 1000;
}

/* use square for circle animation */
.loading .loader-section.square {
  width         : 180px;
  height        : 200px;
  margin        : -90px 0 0 -90px;
  border-radius : 32px;
  background    : var(--color-gray-light);
  opacity       : 0.6;
}

/* spinning circles animation */
.loading .loader.circles {
  position        : fixed;
  width           : 150px;
  height          : 150px;
  margin          : -100px 0 0 -75px;
  border          : var(--px-md) solid transparent;
  border-top-color: var(--color-primary-light);

          animation: spin 2s linear infinite;
  -webkit-animation: spin 2s linear infinite;
}
.loading .loader.circles:before {
  top             : 3px;
  left            : 3px;
  right           : 3px;
  bottom          : 3px;
  border          : var(--px-md) solid transparent;
  border-top-color: var(--color-tertiary-light);

          animation: spin 3s linear infinite;
  -webkit-animation: spin 3s linear infinite;
}
.loading .loader.circles:after {
  border           : var(--px-md) solid transparent;
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
