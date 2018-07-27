<template>

  <div class="parliament-404 container">

  <div class="center-area">

    <img src="../assets/watching.gif"
      alt="hoot" />

    <div class="well well-lg">
      <h1>404</h1>
      <h4>Nothing to see here...</h4>
    </div>

    <br/>

    <div class="margined-bottom">
      Maybe you want to view your
      <a href=""
        class="no-decoration">
        Parliament</a>?
    </div>

  </div>

</div>

</template>

<script>
export default {
  name: 'Parliament404'
};
</script>

<style scoped>
.parliament-404 {
  margin-top: 20px;
}

.parliament-404 .center-area > img {
  z-index: 99;
}

.parliament-404 .center-area {
  display: flex;
  align-items: center;
  justify-content: center;
  text-align: center;
  flex-direction: column;
  min-height: 75vh;
}

.parliament-404 .well {
  margin-top: -6px;
  min-width: 25%;
  padding: 12px;
  background-color: #F6F6F6;
  border: 1px solid #EEEEEE;
  border-radius: 3px;
  box-shadow: 4px 4px 10px 0 rgba(0,0,0,0.5);
}

.parliament-404 .well > h1 {
  margin-top: 0;
  color: #DB0A65;
}
</style>
