<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <!-- search -->
    <div class="d-flex justify-content-between mt-3 mx-4">
      <div class="mr-2 flex-grow-1 ">
        <!-- TODO: toby, had debounce of 400ms -->
        <v-text-field
          autofocus
          prepend-inner-icon="mdi-magnify"
          v-model="searchTerm"
          placeholder="Begin typing to search for users by name, id, or role"
          clearable
        />
      </div>
      <my-pagination
        :page-numbers-visible="4"
        v-model:per-page="perPage"
        v-model:current-page="currentPage"
        :total-items="recordsTotal"
        @per-page-change="perPageChange"
      />
      <div>
        <v-btn
          class="skinny-search-row-btn"
          @click="download"
          color="primary"
          v-tooltip="'Download CSV'"
          title="Download CSV">
          <span class="fa fa-download" />
        </v-btn>
      </div>
    </div> <!-- /search -->

    <!-- error -->
    <div v-if="error"
      class="info-area vertical-center text-monospace">
      <div class="text-danger">
        <span class="fa fa-2x fa-warning" />
        {{ error }}
      </div>
    </div> <!-- /error -->

    <!-- loading -->
    <template v-if="loading">
      <slot name="loading">
        <div class="text-center mt-5">
          <span class="fa fa-2x fa-spin fa-spinner" />
          <br />
          Loading...
        </div>
      </slot>
    </template> <!-- /loading -->

    <!-- users table -->
    <div v-if="!error">
        <!-- hide-default-footer -->

      <v-data-table
        v-model:expanded="expandedUserIds"
        show-expanded
        item-value="userId"
        hover
        class="table-striped"
        :loading="loading"
        :headers="headers"
        :items="users"
        v-model:sort-by="sortBy"
        :no-data-text="searchTerm ? 'No users or roles match your search' : 'No users or roles'"
        :items-per-page="-1"
        hide-default-footer
      >
        <!-- toggle column -->
        <template #item.toggle="{ item, internalItem, isExpanded, toggleExpand }">
          <span :class="{'btn-indicator':!item.emailSearch || !item.removeEnabled || !item.packetSearch || item.hideStats || item.hideFiles || item.hidePcap || item.disablePcapDownload || item.timeLimit || item.expression}">
            <!-- TODO: toby use v-toggle -->
            <ToggleBtn
            class="btn-toggle-user"
            @toggle="toggleExpand(internalItem)"
            :opened="isExpanded(internalItem)"
            :class="{expanded: isExpanded(internalItem)}"
            :id="`user-togglebtn-${item.userId}`"
            />
            <v-tooltip v-if="!item.emailSearch || !item.removeEnabled || !item.packetSearch || item.hideStats || item.hideFiles || item.hidePcap || item.disablePcapDownload || item.timeLimit || item.expression"
              :target="`user-togglebtn-${item.userId}`"
              :activator="`#user-togglebtn-${item.userId}`"
              >
              This user has additional restricted permissions
            </v-tooltip>
          </span>
        </template> <!-- /toggle column -->
        <!-- action column -->
        <template #item.action="{ item }">
          <div class="pull-right">
            <v-btn
              size="small"
              color="primary"
              @click="openSettings(item.userId)"
              v-has-role="{user:currentUser,roles:'arkimeAdmin'}"
              v-if="parentApp === 'Arkime' && isUser(item)"
              v-tooltip="`Arkime settings for ${item.userId}`">
              <span class="fa fa-gear" />
            </v-btn>
            <v-btn
              size="small"
              color="secondary"
              v-if="parentApp === 'Arkime'"
              @click="openHistory(item.userId)"
              v-tooltip="`History for ${item.userId}`">
              <span class="fa fa-history" />
            </v-btn>
            <!-- cancel confirm delete button -->
            <transition name="buttons">
              <v-btn
                size="small"
                color="warning"
                v-tooltip="'Cancel'"
                title="Cancel"
                v-if="confirmDelete[item.userId]"
                @click="toggleConfirmDeleteUser(item.userId)">
                <span class="fa fa-ban" />
              </v-btn>
            </transition> <!-- /cancel confirm delete button -->
            <!-- confirm delete button -->
            <transition name="buttons">
              <v-btn
                size="small"
                color="danger"
                v-tooltip="'Are you sure?'"
                title="Are you sure?"
                v-if="confirmDelete[item.userId]"
                @click="deleteUser(item, data.index)">
                <span class="fa fa-check" />
              </v-btn>
            </transition> <!-- /confirm delete button -->
            <!-- delete button -->
            <transition name="buttons">
              <v-btn
                size="small"
                color="danger"
                v-tooltip:start="`Delete ${item.userId}`"
                :title="`Delete ${item.userId}`"
                v-if="!confirmDelete[item.userId]"
                @click="toggleConfirmDeleteUser(item.userId)">
                <span class="fa fa-trash-o" />
              </v-btn>
            </transition> <!-- /delete button -->
          </div>
        </template> <!-- /action column -->
        <!-- user id column -->
        <template #item.userId="{ value }">
          <div class="mt-1">{{ value }}</div>
        </template> <!-- /user id column -->
        <!-- last used column -->
        <template #item.lastUsed="{ value }">
          <div class="mt-1">{{ value ? (tzDateStr(value, currentUser.settings.timezone || 'local', currentUser.settings.ms)) : 'Never' }}</div>
        </template> <!-- /last used column -->

        <template #item.userName="{ item }">
          <v-text-field
            size="small"
            variant="filled"
            v-model="item.userName"
            @input="userHasChanged(item)"
          />
        </template>

        <template #item.enabled="{ item }">
          <v-checkbox
            class="mt-1"
            data-testid="checkbox"
            v-model="item.enabled"
            @input="userHasChanged(item)"
          />
        </template>

        <template #item.webEnabled="{ item }">
          <v-checkbox
            class="mt-1"
            data-testid="checkbox"
            v-model="item.webEnabled"
            v-if="!item.userId.startsWith('role:')"
            @input="userHasChanged(item)"
          />
        </template>

        <template #item.headerAuthEnabled="{ item }">
          <v-checkbox
            class="mt-1"
            data-testid="checkbox"
            v-model="item.headerAuthEnabled"
            v-if="!item.userId.startsWith('role:')"
            @input="userHasChanged(item)"
          />
        </template>

        <template #item.roles="{ item }">
            <RoleDropdown
              :roles="isUser(item) ? roles : roleAssignableRoles"
              :id="item.userId"
              :selected-roles="item.roles"
              @selected-roles-updated="updateRoles"
            />
        </template>

        <!-- detail row -->
        <template #expanded-row="{ columns, item }">
          <tr>
            <td :colspan="columns.length">
              <div class="m-2">
                <!-- <v-container fluid> -->
                <!--   <b-form-checkbox inline -->
                <!--     data-testid="checkbox" -->
                <!--     :checked="!item.emailSearch" -->
                <!--     v-if="isUser(item)" -->
                <!--     @input="newVal => negativeToggle(newVal, item, 'emailSearch', true)"> -->
                <!--     Disable Arkime Email Search -->
                <!--   </b-form-checkbox> -->
                <!--   <b-form-checkbox inline -->
                <!--     data-testid="checkbox" -->
                <!--     :checked="!item.removeEnabled" -->
                <!--     v-if="isUser(item)" -->
                <!--     @input="newVal => negativeToggle(newVal, item, 'removeEnabled', true)"> -->
                <!--     Disable Arkime Data Removal -->
                <!--   </b-form-checkbox> -->
                <!--   <b-form-checkbox inline -->
                <!--     data-testid="checkbox" -->
                <!--     :checked="!item.packetSearch" -->
                <!--     v-if="isUser(item)" -->
                <!--     @input="newVal => negativeToggle(newVal, item, 'packetSearch', true)"> -->
                <!--     Disable Arkime Hunting -->
                <!--   </b-form-checkbox> -->
                <!--   <b-form-checkbox inline -->
                <!--     data-testid="checkbox" -->
                <!--     v-model="item.hideStats" -->
                <!--     v-if="isUser(item)" -->
                <!--     @input="userHasChanged(item)"> -->
                <!--     Hide Arkime Stats Page -->
                <!--   </b-form-checkbox> -->
                <!--   <b-form-checkbox inline -->
                <!--     data-testid="checkbox" -->
                <!--     v-model="item.hideFiles" -->
                <!--     v-if="isUser(item)" -->
                <!--     @input="userHasChanged(item)"> -->
                <!--     Hide Arkime Files Page -->
                <!--   </b-form-checkbox> -->
                <!--   <b-form-checkbox inline -->
                <!--     data-testid="checkbox" -->
                <!--     v-model="item.hidePcap" -->
                <!--     v-if="isUser(item)" -->
                <!--     @input="userHasChanged(item)"> -->
                <!--     Hide Arkime PCAP -->
                <!--   </b-form-checkbox> -->
                <!--   <b-form-checkbox inline -->
                <!--     data-testid="checkbox" -->
                <!--     v-model="item.disablePcapDownload" -->
                <!--     v-if="isUser(item)" -->
                <!--     @input="userHasChanged(item)"> -->
                <!--     Disable Arkime PCAP Download -->
                <!--   </b-form-checkbox> -->
                <!--   <b-input-group -->
                <!--     size="sm" -->
                <!--     class="mt-2"> -->
                <!--     <template #prepend> -->
                <!--       <b-input-group-text -->
                <!--         v-tooltip="'An Arkime search expression that is silently added to all queries. Useful to limit what data can be accessed (e.g. which nodes or IPs)'"> -->
                <!--         Forced Expression -->
                <!--       </b-input-group-text> -->
                <!--     </template> -->
                <!--     <b-form-input -->
                <!--       v-model="item.expression" -->
                <!--       @input="userHasChanged(item)" -->
                <!--       /> -->
                <!--   </b-input-group> -->
                <!--   <b-input-group -->
                <!--     size="sm" -->
                <!--     class="mt-2 w-25"> -->
                <!--     <template #prepend> -->
                <!--       <b-input-group-text -->
                <!--         v-tooltip="'Restrict the maximum time window of a query'"> -->
                <!--         Query Time Limit -->
                <!--       </b-input-group-text> -->
                <!--     </template> -->
                <!--     <!-- NOTE: can't use b-form-select because it doesn't allow for undefined v-models --> -->
                <!--     <select -->
                <!--       class="form-control" -->
                <!--       v-model="item.timeLimit" -->
                <!--       @change="changeTimeLimit(item)"> -->
                <!--       <option value="1">1 hour</option> -->
                <!--       <option value="6">6 hours</option> -->
                <!--       <option value="24">24 hours</option> -->
                <!--       <option value="48">48 hours</option> -->
                <!--       <option value="72">72 hours</option> -->
                <!--       <option value="168">1 week</option> -->
                <!--       <option value="336">2 weeks</option> -->
                <!--       <option value="720">1 month</option> -->
                <!--       <option value="1440">2 months</option> -->
                <!--       <option value="4380">6 months</option> -->
                <!--       <option value="8760">1 year</option> -->
                <!--       <option value=undefined>All (careful)</option> -->
                <!--     </select> -->
                <!--   </b-input-group> -->
                <!---->
                <!--   <!-- display change password if not a role and -->
                <!--     we're in cont3xt or arkime -->
                <!--     (assumes user is a usersAdmin since only usersAdmin can see this page) --> -->
                <!--     <template v-if="parentApp === 'Cont3xt' || parentApp === 'Arkime'"> -->
                <!--       <form class="row" v-if="isUser(item)"> -->
                <!--         <div class="col-9 mt-4"> -->
                <!--           <!-- new password --> -->
                <!--           <b-input-group -->
                <!--             size="sm" -->
                <!--             class="mt-2" -->
                <!--             prepend="New Password"> -->
                <!--             <b-form-input -->
                <!--               type="password" -->
                <!--               v-model="newPassword" -->
                <!--               autocomplete="new-password" -->
                <!--               @keydown.enter="changePassword" -->
                <!--               placeholder="Enter a new password" -->
                <!--               /> -->
                <!--           </b-input-group> -->
                <!--           <!-- confirm new password --> -->
                <!--           <b-input-group -->
                <!--             size="sm" -->
                <!--             class="mt-2" -->
                <!--             prepend="Confirm Password"> -->
                <!--             <b-form-input -->
                <!--               type="password" -->
                <!--               autocomplete="new-password" -->
                <!--               v-model="confirmNewPassword" -->
                <!--               @keydown.enter="changePassword" -->
                <!--               placeholder="Confirm the new password" -->
                <!--               /> -->
                <!--           </b-input-group> -->
                <!--           <!-- change password button --> -->
                <!--           <v-btn -->
                <!--             size="small" -->
                <!--             class="mt-2" -->
                <!--             color="success" -->
                <!--             @click="changePassword(item.userId)"> -->
                <!--             Change Password -->
                <!--           </v-btn> -->
                <!--         </div> -->
                <!--       </form> -->
                <!--       <span v-else> -->
                <!--         <UserDropdown class="mt-2" label="Role Assigners: " -->
                <!--         :selected-users="item.roleAssigners || []" -->
                <!--         :role-id="item.userId" -->
                <!--         @selected-users-updated="updateRoleAssigners" /> -->
                <!--       </span> -->
                <!--     </template> -->
                <!-- </v-container> -->

              </div>
            </td>
          </tr>
        </template><!-- /detail row -->
      </v-data-table>

      <!-- <b-table -->
      <!--   small -->
      <!--   hover -->
      <!--   striped -->
      <!--   foot-clone -->
      <!--   show-empty -->
      <!--   sort-icon-left -->
      <!--   no-local-sorting -->
      <!--   :items="users" -->
      <!--   :fields="fields" -->
      <!--   class="small-table-font" -->
      <!--   v-model:sort-by="sortBy" -->
      <!--   :empty-text="searchTerm ? 'No users or roles match your search' : 'No users or roles'"> -->
      <!---->
      <!--   <!-- column headers --> -->
      <!--   <template v-slot:head()="data"> -->
      <!--     <span v-tooltip="data.field.help"> -->
      <!--       {{ data.label }} -->
      <!--       <span -->
      <!--         v-if="data.field.key === 'roles'" -->
      <!--         class="fa fa-info-circle fa-lg cursor-help ml-2" -->
      <!--         v-tooltip="'These roles are applied across apps (Arkime, Parliament, WISE, Cont3xt)'" -->
      <!--       /> -->
      <!--       <div class="pull-right" -->
      <!--         v-if="data.field.key === 'action'"> -->
      <!--         <v-btn -->
      <!--           size="small" -->
      <!--           v-if="roles" -->
      <!--           color="success" -->
      <!--           title="Create a new role" -->
      <!--           v-b-modal.create-user-modal -->
      <!--           @click="createMode = 'role'"> -->
      <!--           <span class="fa fa-plus-circle mr-1" /> -->
      <!--           Role -->
      <!--         </v-btn> -->
      <!--         <v-btn -->
      <!--           size="small" -->
      <!--           color="primary" -->
      <!--           title="Create a new user" -->
      <!--           v-b-modal.create-user-modal -->
      <!--           @click="createMode = 'user'"> -->
      <!--           <span class="fa fa-plus-circle mr-1" /> -->
      <!--           User -->
      <!--         </v-btn> -->
      <!--       </div> -->
      <!--     </span> -->
      <!--   </template> <!-- /column headers --> -->
      <!---->
      <!--   <!-- toggle column --> -->
      <!--    <template #cell(toggle)="data"> -->
      <!--     <span :class="{'btn-indicator':!data.item.emailSearch || !data.item.removeEnabled || !data.item.packetSearch || data.item.hideStats || data.item.hideFiles || data.item.hidePcap || data.item.disablePcapDownload || data.item.timeLimit || data.item.expression}"> -->
      <!--       <ToggleBtn -->
      <!--         class="btn-toggle-user" -->
      <!--         @toggle="data.toggleDetails" -->
      <!--         :opened="data.detailsShowing" -->
      <!--         :class="{expanded: data.detailsShowing}" -->
      <!--         v-tooltip:close-on-content-click="!data.item.emailSearch || !data.item.removeEnabled || !data.item.packetSearch || data.item.hideStats || data.item.hideFiles || data.item.hidePcap || data.item.disablePcapDownload || data.item.timeLimit || data.item.expression ? 'This user has additional restricted permissions' : ''" -->
      <!--       /> -->
      <!--     </span> -->
      <!--   </template> <!-- /toggle column --> -->
      <!--   <!-- action column --> -->
      <!--   <template #cell(action)="data"> -->
      <!--     <div class="pull-right"> -->
      <!--       <v-btn -->
      <!--         size="small" -->
      <!--         color="primary" -->
      <!--         @click="openSettings(data.item.userId)" -->
      <!--         v-has-role="{user:currentUser,roles:'arkimeAdmin'}" -->
      <!--         v-if="parentApp === 'Arkime' && isUser(data.item)" -->
      <!--         v-tooltip="`Arkime settings for ${data.item.userId}`"> -->
      <!--         <span class="fa fa-gear" /> -->
      <!--       </v-btn> -->
      <!--       <v-btn -->
      <!--         size="small" -->
      <!--         color="secondary" -->
      <!--         v-if="parentApp === 'Arkime'" -->
      <!--         @click="openHistory(data.item.userId)" -->
      <!--         v-tooltip="`History for ${data.item.userId}`"> -->
      <!--         <span class="fa fa-history" /> -->
      <!--       </v-btn> -->
      <!--       <!-- cancel confirm delete button --> -->
      <!--       <transition name="buttons"> -->
      <!--         <v-btn -->
      <!--           size="small" -->
      <!--           color="warning" -->
      <!--           v-tooltip="'Cancel'" -->
      <!--           title="Cancel" -->
      <!--           v-if="confirmDelete[data.item.userId]" -->
      <!--           @click="toggleConfirmDeleteUser(data.item.userId)"> -->
      <!--           <span class="fa fa-ban" /> -->
      <!--         </v-btn> -->
      <!--       </transition> <!-- /cancel confirm delete button --> -->
      <!--       <!-- confirm delete button --> -->
      <!--       <transition name="buttons"> -->
      <!--         <v-btn -->
      <!--           size="small" -->
      <!--           color="danger" -->
      <!--           v-tooltip="'Are you sure?'" -->
      <!--           title="Are you sure?" -->
      <!--           v-if="confirmDelete[data.item.userId]" -->
      <!--           @click="deleteUser(data.item, data.index)"> -->
      <!--           <span class="fa fa-check" /> -->
      <!--         </v-btn> -->
      <!--       </transition> <!-- /confirm delete button --> -->
      <!--       <!-- delete button --> -->
      <!--       <transition name="buttons"> -->
      <!--         <v-btn -->
      <!--           size="small" -->
      <!--           color="danger" -->
      <!--           v-tooltip:start="`Delete ${data.item.userId}`" -->
      <!--           :title="`Delete ${data.item.userId}`" -->
      <!--           v-if="!confirmDelete[data.item.userId]" -->
      <!--           @click="toggleConfirmDeleteUser(data.item.userId)"> -->
      <!--           <span class="fa fa-trash-o" /> -->
      <!--         </v-btn> -->
      <!--       </transition> <!-- /delete button --> -->
      <!--     </div> -->
      <!--   </template> <!-- /action column --> -->
      <!--   <!-- user id column --> -->
      <!--   <template #cell(userId)="data"> -->
      <!--     <div class="mt-1">{{ data.value }}</div> -->
      <!--   </template> <!-- /user id column --> -->
      <!--   <!-- last used column --> -->
      <!--   <template #cell(lastUsed)="data"> -->
      <!--     <div class="mt-1">{{ data.value ? (tzDateStr(data.value, currentUser.settings.timezone || 'local', currentUser.settings.ms)) : 'Never' }}</div> -->
      <!--   </template> <!-- /last used column --> -->
      <!--   <!-- all other columns --> -->
      <!--   <template #cell()="data"> -->
      <!--     <b-form-input -->
      <!--       size="sm" -->
      <!--       v-model="data.item[data.field.key]" -->
      <!--       v-if="data.field.type === 'text'" -->
      <!--       @input="userHasChanged(data.item)" -->
      <!--     /> -->
      <!--     <b-form-checkbox -->
      <!--       class="mt-1" -->
      <!--       data-testid="checkbox" -->
      <!--       v-model="data.item[data.field.key]" -->
      <!--       v-else-if="data.field.type === 'checkbox'" -->
      <!--       @input="userHasChanged(data.item)" -->
      <!--     /> -->
      <!--     <b-form-checkbox -->
      <!--       class="mt-1" -->
      <!--       data-testid="checkbox" -->
      <!--       v-model="data.item[data.field.key]" -->
      <!--       v-else-if="data.field.type === 'checkbox-notrole' && !data.item.userId.startsWith('role:')" -->
      <!--       @input="userHasChanged(data.item)" -->
      <!--     /> -->
      <!--     <template v-else-if="data.field.type === 'select' && roles && roles.length"> -->
      <!--       <RoleDropdown -->
      <!--         :roles="isUser(data.item) ? roles : roleAssignableRoles" -->
      <!--         :id="data.item.userId" -->
      <!--         :selected-roles="data.item.roles" -->
      <!--         @selected-roles-updated="updateRoles" -->
      <!--       /> -->
      <!--     </template> -->
      <!--   </template> <!-- all other columns --> -->
      <!---->
      <!--   <!-- detail row --> -->
      <!--   <template #row-details="data"> -->
      <!--     <div class="m-2"> -->
      <!--       <b-form-checkbox inline -->
      <!--         data-testid="checkbox" -->
      <!--         :checked="!data.item.emailSearch" -->
      <!--         v-if="isUser(data.item)" -->
      <!--         @input="newVal => negativeToggle(newVal, data.item, 'emailSearch', true)"> -->
      <!--         Disable Arkime Email Search -->
      <!--       </b-form-checkbox> -->
      <!--       <b-form-checkbox inline -->
      <!--         data-testid="checkbox" -->
      <!--         :checked="!data.item.removeEnabled" -->
      <!--         v-if="isUser(data.item)" -->
      <!--         @input="newVal => negativeToggle(newVal, data.item, 'removeEnabled', true)"> -->
      <!--         Disable Arkime Data Removal -->
      <!--       </b-form-checkbox> -->
      <!--       <b-form-checkbox inline -->
      <!--         data-testid="checkbox" -->
      <!--         :checked="!data.item.packetSearch" -->
      <!--         v-if="isUser(data.item)" -->
      <!--         @input="newVal => negativeToggle(newVal, data.item, 'packetSearch', true)"> -->
      <!--         Disable Arkime Hunting -->
      <!--       </b-form-checkbox> -->
      <!--       <b-form-checkbox inline -->
      <!--         data-testid="checkbox" -->
      <!--         v-model="data.item.hideStats" -->
      <!--         v-if="isUser(data.item)" -->
      <!--         @input="userHasChanged(data.item)"> -->
      <!--         Hide Arkime Stats Page -->
      <!--       </b-form-checkbox> -->
      <!--       <b-form-checkbox inline -->
      <!--         data-testid="checkbox" -->
      <!--         v-model="data.item.hideFiles" -->
      <!--         v-if="isUser(data.item)" -->
      <!--         @input="userHasChanged(data.item)"> -->
      <!--         Hide Arkime Files Page -->
      <!--       </b-form-checkbox> -->
      <!--       <b-form-checkbox inline -->
      <!--         data-testid="checkbox" -->
      <!--         v-model="data.item.hidePcap" -->
      <!--         v-if="isUser(data.item)" -->
      <!--         @input="userHasChanged(data.item)"> -->
      <!--         Hide Arkime PCAP -->
      <!--       </b-form-checkbox> -->
      <!--       <b-form-checkbox inline -->
      <!--         data-testid="checkbox" -->
      <!--         v-model="data.item.disablePcapDownload" -->
      <!--         v-if="isUser(data.item)" -->
      <!--         @input="userHasChanged(data.item)"> -->
      <!--         Disable Arkime PCAP Download -->
      <!--       </b-form-checkbox> -->
      <!--       <b-input-group -->
      <!--         size="sm" -->
      <!--         class="mt-2"> -->
      <!--         <template #prepend> -->
      <!--           <b-input-group-text -->
      <!--             v-tooltip="'An Arkime search expression that is silently added to all queries. Useful to limit what data can be accessed (e.g. which nodes or IPs)'"> -->
      <!--             Forced Expression -->
      <!--           </b-input-group-text> -->
      <!--         </template> -->
      <!--         <b-form-input -->
      <!--           v-model="data.item.expression" -->
      <!--           @input="userHasChanged(data.item)" -->
      <!--         /> -->
      <!--       </b-input-group> -->
      <!--       <b-input-group -->
      <!--         size="sm" -->
      <!--         class="mt-2 w-25"> -->
      <!--         <template #prepend> -->
      <!--           <b-input-group-text -->
      <!--             v-tooltip="'Restrict the maximum time window of a query'"> -->
      <!--             Query Time Limit -->
      <!--           </b-input-group-text> -->
      <!--         </template> -->
      <!--         <!-- NOTE: can't use b-form-select because it doesn't allow for undefined v-models --> -->
      <!--         <select -->
      <!--           class="form-control" -->
      <!--           v-model="data.item.timeLimit" -->
      <!--           @change="changeTimeLimit(data.item)"> -->
      <!--           <option value="1">1 hour</option> -->
      <!--           <option value="6">6 hours</option> -->
      <!--           <option value="24">24 hours</option> -->
      <!--           <option value="48">48 hours</option> -->
      <!--           <option value="72">72 hours</option> -->
      <!--           <option value="168">1 week</option> -->
      <!--           <option value="336">2 weeks</option> -->
      <!--           <option value="720">1 month</option> -->
      <!--           <option value="1440">2 months</option> -->
      <!--           <option value="4380">6 months</option> -->
      <!--           <option value="8760">1 year</option> -->
      <!--           <option value=undefined>All (careful)</option> -->
      <!--         </select> -->
      <!--       </b-input-group> -->
      <!---->
      <!--       <!-- display change password if not a role and -->
      <!--            we're in cont3xt or arkime -->
      <!--            (assumes user is a usersAdmin since only usersAdmin can see this page) --> -->
      <!--       <template v-if="parentApp === 'Cont3xt' || parentApp === 'Arkime'"> -->
      <!--         <form class="row" v-if="isUser(data.item)"> -->
      <!--           <div class="col-9 mt-4"> -->
      <!--             <!-- new password --> -->
      <!--             <b-input-group -->
      <!--                 size="sm" -->
      <!--                 class="mt-2" -->
      <!--                 prepend="New Password"> -->
      <!--               <b-form-input -->
      <!--                   type="password" -->
      <!--                   v-model="newPassword" -->
      <!--                   autocomplete="new-password" -->
      <!--                   @keydown.enter="changePassword" -->
      <!--                   placeholder="Enter a new password" -->
      <!--               /> -->
      <!--             </b-input-group> -->
      <!--             <!-- confirm new password --> -->
      <!--             <b-input-group -->
      <!--                 size="sm" -->
      <!--                 class="mt-2" -->
      <!--                 prepend="Confirm Password"> -->
      <!--               <b-form-input -->
      <!--                   type="password" -->
      <!--                   autocomplete="new-password" -->
      <!--                   v-model="confirmNewPassword" -->
      <!--                   @keydown.enter="changePassword" -->
      <!--                   placeholder="Confirm the new password" -->
      <!--               /> -->
      <!--             </b-input-group> -->
      <!--             <!-- change password button --> -->
      <!--             <v-btn -->
      <!--                 size="small" -->
      <!--                 class="mt-2" -->
      <!--                 color="success" -->
      <!--                 @click="changePassword(data.item.userId)"> -->
      <!--               Change Password -->
      <!--             </v-btn> -->
      <!--           </div> -->
      <!--         </form> -->
      <!--         <span v-else> -->
      <!--           <UserDropdown class="mt-2" label="Role Assigners: " -->
      <!--                         :selected-users="data.item.roleAssigners || []" -->
      <!--                         :role-id="data.item.userId" -->
      <!--                         @selected-users-updated="updateRoleAssigners" /> -->
      <!--         </span> -->
      <!--       </template> -->
      <!--     </div> -->
      <!--   </template> <!-- /detail row --> -->
      <!-- </b-table> -->
    </div> <!-- /users table -->

    <!-- create user -->
    <!-- <UserCreate -->
    <!--   :roles="createMode === 'user' ? roles : roleAssignableRoles" -->
    <!--   :create-mode="createMode" -->
    <!--   @user-created="userCreated" -->
    <!-- /> -->

    <!-- TODO: toby - fix how this looks -->
    <!-- messages -->
    <v-alert
      :show="!!msg"
      :model-value="!!msg"
      class="position-fixed fixed-bottom m-0 rounded-0"
      style="z-index: 2000;"
      :color="msgType"
      closable>
      {{ msg }}
    </v-alert> <!-- messages -->
  </div>
