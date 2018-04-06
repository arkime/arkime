<template>

  <div>

    <!-- http.uri -->
    <div v-if="session['http.uri']">
      <div v-for="value in limitArrayLength(session['http.uri'], limit)"
        :key="value">
        <moloch-session-field
          :value="value"
          :session="session"
          :expr="'http.uri'"
          :field="fieldMap['http.uri']">
         </moloch-session-field>
      </div>
      <a class="cursor-pointer"
        style="text-decoration:none;"
        v-if="session['http.uri'].length > initialLimit"
        @click="toggleShowAll()">
        <span v-if="!showAll">
          more...
        </span>
        <span v-else>
          ...less
        </span>
      </a>
    </div> <!-- /http.uri -->

    <!-- email.src -->
    <div v-if="session['email.src']">
      <strong>From:</strong>
      <span v-for="value in session['email.src']"
        :key="value">
        <moloch-session-field
          :value="value"
          :session="session"
          :expr="'email.src'"
          :field="fieldMap['email.src']">
        </moloch-session-field>
      </span>
      <br>
      <div v-if="session['email.dst']">
        <strong>To:</strong>
        <span v-for="value in session['email.dst']"
          :key="value">
          <moloch-session-field
            :value="value"
            :session="session"
            :expr="'email.dst'"
            :field="fieldMap['email.dst']">
          </moloch-session-field>
        </span>
        <br>
      </div>
      <div v-if="session['email.subject']">
        <strong>Subject:</strong>
        <span v-for="value in session['email.subject']"
          :key="value">
          <moloch-session-field
            :value="value"
            :session="session"
            :expr="'email.subject'"
            :field="fieldMap['email.subject']">
          </moloch-session-field>
        </span>
        <br>
      </div>
      <div v-if="session['email.filename']">
        <strong>Files:</strong>
        <span v-for="value in session['email.filename']"
          :key="value">
          <moloch-session-field
            :value="value"
            :session="session"
            :expr="'email.fn'"
            :field="fieldMap['email.fn']">
          </moloch-session-field>
        </span>
        <br>
      </div>
    </div> <!-- /email.src -->

    <!-- dns.host -->
    <div v-if="session['dns.host']">
      <span v-for="value in limitArrayLength(session['dns.host'], limit)"
        :key="value">
        <moloch-session-field
          :value="value"
          :session="session"
          :expr="'dns.host'"
          :field="fieldMap['host.dns']">
         </moloch-session-field>
      </span>
      <a class="cursor-pointer"
        style="text-decoration:none;"
        v-if="session['dns.host'].length > initialLimit"
        @click="toggleShowAll()">
        <span v-if="!showAll">
          more...
        </span>
        <span v-else>
          ...less
        </span>
      </a>
    </div> <!-- /dns.host -->

    <!-- cert -->
    <div v-if="session.cert"
      v-for="(cert, key, index) in session.cert"
      :key="index">
      <span v-for="value in cert.subjectCN"
        :key="value">
        <moloch-session-field
          :value="value"
          :session="session"
          :expr="'cert.subject.cn'"
          :field="fieldMap['cert.subject.cn']">
        </moloch-session-field>
      </span>
      <span v-if="cert.alt">
        <div v-for="(value, key) in limitArrayLength(cert.alt, limit)"
          :key="key">
          <span v-if="key === 0">[</span>
          <moloch-session-field
            :value="value"
            :session="session"
            :expr="'cert.alt'"
            :field="fieldMap['cert.alt']">
          </moloch-session-field>
          <span v-if="key === cert.alt.length - 1 || (!showAll && key === initialLimit - 1)">
            <a class="cursor-pointer"
              style="text-decoration:none;"
              v-if="cert.alt.length > initialLimit"
              @click="toggleShowAll()">
              <span v-if="!showAll">
                more...
              </span>
              <span v-else>
                ...less
              </span>
            </a>
            ]
          </span>
        </div>
      </span>
    </div> <!-- /cert -->

    <!-- irc.channel -->
    <div v-if="session['irc.channel']">
      <strong>Channel:</strong>
      <span v-for="value in session['irc.channel']"
        :key="value">
        <moloch-session-field
          :value="value"
          :session="session"
          :expr="'irc.channel'"
          :field="fieldMap['irc.channel']">
        </moloch-session-field>
      </span>
    </div> <!-- /irc.channel -->

  </div>

</template>

<script>
export default {
  name: 'MolochSessionInfo',
  props: [
    'field', // the field object that describes the field
    'session' // the session object
  ],
  data: function () {
    return {
      limit: 3,
      initialLimit: 3,
      showAll: false
    };
  },
  computed: {
    fieldMap: function () {
      if (!this.field || !this.field.children) { return {}; }

      let map = {};
      for (let field of this.field.children) {
        map[field.exp] = field;
      }

      return map;
    }
  },
  methods: {
    toggleShowAll: function () {
      this.showAll = !this.showAll;

      if (this.showAll) {
        this.limit = 9999;
      } else {
        this.limit = 3;
      }
    },
    limitArrayLength: function (array, length) {
      let limitCount = parseInt(length, 10);

      if (limitCount <= 0) {
        return array;
      }

      return array.slice(0, limitCount);
    }
  }
};
</script>

<style scoped>
</style>
