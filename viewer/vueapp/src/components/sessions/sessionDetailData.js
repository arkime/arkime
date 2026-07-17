// external imports
import { useRoute } from 'vue-router';
// Vuetify components used by the pug session-detail templates. The
// runtime template compiler can't go through vite-plugin-vuetify's
// auto-import path (that only sees .vue SFCs), so register them on
// the runtime instance's components map.
import { VMenu } from 'vuetify/components/VMenu';
import { VList, VListItem } from 'vuetify/components/VList';
import { VDivider } from 'vuetify/components/VDivider';
// internal imports
import store from '@/store';
import SessionsService from './SessionsService';
import FieldActions from './FieldActions.vue';
import UserService from '../users/UserService';
import ArkimeSessionField from './SessionField.vue';
import { buildExpression } from '@common/vueFilters.js';
import { useColumnResize } from '@common/composables/useColumnResize.js';

const MIN_DL_WIDTH = 100;

// Propagate the committed dt width to every dt/dd/label/grip in the section.
function applyDLWidth (newWidth) {
  for (const dt of document.getElementsByTagName('dt')) {
    dt.style.width = `${newWidth}px`;
    if (dt.nextElementSibling) {
      dt.nextElementSibling.style.marginLeft = `${newWidth + 10}px`;
    }
  }
  // .clickable-label is on the <button> directly post-Vuetify migration
  // (used to be a wrapping <div> from b-dropdown). Set maxWidth on every
  // label button so the dt-aligned text stays clipped to the new width.
  for (const btn of document.getElementsByClassName('clickable-label')) {
    btn.style.maxWidth = `${newWidth}px`;
  }
  for (const grip of document.getElementsByClassName('session-detail-grip')) {
    grip.style.left = `${newWidth}px`;
  }
}

function collapseSection (e) {
  e.target.classList.toggle('collapsed');
  e.target.nextElementSibling.classList.toggle('collapse');
  e.target.parentElement.classList.toggle('collapsed');

  if (localStorage) {
    const collapsed = JSON.parse(localStorage['arkime-detail-collapsed'] || '{}');
    collapsed[e.target.innerText.toLowerCase()] = e.target.classList.contains('collapsed');
    localStorage['arkime-detail-collapsed'] = JSON.stringify(collapsed);
  }
}

// This is the vue instance logic for the session detail template that is returned from the server
// The template is written in pug (/viewer/views/sessionDetail.pug) and rendered on the server
// then passed to the client as an HTML string, used here as "template".
// The session action bar (link/download/tags/etc.) lives in SessionActions.vue;
// the inline "+ add tag" button here emits addTags up to open that form.
export default {
  getVueInstance (template, session) {
    return {
      template,
      data () {
        return {
          session,
          fields: store.state.fieldsMap,
          route: useRoute()
        };
      },
      components: {
        FieldActions,
        ArkimeSessionField,
        VMenu,
        VList,
        VListItem,
        VDivider
      },
      emits: ['toggleColVis', 'toggleInfoVis', 'addTags'],
      computed: {
        dlWidth: {
          get: function () {
            return store.state.sessionDetailDLWidth || 160;
          },
          set: function (newValue) {
            store.commit('setSessionDetailDLWidth', newValue);
          }
        }
      },
      mounted () {
        // add grip to each section of the section detail
        const sessionDetailSection = document.getElementById(`${this.session.id}-detail`);
        if (!sessionDetailSection) { return; }

        const sessionDetailDL = sessionDetailSection.getElementsByTagName('dl');
        const dlWidth = this.dlWidth;
        const self = this;
        const resizer = useColumnResize({
          minWidth: MIN_DL_WIDTH,
          guideSide: 'right'
        });

        for (const div of sessionDetailDL) {
          // set the width of the session detail div based on user setting
          const grip = document.createElement('div');
          grip.classList.add('session-detail-grip');
          grip.style.left = `${dlWidth}px`;
          div.prepend(grip);
          grip.addEventListener('pointerdown', (e) => {
            const dt = div.getElementsByTagName('dt')[0];
            if (!dt) return;
            resizer.startResize(e, dt, grip, (newWidth) => {
              applyDLWidth(newWidth);
              self.saveDLWidth(newWidth);
            });
          });
        }

        const dts = sessionDetailSection.getElementsByTagName('dt');
        for (const dt of dts) {
          // set the width of the dt and the margin of the dd based on user setting
          dt.style.width = `${this.dlWidth}px`;
          dt.nextElementSibling.style.marginLeft = `${this.dlWidth + 10}px`;
          for (const btn of dt.getElementsByClassName('clickable-label')) {
            btn.style.maxWidth = `${dlWidth}px`;
          }
        }

        // find all the card titles and add a click listener to toggle the collapse
        const elementsArray = sessionDetailSection.getElementsByClassName('session-card-title');
        for (const elem of elementsArray) {
          // check if the element was previously collapsed and collapse it
          if (localStorage && localStorage['arkime-detail-collapsed']) {
            const collapsed = JSON.parse(localStorage['arkime-detail-collapsed']);
            if (collapsed[elem.innerText.toLowerCase()]) {
              elem.classList.add('collapsed');
              elem.nextElementSibling.classList.add('collapse');
              elem.parentElement.classList.add('collapsed');
            }
          }

          elem.addEventListener('click', collapseSection);
        }
      },
      methods: {
        /* Saves the dl widths */
        saveDLWidth: function (width) {
          this.dlWidth = width;
          UserService.saveState({ width }, 'sessionDetailDLWidth');
        },
        getField: function (expr) {
          if (!this.fields[expr]) {
            console.log('UNDEFINED', expr);
          }
          return this.fields[expr];
        },
        // opens the add-tag form owned by SessionActions.vue
        addTags: function () {
          this.$emit('addTags');
        },
        /**
         * Opens a new browser tab containing all the unique values for a given field
         * @param {string} fieldID  The field id to display unique values for
         * @param {int} counts      Whether to display the unique values with counts (1 or 0)
         */
        exportUnique: function (fieldID, counts) {
          SessionsService.exportUniqueValues(fieldID, counts, this.route.query);
        },
        /**
         * Opens the spi graph page in a new browser tab
         * @param {string} fieldID The field id (dbField) to display spi graph data for
         */
        openSpiGraph: function (fieldID) {
          SessionsService.openSpiGraph(fieldID, this.route.query);
        },
        /**
         * Shows more items in a list of values
         * @param {object} e The click event
         */
        showMoreItems: function (e) {
          e.target.style.display = 'none';
          e.target.previousSibling.style.display = 'inline';
        },
        /**
         * Hides more items in a list of values
         * @param {object} e The click event
         */
        showFewerItems: function (e) {
          e.target.parentElement.style.display = 'none';
          e.target.parentElement.nextElementSibling.style.display = 'inline';
        },
        /**
         * Toggles a column in the sessions table
         * @param {string} fieldID  The field id to toggle in the sessions table
         */
        toggleColVis: function (fieldID) {
          this.$emit('toggleColVis', fieldID);
        },
        /**
         * Toggles a field's visibility in the info column
         * @param {string} fieldID  The field id to toggle in the info column
         */
        toggleInfoVis: function (fieldID) {
          this.$emit('toggleInfoVis', fieldID);
        },
        /**
         * Adds field == EXISTS! to the search expression
         * @param {string} field  The field name
         * @param {string} op     The relational operator
         */
        fieldExists: function (field, op) {
          const fullExpression = buildExpression(field, 'EXISTS!', op);
          store.commit('addToExpression', { expression: fullExpression });
        }
      }
    };
  }
};