</template>

<script>
import HasRole from './HasRole.vue';
import ToggleBtn from './ToggleBtn.vue';
import UserCreate from './UserCreate.vue';
import UserService from './UserService';
import RoleDropdown from './RoleDropdown.vue';
import UserDropdown from './UserDropdown.vue';
import MyPagination from '@/utils/MyPagination.vue';
import { timezoneDateString } from './vueFilters';

let userChangeTimeout;

export default {
  name: 'UsersCommon',
  directives: { HasRole },
  components: {
    ToggleBtn,
    UserCreate,
    RoleDropdown,
    UserDropdown,
    MyPagination
  },
  props: {
    roles: Array,
    parentApp: String,
    currentUser: Object
  },
  data () {
    return {
      error: '',
      msg: '',
      msgType: '',
      loading: true,
      searchTerm: '',
      users: undefined,
      dbUserList: undefined,
      changed: {},
      recordsTotal: 0,
      perPage: 100,
      currentPage: 1,
      sortBy: [{ key: 'userId', order: 'asc' }],
      createMode: 'user',
      // fields: [
      //   { label: '', key: 'toggle', sortable: false },
      //   { label: 'ID', key: 'userId', sortable: true, required: true, help: 'The ID used for login (cannot be changed once created)', thStyle: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
      //   { label: 'Name', key: 'userName', sortable: true, type: 'text', required: true, help: 'Friendly/readable name', thStyle: 'width:250px;white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
      //   { name: 'Enabled', key: 'enabled', sortable: true, type: 'checkbox', help: 'Is the account currently enabled for anything?', thStyle: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
      //   { name: 'Web Interface', key: 'webEnabled', sortable: true, type: 'checkbox-notrole', help: 'Can access the web interface. When off only APIs can be used', thStyle: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
      //   { name: 'Web Auth Header', key: 'headerAuthEnabled', sortable: true, type: 'checkbox-notrole', help: 'Can login using the web auth header. This setting doesn\'t disable the password so it should be scrambled', thStyle: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
      //   { name: 'Roles', key: 'roles', sortable: false, type: 'select', help: 'Roles assigned', thStyle: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
      //   { name: 'Last Used', key: 'lastUsed', sortable: true, type: 'checkbox', help: 'The last time Arkime was used by this account', thStyle: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
      //   { label: '', key: 'action', sortable: false, thStyle: 'width:190px;white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' }
      // ],
      headers: [ // TODO: toby, cleanup?
        { title: '', key: 'toggle', sortable: false },
        { title: 'ID', key: 'userId', sortable: true, required: true, help: 'The ID used for login (cannot be changed once created)', thStyle: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
        { title: 'Name', key: 'userName', sortable: true, type: 'text', required: true, help: 'Friendly/readable name', thStyle: 'width:250px;white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
        { title: 'Enabled', key: 'enabled', sortable: true, type: 'checkbox', help: 'Is the account currently enabled for anything?', thStyle: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
        { title: 'Web Interface', key: 'webEnabled', sortable: true, type: 'checkbox-notrole', help: 'Can access the web interface. When off only APIs can be used', thStyle: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
        { title: 'Web Auth Header', key: 'headerAuthEnabled', sortable: true, type: 'checkbox-notrole', help: 'Can login using the web auth header. This setting doesn\'t disable the password so it should be scrambled', thStyle: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
        { title: 'Roles', key: 'roles', sortable: false, type: 'select', help: 'Roles assigned', thStyle: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
        { title: 'Last Used', key: 'lastUsed', sortable: true, type: 'checkbox', help: 'The last time Arkime was used by this account', thStyle: 'white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' },
        { title: '', key: 'action', sortable: false, thStyle: 'width:190px;white-space:nowrap;text-overflow:ellipsis;vertical-align:middle;' }
      ],
      newPassword: '',
      confirmNewPassword: '',
      confirmDelete: {},
      expandedUserIds: []
    };
  },
  computed: {
    roleAssignableRoles () {
      return this.roles.filter(({ value }) => value !== 'superAdmin' && value !== 'usersAdmin');
    },
    sortField () {
      return this.sortBy?.[0]?.key ?? 'userId';
    },
    sortIsDesc () {
      return this.sortBy?.[0]?.order === 'desc';
    }
  },
  created () {
    this.loadUsers();
  },
  watch: {
    sortBy (newVal, oldVal) {
      this.loadUsers();
    },
    searchTerm () {
      this.loadUsers();
    },
    currentPage () {
      this.loadUsers();
    }
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    tzDateStr (date, tz, ms) {
      return timezoneDateString(date, tz, ms);
    },
    perPageChange (newVal) {
      this.perPage = newVal;
      this.loadUsers(false);
    },
    negativeToggle (newVal, user, field, existing) {
      user[field] = !newVal;
      if (existing) { this.userHasChanged(user); }
    },
    changeTimeLimit (user) {
      if (user.timeLimit === 'undefined') {
        delete user.timeLimit;
      } else {
        user.timeLimit = parseInt(user.timeLimit);
      }

      this.userHasChanged(user);
    },
    updateRoles (roles, userId) {
      const user = this.users.find(u => u.userId === userId);
      user.roles = roles;
      this.userHasChanged(user);
    },
    updateRoleAssigners ({ newSelection }, roleId) {
      const role = this.users.find(u => u.userId === roleId);
      role.roleAssigners = newSelection;
      this.userHasChanged(role);
    },
    normalizeUser (unNormalizedUser) {
      const user = JSON.parse(JSON.stringify(unNormalizedUser));
      // remove _showDetails for user (added by b-table when user row is expanded)
      delete user._showDetails;

      // roles might be undefined, but compare to empty array since toggling on
      // any roles sets roles to an array, and removing that role = empty array
      user.roles ??= [];

      // sort, since order is not meaningful for roles
      user.roles.sort();

      // roleAssigners may be undefined, which should be functionally synonymous with an empty array
      user.roleAssigners ??= [];

      // sort, since order is not meaningful for roleAssigners
      user.roleAssigners.sort();

      // make sure these fields exist or the objects will be different
      // (undefined is the same as false for these fields)
      user.timeLimit ||= undefined;
      user.hidePcap ||= undefined;
      user.hideFiles ||= undefined;
      user.hideStats ||= undefined;
      user.disablePcapDownload ||= undefined;

      user.expanded = undefined; // don't care about expanded field (just for UI)
      user.lastUsed = undefined; // don't compare lastUsed, it might be different if the user is using the UI
      return user;
    },
    isUser (userOrRoleObj) {
      return !userOrRoleObj.userId.startsWith('role:');
    },
    userHasChanged (user) {
      this.changed[user.id] = true;

      if (userChangeTimeout) { clearTimeout(userChangeTimeout); }
      // debounce the input so it only saves after 600ms
      userChangeTimeout = setTimeout(() => {
        userChangeTimeout = null;
        this.updateUser(user);
      }, 600);
    },
    updateUser (user) {
      UserService.updateUser(user).then((response) => {
        this.changed[user.userId] = false;
        this.showMessage({ variant: 'success', message: response.text });

        const oldUser = this.dbUserList.find(u => u.userId === user.userId);
        const currentUserRoleAssignmentChanged =
            user.roleAssigners?.includes(this.currentUser.userId) !== oldUser.roleAssigners?.includes(this.currentUser.userId);

        // update the current user if they were changed or if their assignableRoles should be changed
        if (this.currentUser.userId === user.userId || currentUserRoleAssignmentChanged) {
          this.emitCurrentUserUpdate();
        }
      }).catch((error) => {
        this.showMessage({ variant: 'danger', message: error.text });
      });
    },
    toggleConfirmDeleteUser (id) {
      this.confirmDelete[id] = !this.confirmDelete[id];
    },
    deleteUser (user, index) {
      UserService.deleteUser(user).then((response) => {
        this.users.splice(index, 1);
        this.showMessage({ variant: 'success', message: response.text });
        if (user.roleAssigners?.includes(this.currentUser.userId)) {
          this.emitCurrentUserUpdate(); // update current user if one of their assignable roles is deleted
        }
      }).catch((error) => {
        this.showMessage({ variant: 'danger', message: error.text });
      });
    },
    openSettings (userId) {
      this.$router.push({
        path: '/settings',
        query: {
          ...this.$route.query,
          userId
        }
      });
    },
    openHistory (userId) {
      this.$router.push({
        path: '/history',
        query: {
          ...this.$route.query,
          userId
        }
      });
    },
    changePassword (userId) {
      this.msg = '';

      if (!this.newPassword) {
        this.showMessage({
          variant: 'danger',
          message: 'You must enter a new password'
        });
        return;
      }

      if (!this.confirmNewPassword) {
        this.showMessage({
          variant: 'danger',
          message: 'You must confirm your new password'
        });
        return;
      }

      if (this.newPassword !== this.confirmNewPassword) {
        this.showMessage({
          variant: 'danger',
          message: "Your passwords don't match"
        });
        return;
      }

      const data = {
        newPassword: this.newPassword
      };

      UserService.changePassword(data, userId).then((response) => {
        this.newPassword = null;
        this.confirmNewPassword = null;
        // display success message to user
        this.showMessage({ variant: 'success', message: response.text || 'Updated password!' });
      }).catch((error) => {
        // display error message to user
        this.showMessage({ variant: 'danger', message: error.text || error });
      });
    },
    userCreated (message, user) {
      this.reloadUsers();
      this.$emit('update-roles');
      if (user.roleAssigners?.includes(this.currentUser.userId)) {
        this.emitCurrentUserUpdate(); // update current user if they were made an assigner
      }
      this.$root.$emit('bv::hide::modal', 'create-user-modal');
      this.showMessage({ variant: 'success', message });
    },
    download () {
      const query = this.getUsersQuery();

      UserService.downloadCSV(query).then((response) => {
        // display success message to user
        this.showMessage({ variant: 'success', message: response.text || 'Downloaded!' });
      }).catch((error) => {
        // display error message to user
        this.showMessage({ variant: 'danger', message: error.text || error });
      });
    },
    /* helper functions ---------------------------------------------------- */
    emitCurrentUserUpdate () {
      this.$emit('update-current-user');
    },
    showMessage ({ variant, message }) {
      this.msg = message;
      this.msgType = variant;
      setTimeout(() => {
        this.msg = '';
        this.msgType = '';
      }, 10000);
    },
    getUsersQuery () {
      return {
        desc: this.sortIsDesc,
        length: this.perPage,
        filter: this.searchTerm,
        sortField: this.sortField,
        start: (this.currentPage - 1) * this.perPage
      };
    },
    loadUsers () {
      const query = this.getUsersQuery();

      UserService.searchUsers(query).then((response) => {
        this.error = '';
        this.loading = false;
        this.recordsTotal = response.recordsTotal;
        this.users = JSON.parse(JSON.stringify(response.data));
        // don't modify original list - used for comparing
        this.dbUserList = JSON.parse(JSON.stringify(response.data));
      }).catch((error) => {
        this.loading = false;
        this.error = error.text;
      });
    },
    reloadUsers () {
      const query = {
        desc: this.sortIsDesc,
        length: this.perPage,
        filter: this.searchTerm,
        sortField: this.sortField,
        start: (this.currentPage - 1) * this.perPage
      };

      UserService.searchUsers(query).then((response) => {
        this.error = '';
        this.loading = false;
        this.recordsTotal = response.recordsTotal;
        this.users = JSON.parse(JSON.stringify(response.data));
        // don't modify original list - used for comparing
        this.dbUserList = response.data;
      }).catch((error) => {
        this.loading = false;
        this.error = error.text;
      });
    }
  }
};
</script>

<style scoped>
/* center cell content vertically */
.btn-toggle-user {
  margin-top: 2px;
}

/* indication that a user has additional permissions set */
.btn-indicator .btn-toggle-user:not(.expanded) {
  background: linear-gradient(135deg, var(--primary) 1%, var(--primary) 75%, var(--primary) 75%, var(--dark) 77%, var(--dark) 100%);
}

/* make the roles dropdown text smaller */
.roles-dropdown > button, .users-dropdown > button {
  font-size: 0.8rem;
}

.small-table-font {
  font-size: 0.9rem;
}
</style>
