<template>
  <div>
    <div
      data-testid="role-test-1"
      v-has-role="{user:{roles:['arkimeUser']},roles:'arkimeUser'}">
      has role test
    </div>
    <div
      data-testid="role-test-2"
      v-has-role="{user:{roles:['cont3xtUser']},roles:'arkimeUser'}">
      has role test
    </div>
    <div
      data-testid="role-test-3"
      v-has-role="{user:{roles:['arkimeUser']},roles:'cont3xtUser,arkimeUser'}">
      has role test
    </div>
  </div>
</template>
