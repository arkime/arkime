// external imports
import qs from 'qs';
import { useRoute } from 'vue-router';
import { BDropdown, BDropdownItem, BDropdownDivider, BCardGroup, BCard, BTooltip } from 'bootstrap-vue-next';
// internal imports
import store from '@/store';
import SessionsService from './SessionsService';
import ArkimeTagSessions from '../sessions/Tags.vue';
import ArkimeRemoveData from '../sessions/Remove.vue';
import ArkimeSendSessions from '../sessions/Send.vue';
import ArkimeExportPcap from '../sessions/ExportPcap.vue';
import ArkimeToast from '../utils/Toast.vue';
import FieldActions from './FieldActions.vue';
import UserService from '../users/UserService';
import ArkimeSessionField from './SessionField.vue';
import HasPermission from '../utils/HasPermission.vue';
import { buildExpression } from '@common/vueFilters.js';

// dl resize variables and functions
let selectedDT; // store selected dt to watch drag and calculate new width
let dtOffset; // store offset width to calculate new width
let selectedGrip; // the column resize grip that is currently being dragged
let siblingDD; // the dd element following the dt element in the dl that is being resized

// fired when a column resize grip is clicked
// stores values for calculations when the grip is unclicked
function gripClick (e, div) {
  e.preventDefault();
  e.stopPropagation();
  selectedDT = div.getElementsByTagName('dt')[0];
  siblingDD = selectedDT.nextElementSibling;
  dtOffset = selectedDT.offsetWidth - e.pageX;
  selectedGrip = div.getElementsByClassName('session-detail-grip')[0];
}

// fired when the column resize grip is dragged
// styles the grip to show where it's being dragged
function gripDrag (e) { // move the grip where the user moves their cursor
  if (selectedDT && selectedGrip) {
    const newWidth = dtOffset + e.pageX;
    selectedGrip.style.borderRight = '1px dotted var(--color-gray)';
    selectedGrip.style.left = `${newWidth}px`;
  }
}

// fired when a clicked and dragged grip is dropped
// updates the column and table width and saves the values
function gripUnclick (e, vueThis) {
  if (selectedDT && selectedGrip) {
    const newWidth = Math.max(dtOffset + e.pageX, 100); // min width is 100px
    selectedDT.style.width = `${newWidth}px`;
    siblingDD.style.marginLeft = `${newWidth + 10}px`;
    selectedGrip.style.left = `${newWidth}px`;
    selectedGrip.style.borderRight = 'none';

    // update all the dt and dd styles to reflect the new width
    for (const dt of document.getElementsByTagName('dt')) {
      dt.style.width = `${newWidth}px`;
      dt.nextElementSibling.style.marginLeft = `${newWidth + 10}px`;
    }

    const labelBtns = document.getElementsByClassName('clickable-label');
    if (labelBtns && labelBtns.length) {
      const btn = labelBtns[0].getElementsByTagName('button');
      if (btn && btn.length) {
        btn[0].style.maxWidth = `${newWidth}px`;
      }
    }

    for (const grip of document.getElementsByClassName('session-detail-grip')) {
      grip.style.left = `${newWidth}px`;
    }

    // save it as a user configuration
    vueThis.saveDLWidth(newWidth);
  }

  selectedGrip = undefined;
  selectedDT = undefined;
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
// then passed to the client as an HTML string, used here as "template"
export default {
  getVueInstance (template, session) {
    return {
      template,
      data () {
        return {
          session,
          form: undefined,
          cluster: undefined,
          message: undefined,
          messageType: undefined,
          fields: store.state.fieldsMap,
          remoteclusters: store.state.remoteclusters,
          route: useRoute()
        };
      },
      components: {
        ArkimeTagSessions,
        ArkimeRemoveData,
        ArkimeSendSessions,
        ArkimeExportPcap,
        ArkimeToast,
        FieldActions,
        ArkimeSessionField,
        BDropdown,
        BDropdownItem,
        BDropdownDivider,
        BCardGroup,
        BCard
      },
      directives: {
        BTooltip,
        HasPermission
      },
      computed: {
        expression: {
          get: function () {
            return store.state.expression;
          },
          set: function (newValue) {
            store.commit('setExpression', newValue);
          }
        },
        startTime: {
          get: function () {
            return store.state.time.startTime;
          },
          set: function (newValue) {
            store.commit('setTime', { startTime: newValue });
          }
        },
        permalink () {
          const id = this.session.id.split(':');
          let prefixlessId = id.length > 1 ? id[1] : id[0];
          if (prefixlessId[1] === '@') {
            prefixlessId = prefixlessId.substr(2);
          }

          const params = {
            expression: `id == ${prefixlessId}`,
            startTime: Math.floor(this.session.firstPacket / 1000),
            stopTime: Math.ceil(this.session.lastPacket / 1000),
            cluster: this.session.cluster,
            openAll: 1
          };

          return `sessions?${qs.stringify(params)}`;
        },
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
        for (const div of sessionDetailDL) {
          // set the width of the session detail div based on user setting
          const grip = document.createElement('div');
          grip.classList.add('session-detail-grip');
          grip.style.left = `${dlWidth}px`;
          div.prepend(grip);
          grip.addEventListener('mousedown', (e) => gripClick(e, div));
        }

        const dts = sessionDetailSection.getElementsByTagName('dt');
        for (const dt of dts) {
          // set the width of the dt and the margin of the dd based on user setting
          dt.style.width = `${this.dlWidth}px`;
          dt.nextElementSibling.style.marginLeft = `${this.dlWidth + 10}px`;
          const labelBtn = dt.getElementsByClassName('clickable-label');
          if (labelBtn && labelBtn.length) {
            const btn = labelBtn[0].getElementsByTagName('button');
            if (btn && btn.length) {
              btn[0].style.maxWidth = `${dlWidth}px`;
            }
          }
        }

        // listen for grip drags
        document.addEventListener('mousemove', gripDrag);
        const self = this; // listen for grip unclicks
        document.addEventListener('mouseup', (e) => gripUnclick(e, self));

        // find all the card titles and add a click listener to toggle the collapse
        const elementsArray = sessionDetailSection.getElementsByClassName('card-title');
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
        toggleLayout (numCols) {
          store.commit('setSessionDetailCols', numCols);
        },
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
        actionFormDone: function (doneMsg, success, reload) {
          this.form = undefined; // clear the form
          const doneMsgType = success ? 'success' : 'warning';

          if (reload) {
            this.$emit('reload', doneMsg, doneMsgType);
            return;
          }

          if (doneMsg) {
            this.message = doneMsg;
            this.messageType = doneMsgType;
          }
        },
        messageDone: function () {
          this.message = undefined;
          this.messageType = undefined;
        },
        addTags: function () {
          this.form = 'add:tags';
        },
        removeTags: function () {
          this.form = 'remove:tags';
        },
        exportPCAP: function () {
          this.form = 'export:pcap';
        },
        removeData: function () {
          this.form = 'remove:data';
        },
        sendSession: function (cluster) {
          this.form = 'send:session';
          this.cluster = cluster;
        },
        /**
         * Adds a rootId expression and applies a new start time
         * @param {string} rootId The root id of the session
         * @param {int} startTime The start time of the session
         */
        allSessions: function (rootId, startTime) {
          startTime = Math.floor(startTime / 1000);

          const fullExpression = `rootId == ${rootId}`;

          this.expression = fullExpression;

          if (this.route.query.startTime) {
            if (this.route.query.startTime < startTime) {
              startTime = this.route.query.startTime;
            }
          }

          this.startTime = startTime;
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
