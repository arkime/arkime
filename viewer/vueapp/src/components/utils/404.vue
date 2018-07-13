<template>

  <div class="moloch-404 container">

  <div class="center-area">

    <img src="watching.gif" />

    <div class="well well-lg">
      <h1>404</h1>
      <h4>Nothing to see here...</h4>
    </div>

    <br/>

    <div class="margined-bottom">
      Maybe you want to search for
      <a href="sessions"
        class="no-decoration">
        sessions</a>?
    </div>

    <div>
      Or, maybe you need some
      <a href="help"
        class="no-decoration">
        help</a>?
    </div>

  </div>

</div>

</template>

<script>
export default {
  name: 'Moloch404'
};
</script>

<style scoped>
.moloch-404 {
  margin-top: 20px;
}

.moloch-404 .center-area > img {
  z-index: 99;
}

.moloch-404 .center-area {
  display: flex;
  align-items: center;
  justify-content: center;
  text-align: center;
  flex-direction: column;
  min-height: 75vh;
}

.moloch-404 .well {
  margin-top: -6px;
  min-width: 25%;
  box-shadow: 4px 4px 10px 0 rgba(0,0,0,0.5);
}

.moloch-404 .well > h1 {
  margin-top: 0;
  color: var(--color-primary);
}
</style>
