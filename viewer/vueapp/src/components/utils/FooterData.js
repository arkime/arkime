import store from '@/store';
import { commaString } from '@real_common/vueFilters.js';

// TODO ECR COMMENT This is the vue instance logic for the session detail template that is returned from the server
// The template is written in pug (/viewer/views/sessionDetail.pug) and rendered on the server
// then passed to the client as an HTML string, used here as "template"
export default {
  getVueInstance (template) {
    return {
      template,
      computed: {
        responseTime () {
          return store.state.responseTime;
        },
        loadingData () {
          return store.state.loadingData;
        }
      },
      methods: {
        commaString
      },
      onMounted () {
        console.log('footer data mounted', template); // TODO ECR REMOVE
      }
    };
  }
};
