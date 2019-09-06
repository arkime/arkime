<template>

  <!-- settings content -->
  <div class="settings-page">

    <!-- sub navbar -->
    <div class="sub-navbar">
      <span class="sub-navbar-title">
        <span class="fa-stack">
          <span class="fa fa-cogs fa-stack-1x"></span>
          <span class="fa fa-square-o fa-stack-2x"></span>
        </span>&nbsp;
        <span>
          Settings
          <span v-if="displayName">
            for {{ displayName }}
          </span>
        </span>
      </span>
      <div class="pull-right small toast-container">
        <moloch-toast
          class="mr-1"
          :message="msg"
          :type="msgType"
          :done="messageDone">
        </moloch-toast>
      </div>
    </div> <!-- /sub navbar -->

    <!-- loading overlay -->
    <moloch-loading
      v-if="loading">
    </moloch-loading> <!-- /loading overlay -->

    <!-- page error -->
    <moloch-error
      v-if="error"
      :message-html="error"
      class="settings-error">
    </moloch-error> <!-- /page error -->

    <!-- content -->
    <div class="settings-content row"
       v-if="!loading && !error && settings">

      <!-- navigation -->
      <div class="col-xl-2 col-lg-3 col-md-3 col-sm-4 col-xs-12"
        role="tablist"
        aria-orientation="vertical">
        <div class="nav flex-column nav-pills">
          <a class="nav-link cursor-pointer"
            @click="openView('general')"
            :class="{'active':visibleTab === 'general'}">
            <span class="fa fa-fw fa-cog">
            </span>&nbsp;
            General
          </a>
          <a class="nav-link cursor-pointer"
            @click="openView('views')"
            :class="{'active':visibleTab === 'views'}">
            <span class="fa fa-fw fa-eye">
            </span>&nbsp;
            Views
          </a>
          <a class="nav-link cursor-pointer"
            @click="openView('cron')"
            :class="{'active':visibleTab === 'cron'}">
            <span class="fa fa-fw fa-search">
            </span>&nbsp;
            Cron Queries
          </a>
          <a class="nav-link cursor-pointer"
            @click="openView('col')"
            :class="{'active':visibleTab === 'col'}">
            <span class="fa fa-fw fa-columns">
            </span>&nbsp;
            Column Configs
          </a>
          <a class="nav-link cursor-pointer"
            @click="openView('spiview')"
            :class="{'active':visibleTab === 'spiview'}">
            <span class="fa fa-fw fa-eyedropper">
            </span>&nbsp;
            SPI View Configs
          </a>
          <a class="nav-link cursor-pointer"
            @click="openView('theme')"
            :class="{'active':visibleTab === 'theme'}">
            <span class="fa fa-fw fa-paint-brush">
            </span>&nbsp;
            Themes
          </a>
          <a v-if="!multiviewer"
            class="nav-link cursor-pointer"
            @click="openView('password')"
            :class="{'active':visibleTab === 'password'}">
            <span class="fa fa-fw fa-lock">
            </span>&nbsp;
            Password
          </a>
          <a class="nav-link cursor-pointer"
            v-has-permission="'createEnabled'"
            @click="openView('notifiers')"
            :class="{'active':visibleTab === 'notifiers'}">
            <span class="fa fa-fw fa-bell">
            </span>&nbsp;
            Notifiers
          </a>
          <a v-if="!multiviewer"
            class="nav-link cursor-pointer"
            @click="openView('shortcuts')"
            :class="{'active':visibleTab === 'shortcuts'}">
            <span class="fa fa-fw fa-list">
            </span>&nbsp;
            Shortcuts
          </a>
        </div>
      </div> <!-- /navigation -->

      <div class="col-xl-10 col-lg-9 col-md-9 col-sm-8 col-xs-12 settings-right-panel">

        <!-- general settings -->
        <form class="form-horizontal"
          v-if="visibleTab === 'general'"
          id="general">

          <h3>General</h3>

          <hr>

          <!-- timezone -->
          <div class="form-group row">
            <label class="col-sm-3 col-form-label text-right font-weight-bold">
              Timezone Format
            </label>
            <div class="col-sm-9">
              <div class="btn-group">
                <b-form-group>
                  <b-form-radio-group
                    size="sm"
                    buttons
                    @change="updateTime"
                    v-model="settings.timezone">
                    <b-radio value="local"
                      class="btn-radio">
                      Local
                    </b-radio>
                    <b-radio value="localtz"
                      class="btn-radio">
                      Local + Timezone
                    </b-radio>
                    <b-radio value="gmt"
                      class="btn-radio">
                      UTC
                    </b-radio>
                  </b-form-radio-group>
                </b-form-group>
              </div>
              <div class="btn-group">
                <b-form-group>
                  <b-form-checkbox
                    button
                    size="sm"
                    v-b-tooltip.hover
                    class="btn-checkbox"
                    @change="updateTime"
                    v-model="settings.ms"
                    :active="settings.ms"
                    title="(for session and packet timestamps only)">
                    milliseconds
                  </b-form-checkbox>
                </b-form-group>
              </div>
              <label class="ml-4 font-weight-bold text-theme-primary">
                {{ date | timezoneDateString(settings.timezone, settings.ms) }}
              </label>
            </div>
          </div> <!-- /timezone -->

          <!-- session detail format -->
          <div class="form-group row">
            <label class="col-sm-3 col-form-label text-right font-weight-bold">
              Session Detail Format
            </label>
            <div class="col-sm-9">
              <b-form-group>
                <b-form-radio-group
                  size="sm"
                  buttons
                  @change="updateSessionDetailFormat"
                  v-model="settings.detailFormat">
                  <b-radio value="last"
                    v-b-tooltip.hover
                    class="btn-radio">
                    Last Used
                  </b-radio>
                  <b-radio value="natural"
                    v-b-tooltip.hover
                    class="btn-radio">
                    Natural
                  </b-radio>
                  <b-radio value="ascii"
                    v-b-tooltip.hover
                    class="btn-radio">
                    ASCII
                  </b-radio>
                  <b-radio value="utf8"
                    v-b-tooltip.hover
                    class="btn-radio">
                    UTF-8
                  </b-radio>
                  <b-radio value="hex"
                    v-b-tooltip.hover
                    class="btn-radio">
                    Hex
                  </b-radio>
                </b-form-radio-group>
              </b-form-group>
            </div>
          </div> <!-- /session detail format -->

          <!-- number of packets -->
          <div class="form-group row">
            <label class="col-sm-3 col-form-label text-right font-weight-bold">
              Number of Packets
            </label>
            <div class="col-sm-9">
              <b-form-group>
                <b-form-radio-group
                  size="sm"
                  buttons
                  @change="updateNumberOfPackets"
                  v-model="settings.numPackets">
                  <b-radio value="last"
                    v-b-tooltip.hover
                    class="btn-radio">
                    Last Used
                  </b-radio>
                  <b-radio value="50"
                    v-b-tooltip.hover
                    class="btn-radio">
                    50
                  </b-radio>
                  <b-radio value="200"
                    v-b-tooltip.hover
                    class="btn-radio">
                    200
                  </b-radio>
                  <b-radio value="500"
                    v-b-tooltip.hover
                    class="btn-radio">
                    500
                  </b-radio>
                  <b-radio value="1000"
                    v-b-tooltip.hover
                    class="btn-radio">
                    1,000
                  </b-radio>
                  <b-radio value="2000"
                    v-b-tooltip.hover
                    class="btn-radio">
                    2,000
                  </b-radio>
                </b-form-radio-group>
              </b-form-group>
            </div>
          </div> <!-- /number of packets -->

          <!-- show packet timestamp -->
          <div class="form-group row">
            <label class="col-sm-3 col-form-label text-right font-weight-bold">
              Show Packet Timestamps
            </label>
            <div class="col-sm-9">
              <b-form-group>
                <b-form-radio-group
                  size="sm"
                  buttons
                  @change="updateShowPacketTimestamps"
                  v-model="settings.showTimestamps">
                  <b-radio value="last"
                    v-b-tooltip.hover
                    class="btn-radio">
                    Last Used
                  </b-radio>
                  <b-radio value="on"
                    v-b-tooltip.hover
                    class="btn-radio">
                    On
                  </b-radio>
                  <b-radio value="off"
                    v-b-tooltip.hover
                    class="btn-radio">
                    Off
                  </b-radio>
                </b-form-radio-group>
              </b-form-group>
            </div>
          </div> <!-- /show packet timestamp -->

          <!-- issue query on initial page load -->
          <div class="form-group row">
            <label class="col-sm-3 col-form-label text-right font-weight-bold">
              Issue Query on Page Load
            </label>
            <div class="col-sm-9">
              <b-form-group>
                <b-form-radio-group
                  size="sm"
                  buttons
                  @change="updateQueryOnPageLoad"
                  v-model="settings.manualQuery">
                  <b-radio value="false"
                    v-b-tooltip.hover
                    class="btn-radio">
                    Yes
                  </b-radio>
                  <b-radio value="true"
                    v-b-tooltip.hover
                    class="btn-radio">
                    No
                  </b-radio>
                </b-form-radio-group>
              </b-form-group>
            </div>
          </div> <!-- /issue query on initial page load -->

          <!-- session sort -->
          <div class="form-group row">
            <label class="col-sm-3 col-form-label text-right font-weight-bold">
              Sort Sessions By
            </label>
            <div class="col-sm-6">
              <select class="form-control form-control-sm"
                v-model="settings.sortColumn"
                @change="update">
                <option value="last">Last Used</option>
                <option v-for="field in sortableColumns"
                  :key="field.dbField"
                  :value="field.dbField">
                  {{ field.friendlyName }}
                </option>
              </select>
            </div>
            <div class="col-sm-3">
              <b-form-group>
                <b-form-radio-group
                  v-if="settings.sortColumn !== 'last'"
                  size="sm"
                  buttons
                  @change="updateSortDirection"
                  v-model="settings.sortDirection">
                  <b-radio value="asc"
                    v-b-tooltip.hover
                    class="btn-radio">
                    ascending
                  </b-radio>
                  <b-radio value="desc"
                    v-b-tooltip.hover
                    class="btn-radio">
                    descending
                  </b-radio>
                </b-form-radio-group>
              </b-form-group>
            </div>
          </div> <!-- /session sort -->

          <!-- default spi graph -->
          <div v-if="fields && settings.spiGraph"
            class="form-group row">
            <label class="col-sm-3 col-form-label text-right font-weight-bold">
              Default SPI Graph
            </label>
            <div class="col-sm-6">
              <moloch-field-typeahead
                :dropup="true"
                :fields="fields"
                query-param="field"
                :initial-value="spiGraphTypeahead"
                @fieldSelected="spiGraphFieldSelected">
              </moloch-field-typeahead>
            </div>
            <div class="col-sm-3">
              <h4 v-if="spiGraphField">
                <label class="badge badge-info cursor-help"
                  v-b-tooltip.hover
                  :title="spiGraphField.help">
                  {{ spiGraphTypeahead || 'unknown field' }}
                </label>
              </h4>
            </div>
          </div> <!-- /default spi graph -->

          <!-- connections src field -->
          <div v-if="fields && settings.connSrcField"
            class="form-group row">
            <label class="col-sm-3 col-form-label text-right font-weight-bold">
              Connections Src
            </label>
            <div class="col-sm-6">
              <moloch-field-typeahead
                :dropup="true"
                :fields="fields"
                query-param="field"
                :initial-value="connSrcFieldTypeahead"
                @fieldSelected="connSrcFieldSelected">
              </moloch-field-typeahead>
            </div>
            <div class="col-sm-3">
              <h4 v-if="connSrcField">
                <label class="badge badge-info cursor-help"
                  v-b-tooltip.hover
                  :title="connSrcField.help">
                  {{ connSrcFieldTypeahead || 'unknown field' }}
                </label>
              </h4>
            </div>
          </div> <!-- /connections src field -->

          <!-- connections dst field -->
          <div v-if="fields && settings.connDstField"
            class="form-group row">
            <label class="col-sm-3 col-form-label text-right font-weight-bold">
              Connections Dst
            </label>
            <div class="col-sm-6">
              <moloch-field-typeahead
                :dropup="true"
                :fields="fieldsPlus"
                query-param="field"
                :initial-value="connDstFieldTypeahead"
                @fieldSelected="connDstFieldSelected">
              </moloch-field-typeahead>
            </div>
            <div class="col-sm-3">
              <h4 v-if="connDstField">
                <label class="badge badge-info cursor-help"
                  v-b-tooltip.hover
                  :title="connDstField.help">
                  {{ connDstFieldTypeahead || 'unknown field' }}
                </label>
              </h4>
            </div>
          </div> <!-- /connections dst field -->

        </form> <!-- / general settings -->

        <!-- view settings -->
        <form v-if="visibleTab === 'views'"
          id="views"
          class="form-horizontal">

          <h3>Views</h3>

          <p>
             Saved views provide an easier method to specify common base queries
             and can be activated in the search bar.
          </p>

          <table class="table table-striped table-sm">
            <thead>
              <tr>
                <th>Share</th>
                <th>Name</th>
                <th>Expression</th>
                <th width="30%">Sessions Columns</th>
                <th>Sessions Sort</th>
                <th>&nbsp;</th>
              </tr>
            </thead>
            <tbody>
              <!-- views -->
              <tr v-for="(item, key) in views"
                @keyup.enter="updateView(key)"
                @keyup.esc="cancelViewChange(key)"
                :key="key">
                <td>
                  <input type="checkbox"
                    v-model="item.shared"
                    @change="toggleShared(item)"
                    class="form-check mt-2"
                    :disabled="!user.createEnabled && item.user && item.user !== user.userId"
                  />
                </td>
                <td>
                  <input type="text"
                    maxlength="20"
                    v-model="item.name"
                    @input="viewChanged(key)"
                    class="form-control form-control-sm"
                    :disabled="!user.createEnabled && item.user && item.user !== user.userId"
                  />
                </td>
                <td>
                  <input type="text"
                    v-model="item.expression"
                    @input="viewChanged(key)"
                    class="form-control form-control-sm"
                    :disabled="!user.createEnabled && item.user && item.user !== user.userId"
                  />
                </td>
                <td>
                  <span v-if="item.sessionsColConfig">
                    <template v-for="col in item.sessionsColConfig.visibleHeaders">
                      <label class="badge badge-secondary mr-1 mb-0 help-cursor"
                        v-if="fieldsMap[col]"
                        v-b-tooltip.hover
                        :title="fieldsMap[col].help"
                        :key="col">
                        {{ fieldsMap[col].friendlyName }}
                      </label>
                    </template>
                  </span>
                </td>
                <td>
                  <span v-if="item.sessionsColConfig">
                    <template v-for="order in item.sessionsColConfig.order">
                      <label class="badge badge-secondary mr-1 help-cursor"
                        :title="fieldsMap[order[0]].help"
                        v-if="fieldsMap[order[0]]"
                        v-b-tooltip.hover
                        :key="order[0]">
                        {{ fieldsMap[order[0]].friendlyName }}&nbsp;
                        ({{ order[1] }})
                      </label>
                    </template>
                  </span>
                </td>
                <td>
                  <div v-if="user.createEnabled || item.user === user.userId || !item.user">
                    <div class="btn-group btn-group-sm pull-right"
                      v-if="item.changed">
                      <button type="button"
                        v-b-tooltip.hover
                        @click="updateView(key)"
                        title="Save changes to this view"
                        class="btn btn-theme-tertiary">
                        <span class="fa fa-save">
                        </span>
                      </button>
                      <button type="button"
                        v-b-tooltip.hover
                        class="btn btn-warning"
                        @click="cancelViewChange(key)"
                        title="Undo changes to this view">
                        <span class="fa fa-ban">
                        </span>
                      </button>
                    </div>
                    <button v-else
                      type="button"
                      class="btn btn-sm btn-danger pull-right"
                      @click="deleteView(item, key)">
                      <span class="fa fa-trash-o">
                      </span>&nbsp;
                      Delete
                    </button>
                  </div>
                </td>
              </tr> <!-- /views -->
              <!-- view list error -->
              <tr v-if="viewListError">
                <td colspan="6">
                  <p class="text-danger mb-0">
                    <span class="fa fa-exclamation-triangle">
                    </span>&nbsp;
                    {{ viewListError }}
                  </p>
                </td>
              </tr> <!-- /view list error -->
              <!-- new view form -->
              <tr @keyup.enter="createView">
                <td>
                  <input type="checkbox"
                    v-model="newViewShared"
                    class="form-check mt-2"
                  />
                </td>
                <td>
                  <input type="text"
                    maxlength="20"
                    v-model="newViewName"
                    class="form-control form-control-sm"
                    placeholder="Enter a new view name (20 chars or less)"
                  />
                </td>
                <td colspan="2">
                  <input type="text"
                    v-model="newViewExpression"
                    class="form-control form-control-sm"
                    placeholder="Enter a new view expression"
                  />
                </td>
                <td>&nbsp;</td>
                <td>
                  <button class="btn btn-theme-tertiary btn-sm pull-right"
                    type="button"
                    @click="createView">
                    <span class="fa fa-plus-circle">
                    </span>&nbsp;
                    Create
                  </button>
                </td>
              </tr> <!-- /new view form -->
              <!-- view form error -->
              <tr v-if="viewFormError">
                <td colspan="6">
                  <p class="text-danger mb-0">
                    <span class="fa fa-exclamation-triangle">
                    </span>&nbsp;
                    {{ viewFormError }}
                  </p>
                </td>
              </tr> <!-- /view form error -->
            </tbody>
          </table>

        </form> <!-- /view settings -->

        <!-- cron query settings -->
        <form v-if="visibleTab === 'cron'"
          class="form-horizontal"
          id="cron">

          <h3>Cron Queries</h3>

          <p>
            Allow queries to be run periodically that can perform actions on
            sessions that match the queries. The query runs against sessions
            delayed by 90 second to make sure all updates have been completed
            for that session.
          </p>

          <table class="table table-striped table-sm">
            <thead>
              <tr>
                <th>Enabled</th>
                <th>Processed</th>
                <th>Name</th>
                <th>Expression</th>
                <th>Action</th>
                <th>Tags</th>
                <th>Notify</th>
                <th>&nbsp;</th>
              </tr>
            </thead>
            <tbody>
              <!-- cron queries -->
              <tr v-for="(item, key) in cronQueries"
                @keyup.enter="updateCronQuery(key)"
                @keyup.esc="cancelCronQueryChange(key)"
                :key="key">
                <td>
                  <input type="checkbox"
                    v-model="item.enabled"
                    @input="cronQueryChanged(key)"
                  />
                </td>
                <td>{{ item.count }}</td>
                <td>
                  <input type="text"
                    maxlength="20"
                    v-model="item.name"
                    class="form-control form-control-sm"
                    @input="cronQueryChanged(key)"
                  />
                </td>
                <td>
                  <input type="text"
                    v-model="item.query"
                    class="form-control form-control-sm"
                    @input="cronQueryChanged(key)"
                  />
                </td>
                <td>
                  <select class="form-control form-control-sm"
                    v-model="item.action"
                    @change="cronQueryChanged(key)">
                    <option value="tag">Tag</option>
                    <option v-for="(item, key) in molochClusters"
                      :value="`forward:${key}`"
                      :key="key">
                      Tag & Export to {{ item.name }}
                    </option>
                  </select>
                </td>
                <td>
                  <input type="text"
                    v-model="item.tags"
                    class="form-control form-control-sm"
                    @input="cronQueryChanged(key)"
                  />
                </td>
                <td>
                  <select v-model="item.notifier"
                    class="form-control form-control-sm"
                    @input="cronQueryChanged(key)">
                    <option value=undefined>none</option>
                    <option v-for="notifier in notifiers"
                      :key="notifier.name"
                      :value="notifier.name">
                      {{ notifier.name }} ({{ notifier.type }})
                    </option>
                  </select>
                </td>
                <td>
                  <div v-if="item.changed"
                    class="btn-group btn-group-sm pull-right">
                    <button type="button"
                      class="btn btn-theme-tertiary"
                      v-b-tooltip.hover
                      title="Save changes to this cron query"
                      @click="updateCronQuery(key)">
                      <span class="fa fa-save">
                      </span>
                    </button>
                    <button type="button"
                      class="btn btn-warning"
                      v-b-tooltip.hover
                      title="Undo changes to this cron query"
                      @click="cancelCronQueryChange(key)">
                      <span class="fa fa-ban">
                      </span>
                    </button>
                  </div>
                  <button type="button"
                    class="btn btn-sm btn-danger pull-right"
                    v-if="!item.changed"
                    @click="deleteCronQuery(key)">
                    <span class="fa fa-trash-o">
                    </span>&nbsp;
                    Delete
                  </button>
                </td>
              </tr> <!-- /cron queries -->
              <!-- cron query form error -->
              <tr v-if="cronQueryListError">
                <td colspan="8">
                  <p class="text-danger mb-0">
                    <span class="fa fa-exclamation-triangle">
                    </span>&nbsp;
                    {{ cronQueryListError }}
                  </p>
                </td>
              </tr> <!-- /cron query form error -->
              <!-- new cron query form -->
              <tr @keyup.enter="createCronQuery">
                <td>&nbsp;</td>
                <td>
                  <select class="form-control form-control-sm"
                    v-model="newCronQueryProcess"
                    v-b-tooltip.hover
                    title="Start processing cron query since">
                    <option value="0">Now</option>
                    <option value="1">1 hour ago</option>
                    <option value="6">6 hours ago</option>
                    <option value="24">24 hours ago</option>
                    <option value="48">48 hours ago</option>
                    <option value="72">72 hours ago</option>
                    <option value="168">1 week ago</option>
                    <option value="336">2 weeks ago</option>
                    <option value="720">1 month ago</option>
                    <option value="1440">2 months ago</option>
                    <option value="4380">6 months ago</option>
                    <option value="8760">1 year ago</option>
                    <option value="-1">All (careful)</option>
                  </select>
                </td>
                <td>
                  <input type="text"
                    v-model="newCronQueryName"
                    class="form-control form-control-sm"
                    maxlength="20"
                    v-b-tooltip.hover
                    title="Enter a new cron query name (20 chars or less)"
                    placeholder="Cron query name"
                  />
                </td>
                <td>
                  <input type="text"
                    v-model="newCronQueryExpression"
                    class="form-control form-control-sm"
                    v-b-tooltip.hover
                    title="Enter a new cron query expression"
                    placeholder="Cron query expression"
                  />
                </td>
                <td>
                  <select class="form-control form-control-sm"
                    v-model="newCronQueryAction">
                    <option value="tag">Tag</option>
                    <option v-for="(item, key) in molochClusters"
                      :key="key"
                      :value="`forward:${key}`">
                      Tag & Export to {{ item.name }}
                    </option>
                  </select>
                </td>
                <td>
                  <input type="text"
                    v-model="newCronQueryTags"
                    class="form-control form-control-sm"
                    v-b-tooltip.hover
                    title="Enter a comma separated list of tags"
                    placeholder="Comma separated list of tags"
                  />
                </td>
                <td>
                  <select v-model="newCronQueryNotifier"
                    class="form-control form-control-sm">
                    <option value=undefined>none</option>
                    <option v-for="notifier in notifiers"
                      :key="notifier.name"
                      :value="notifier.name">
                      {{ notifier.name }} ({{ notifier.type }})
                    </option>
                  </select>
                </td>
                <td>
                  <button type="button"
                    class="btn btn-theme-tertiary btn-sm pull-right"
                    @click="createCronQuery">
                    <span class="fa fa-plus-circle">
                    </span>&nbsp;
                    Create
                  </button>
                </td>
              </tr> <!-- /new cron query form -->
              <!-- cron query form error -->
              <tr v-if="cronQueryFormError">
                <td colspan="8">
                  <p class="small text-danger mb-0">
                    <span class="fa fa-exclamation-triangle">
                    </span>&nbsp;
                    {{ cronQueryFormError }}
                  </p>
                </td>
              </tr> <!-- /cron query form error -->
            </tbody>
          </table>

        </form> <!-- /cron query settings -->

        <!-- col configs settings -->
        <form v-if="visibleTab === 'col'"
          class="form-horizontal"
          id="col">

          <h3>Custom Column Configurations</h3>

          <p>
            Custom column configurations allow the user to save their session
            table's column layout for future use.
          </p>

          <table class="table table-striped table-sm">
            <thead>
              <tr>
                <th>Name</th>
                <th>Columns</th>
                <th>Order</th>
                <th>&nbsp;</th>
              </tr>
            </thead>
            <tbody>
              <!-- default col config -->
              <tr v-if="defaultColConfig && fieldsMap">
                <td>
                  Moloch Default
                </td>
                <td>
                  <template v-for="col in defaultColConfig.columns">
                    <label class="badge badge-secondary mr-1 help-cursor"
                      v-b-tooltip.hover
                      :title="fieldsMap[col].help"
                      v-if="fieldsMap[col]"
                      :key="col">
                      {{ fieldsMap[col].friendlyName }}
                    </label>
                  </template>
                </td>
                <td>
                  <span v-for="order in defaultColConfig.order"
                    :key="order[0]">
                    <label class="badge badge-secondary mr-1 help-cursor"
                      :title="fieldsMap[order[0]].help"
                      v-if="fieldsMap[order[0]]"
                      v-b-tooltip.hover>
                      {{ fieldsMap[order[0]].friendlyName }}&nbsp;
                      ({{ order[1] }})
                    </label>
                  </span>
                </td>
                <td>&nbsp;</td>
              </tr> <!-- /default col configs -->
              <!-- col configs -->
              <template v-if="fieldsMap">
                <tr v-for="(config, index) in colConfigs"
                  :key="config.name">
                  <td>
                    {{ config.name }}
                  </td>
                  <td>
                    <template v-for="col in config.columns">
                      <label class="badge badge-secondary mr-1 help-cursor"
                        :title="fieldsMap[col].help"
                        v-b-tooltip.hover
                        v-if="fieldsMap[col]"
                        :key="col">
                        {{ fieldsMap[col].friendlyName }}
                      </label>
                    </template>
                  </td>
                  <td>
                    <span v-for="order in config.order"
                      :key="order[0]">
                      <label class="badge badge-secondary mr-1 help-cursor"
                        :title="fieldsMap[order[0]].help"
                        v-if="fieldsMap[order[0]]"
                        v-b-tooltip.hover>
                        {{ fieldsMap[order[0]].friendlyName }}&nbsp;
                        ({{ order[1] }})
                      </label>
                    </span>
                  </td>
                  <td>
                    <button type="button"
                      class="btn btn-sm btn-danger pull-right"
                      @click="deleteColConfig(config.name, index)">
                      <span class="fa fa-trash-o">
                      </span>&nbsp;
                      Delete
                    </button>
                  </td>
                </tr>
              </template> <!-- /col configs -->
              <!-- col config list error -->
              <tr v-if="colConfigError">
                <td colspan="3">
                  <p class="text-danger mb-0">
                    <span class="fa fa-exclamation-triangle">
                    </span>&nbsp;
                    {{ colConfigError }}
                  </p>
                </td>
              </tr> <!-- /col config list error -->
            </tbody>
          </table>

          <div v-if="!colConfigs || !colConfigs.length"
            class="alert alert-info">
            <span class="fa fa-info-circle fa-lg">
            </span>
            <strong>
              You have no custom column configurations.
            </strong>
            <br>
            <br>
            To create one, go to the sessions page, rearrange the columns into
            your preferred configuration, and click the column configuration
            button ( <span class="fa fa-columns"></span> ) at the top left of the
            table. Name your new custom column configuration then click the save
            button. You can now switch to this column configuration anytime you
            want by clicking on its name in the dropdown!
          </div>

        </form> <!-- /col configs settings -->

        <!-- spiview field configs settings -->
        <form v-if="visibleTab === 'spiview'"
          class="form-horizontal"
          id="spiview">

          <h3>Custom SPI View Configurations</h3>

          <p>
            Custom visible field configurations allow the user to save their
            visible fields on the SPI View page for future use.
          </p>

          <table class="table table-striped table-sm">
            <thead>
              <tr>
                <th>Name</th>
                <th>Fields</th>
                <th>&nbsp;</th>
              </tr>
            </thead>
            <tbody>
              <!-- default spiview field config -->
              <tr v-if="defaultSpiviewConfig && fieldsMap">
                <td>
                  Moloch Default
                </td>
                <td>
                  <label class="badge badge-secondary mr-1 help-cursor"
                    :title="fieldsMap[field].help"
                    v-b-tooltip.hover
                    v-for="field in defaultSpiviewConfig.fields"
                    :key="field">
                    {{ fieldsMap[field].friendlyName }} (100)
                  </label>
                </td>
                <td>&nbsp;</td>
              </tr> <!-- /default spiview field confg -->
              <!-- spiview field configs -->
              <template v-if="fieldsMap">
                <tr v-for="(config, index) in spiviewConfigs"
                  :key="config.name">
                  <td>
                    {{ config.name }}
                  </td>
                  <td>
                    <label class="badge badge-secondary mr-1 help-cursor"
                      :title="fieldObj.help"
                      v-b-tooltip.hover
                      v-for="fieldObj in config.fieldObjs"
                      :key="fieldObj.dbField">
                      {{fieldObj.friendlyName}}
                      ({{fieldObj.count}})
                    </label>
                  </td>
                  <td>
                    <button type="button"
                      class="btn btn-sm btn-danger pull-right"
                      @click="deleteSpiviewConfig(config.name, index)">
                      <span class="fa fa-trash-o">
                      </span>&nbsp;
                      Delete
                    </button>
                  </td>
                </tr>
              </template> <!-- /spiview field configs -->
              <!-- spiview field config list error -->
              <tr v-if="spiviewConfigError">
                <td colspan="3">
                  <p class="text-danger mb-0">
                    <span class="fa fa-exclamation-triangle">
                    </span>&nbsp;
                    {{ spiviewConfigError }}
                  </p>
                </td>
              </tr> <!-- /spview field config list error -->
            </tbody>
          </table>

          <div v-if="!spiviewConfigs || !spiviewConfigs.length"
            class="alert alert-info">
            <span class="fa fa-info-circle fa-lg">
            </span>
            <strong>
              You have no custom SPI View field configurations.
            </strong>
            <br>
            <br>
            To create one, go to the SPI View page, toggle fields to
            your preferred configuration, and click the field configuration
            button ( <span class="fa fa-columns"></span> ) at the top left of the
            page. Name your new custom field configuration then click the save
            button. You can now switch to this field configuration anytime you
            want by clicking on its name in the dropdown!
          </div>

        </form> <!-- /spiview field configs settings -->

        <!-- theme settings -->
        <form v-if="visibleTab === 'theme'"
          id="theme">

          <h3>UI Themes</h3>

          <p>
            Pick from a preexisting theme below
          </p>

          <hr>

          <!-- theme picker -->
          <div class="row">
            <div class="col-lg-6 col-md-12"
              v-for="theme in themeDisplays"
              :class="theme.class"
              :key="theme.class">
              <div class="theme-display">
                <nav class="navbar navbar-dark">
                  <a class="navbar-brand cursor-pointer">
                    <img src="../../assets/logo.png"
                      class="moloch-logo"
                      alt="hoot"
                    />
                  </a>
                  <ul class="nav">
                    <a class="nav-item cursor-pointer active">
                      Current Page
                    </a>
                    <a class="nav-item cursor-pointer ml-3">
                      Other Pages
                    </a>
                  </ul>
                  <ul class="navbar-nav">
                    <span class="fa fa-info-circle fa-lg health-green">
                    </span>
                  </ul>
                </nav>
                <div class="display-sub-navbar">
                  <div class="row">
                    <div class="col-xl-5 col-lg-4 col-md-5">
                      <div class="input-group input-group-sm ml-1">
                        <span class="input-group-prepend">
                          <span class="input-group-text">
                            <span class="fa fa-search">
                            </span>
                          </span>
                        </span>
                        <input type="text"
                          placeholder="Search"
                          class="form-control"
                        />
                      </div>
                    </div>
                    <div class="col-xl-7 col-lg-8 col-sm-7">
                      <div class="font-weight-bold text-theme-accent">
                        Important text
                      </div>
                      <div class="pull-right display-sub-navbar-buttons">
                        <a class="btn btn-sm btn-default btn-theme-tertiary-display">
                          Search
                        </a>
                        <a class="btn btn-sm btn-default btn-theme-quaternary-display">
                          <span class="fa fa-cog fa-lg">
                          </span>
                        </a>
                        <a class="btn btn-sm btn-default btn-theme-secondary-display">
                          <span class="fa fa-eye fa-lg">
                          </span>
                        </a>
                        <b-dropdown right
                          size="sm"
                          class="pull-right ml-1 action-menu-dropdown"
                          variant="theme-primary-display">
                          <b-dropdown-item>
                            Example
                          </b-dropdown-item>
                          <b-dropdown-item class="active">
                            Active Example
                          </b-dropdown-item>
                        </b-dropdown>
                      </div>
                    </div>
                  </div>
                </div>
                <div class="display-sub-sub-navbar">
                  <div class="ml-1 mt-2 pb-2">
                    <span class="field cursor-pointer">
                      example field value
                      <span class="fa fa-caret-down">
                      </span>
                    </span>
                  </div>
                </div>
                <div class="row">
                  <div class="col-md-12">
                    <div class="custom-control custom-radio ml-1">
                      <input type="radio"
                        class="custom-control-input cursor-pointer"
                        v-model="settings.theme"
                        @change="changeTheme(theme.class)"
                        :value="theme.class"
                        :id="theme.class"
                      />
                      <label class="custom-control-label cursor-pointer"
                        :for="theme.class">
                        {{ theme.name }}
                      </label>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </div> <!-- /theme picker -->

          <hr>

          <!-- custom theme -->
          <p v-if="!creatingCustom">
            Want to more control
            <small>(to make the UI completely unusable)</small>?
            <a href="javascript:void(0)"
              class="cursor-pointer"
              @click="creatingCustom = true">
              Create your own custom theme
            </a>
            <br><br>
          </p>

          <div v-if="creatingCustom">

            <!-- custom theme display -->
            <div class="row">
              <div class="col-md-4">
                <h3 class="mt-0 mb-3">
                  Custom Theme
                  <button type="button"
                    class="btn btn-theme-tertiary pull-right"
                    title="Toggle color theme help"
                    v-b-tooltip.hover
                    @click="displayHelp = !displayHelp">
                    <span class="fa fa-question-circle">
                    </span>&nbsp;
                    <span v-if="displayHelp">
                      Hide
                    </span>
                    <span v-else>
                      Show
                    </span>
                    Help
                  </button>
                </h3>
                <color-picker :color="background"
                  @colorSelected="changeColor"
                  colorName="background"
                  fieldName="Background"
                  :class="{'mb-2':!displayHelp}">
                </color-picker>
                <p class="help-block small"
                  v-if="displayHelp">
                  This color should either be very light or very dark.
                </p>
                <color-picker :color="foreground"
                  @colorSelected="changeColor"
                  colorName="foreground"
                  fieldName="Foreground"
                  :class="{'mb-2':!displayHelp}">
                </color-picker>
                <p class="help-block small"
                  v-if="displayHelp">
                  This color should be visible on the background.
                </p>
                <color-picker :color="foregroundAccent"
                  @colorSelected="changeColor"
                  colorName="foregroundAccent"
                  fieldName="Foreground Accent">
                </color-picker>
                <p class="help-block small"
                  v-if="displayHelp">
                  This color should stand out.
                  It displays session field values and important text in navbars.
                </p>
              </div>
              <div class="col-md-8">

                <div class="custom-theme"
                  id="custom-theme-display">
                  <div class="theme-display">
                    <div class="navbar navbar-dark">
                      <a class="navbar-brand cursor-pointer">
                        <img src="../../assets/logo.png"
                          class="moloch-logo"
                          alt="hoot"
                        />
                      </a>
                      <ul class="nav">
                        <a class="nav-item cursor-pointer active">
                          Current Page
                        </a>
                        <a class="nav-item cursor-pointer ml-3">
                          Other Pages
                        </a>
                      </ul>
                      <ul class="navbar-nav">
                        <span class="fa fa-info-circle fa-lg health-green">
                        </span>
                      </ul>
                    </div>
                    <div class="display-sub-navbar">
                      <div class="row">
                        <div class="col-xl-5 col-lg-4 col-md-5">
                          <div class="input-group input-group-sm ml-1">
                            <span class="input-group-prepend">
                              <span class="input-group-text">
                                <span class="fa fa-search">
                                </span>
                              </span>
                            </span>
                            <input type="text"
                              placeholder="Search"
                              class="form-control"
                            />
                          </div>
                        </div>
                        <div class="col-xl-7 col-lg-8 col-sm-7">
                          <div class="font-weight-bold text-theme-accent">
                            Important text
                          </div>
                          <div class="pull-right display-sub-navbar-buttons">
                            <a class="btn btn-sm btn-default btn-theme-tertiary-display">
                              Search
                            </a>
                            <a class="btn btn-sm btn-default btn-theme-quaternary-display">
                              <span class="fa fa-cog fa-lg">
                              </span>
                            </a>
                            <a class="btn btn-sm btn-default btn-theme-secondary-display">
                              <span class="fa fa-eye fa-lg">
                              </span>
                            </a>
                            <b-dropdown right
                              size="sm"
                              class="pull-right ml-1 action-menu-dropdown"
                              variant="theme-primary-display">
                              <b-dropdown-item>
                                Example
                              </b-dropdown-item>
                              <b-dropdown-item class="active">
                                Active Example
                              </b-dropdown-item>
                            </b-dropdown>
                          </div>
                        </div>
                      </div>
                    </div>
                    <div class="display-sub-sub-navbar">
                      <moloch-paging
                        class="mt-1 ml-1"
                        :records-total="200"
                        :records-filtered="100">
                      </moloch-paging>
                    </div>
                    <div>
                      <div class="ml-1 mr-1 mt-2 pb-2">
                        <span class="field cursor-pointer">
                          example field value
                          <span class="fa fa-caret-down">
                          </span>
                        </span>
                        <br><br>
                        <div class="row">
                          <div class="col-md-6 sessionsrc">
                            <small class="session-detail-ts font-weight-bold">
                              <em class="ts-value">
                                2013/11/18 03:06:52.831
                              </em>
                              <span class="pull-right">
                                27 bytes
                              </span>
                            </small>
                            <pre>Source packet text</pre>
                          </div>
                          <div class="col-md-6 sessiondst">
                            <small class="session-detail-ts font-weight-bold">
                              <em class="ts-value">
                                2013/11/18 03:06:52.841
                              </em>
                              <span class="pull-right">
                                160 bytes
                              </span>
                            </small>
                            <pre>Destination packet text</pre>
                          </div>
                        </div>
                      </div>
                    </div>
                  </div>
                </div>

              </div>
            </div> <!-- /custom theme display -->

            <br>

            <p v-if="displayHelp"
              class="help-block">
              Main theme colors are lightened/darkened programmatically to
              provide dark boarders, active buttons, hover colors, etc.
            </p>

            <!-- main colors -->
            <div class="row form-group">
              <div class="col-md-3">
                <color-picker :color="primary"
                  @colorSelected="changeColor"
                  colorName="primary"
                  fieldName="Primary">
                </color-picker>
                <p v-if="displayHelp"
                  class="help-block small">
                  Primary navbar, buttons, active item(s) in lists
                </p>
              </div>
              <div class="col-md-3">
                <color-picker :color="secondary"
                  @colorSelected="changeColor"
                  colorName="secondary"
                  fieldName="Secondary">
                </color-picker>
                <p v-if="displayHelp"
                  class="help-block small">
                  Buttons
                </p>
              </div>
              <div class="col-md-3">
                <color-picker :color="tertiary"
                  @colorSelected="changeColor"
                  colorName="tertiary"
                  fieldName="Tertiary">
                </color-picker>
                <p v-if="displayHelp"
                  class="help-block small">
                  Action buttons (search, apply, open, etc)
                </p>
              </div>
              <div class="col-md-3">
                <color-picker :color="quaternary"
                  @colorSelected="changeColor"
                  colorName="quaternary"
                  fieldName="Quaternary">
                </color-picker>
                <p v-if="displayHelp"
                  class="help-block small">
                  Accent and all other buttons
                </p>
              </div>
            </div> <!-- /main colors -->

            <p v-if="displayHelp"
              class="help-block">
              <em>Highlight colors should be similar to their parent color, above.</em>
              <br>
              For <strong>light themes</strong>, the highlight color should be <strong>lighter</strong> than the original.
              For <strong>dark themes</strong>, the highlight color should be <strong>darker</strong> than original.
            </p>

            <!-- main color highlights/backgrounds -->
            <div class="row form-group">
              <div class="col-md-3">
                <color-picker :color="primaryLightest"
                  @colorSelected="changeColor"
                  colorName="primaryLightest"
                  fieldName="Highlight 1">
                </color-picker>
                <p v-if="displayHelp"
                  class="help-block small">
                  Backgrounds
                </p>
              </div>
              <div class="col-md-3">
                <color-picker :color="secondaryLightest"
                  @colorSelected="changeColor"
                  colorName="secondaryLightest"
                  fieldName="Highlight 2">
                </color-picker>
                <p v-if="displayHelp"
                  class="help-block small">
                  Search/Secondary navbar
                </p>
              </div>
              <div class="col-md-3">
                <color-picker :color="tertiaryLightest"
                  @colorSelected="changeColor"
                  colorName="tertiaryLightest"
                  fieldName="Highlight 3">
                </color-picker>
                <p v-if="displayHelp"
                  class="help-block small">
                  Tertiary navbar, table hover
                </p>
              </div>
              <div class="col-md-3">
                <color-picker :color="quaternaryLightest"
                  @colorSelected="changeColor"
                  colorName="quaternaryLightest"
                  fieldName="Highlight 4">
                </color-picker>
                <p v-if="displayHelp"
                  class="help-block small">
                  Session detail background
                </p>
              </div>
            </div> <!-- /main color highlights/backgrounds -->

            <br>

            <div class="row form-group">
              <!-- visualization colors -->
              <div class="col-md-3">
                <color-picker :color="water"
                  @colorSelected="changeColor"
                  colorName="water"
                  fieldName="Map Water">
                </color-picker>
              </div>
              <div class="col-md-3">
                <color-picker :color="land"
                  @colorSelected="changeColor"
                  colorName="land"
                  fieldName="Map Land">
                </color-picker>
              </div> <!-- /visualization colors -->
              <!-- packet colors -->
              <div class="col-md-3">
                <color-picker :color="src"
                  @colorSelected="changeColor"
                  colorName="src"
                  fieldName="Source Packets">
                </color-picker>
              </div>
              <div class="col-md-3">
                <color-picker :color="dst"
                  @colorSelected="changeColor"
                  colorName="dst"
                  fieldName="Destination Packets">
                </color-picker>
              </div> <!-- /packet colors -->
            </div>

            <br>

            <div class="row mb-4">
              <div class="col-md-12">
                <label>
                  Share your theme with others:
                </label>
                <div class="input-group input-group-sm">
                  <input type="text"
                    class="form-control"
                    v-model="themeString"
                  />
                  <span class="input-group-append">
                    <button class="btn btn-theme-secondary"
                      type="button"
                      v-clipboard:copy="themeString">
                      <span class="fa fa-clipboard">
                      </span>&nbsp;
                      Copy
                    </button>
                    <button class="btn btn-theme-primary"
                      type="button"
                      @click="updateThemeString">
                      <span class="fa fa-check">
                      </span>&nbsp;
                      Apply
                    </button>
                  </span>
                </div>
              </div>
            </div>

          </div> <!-- /custom theme -->

        </form> <!-- /theme settings -->

        <!-- password settings -->
        <form v-if="visibleTab === 'password' && !multiviewer"
          class="form-horizontal"
          @keyup.enter="changePassword"
          id="password">

          <h3>Change Password</h3>

          <hr>

          <!-- current password -->
          <div v-if="!userId"
            class="form-group row">
            <label class="col-sm-3 col-form-label text-right font-weight-bold">
              Current Password
            </label>
            <div class="col-sm-6">
              <input type="password"
                class="form-control form-control-sm"
                v-model="currentPassword"
                placeholder="Enter your current password"
              />
            </div>
          </div>

          <!-- new password -->
          <div class="form-group row">
            <label class="col-sm-3 col-form-label text-right font-weight-bold">
              New Password
            </label>
            <div class="col-sm-6">
              <input type="password"
                class="form-control form-control-sm"
                v-model="newPassword"
                placeholder="Enter a new password"
              />
            </div>
          </div>

          <!-- confirm new password -->
          <div class="form-group row">
            <label class="col-sm-3 col-form-label text-right font-weight-bold">
              New Password
            </label>
            <div class="col-sm-6">
              <input type="password"
                class="form-control form-control-sm"
                v-model="confirmNewPassword"
                placeholder="Confirm your new password"
              />
            </div>
          </div>

          <!-- change password button/error -->
          <div class="form-group row">
            <label class="col-sm-3 col-form-label">&nbsp;</label>
            <div class="col-sm-9">
              <button type="button"
                class="btn btn-theme-tertiary"
                @click="changePassword">
                Change Password
              </button>
              <span v-if="changePasswordError"
                class="small text-danger pl-4">
                <span class="fa fa-exclamation-triangle">
                </span>&nbsp;
                {{ changePasswordError }}
              </span>
            </div>
          </div> <!-- /change password button/error -->

        </form> <!-- /password settings -->

        <!-- notifiers settings -->
        <form class="form-horizontal"
          v-if="visibleTab === 'notifiers'"
          v-has-permission="'createEnabled'"
          id="notifiers">

          <h3>
            Notifiers
            <template v-if="notifierTypes">
              <button v-for="notifier of notifierTypes"
                :key="notifier.name"
                class="btn btn-theme-tertiary btn-sm pull-right ml-1"
                type="button"
                @click="createNewNotifier(notifier)">
                <span class="fa fa-plus-circle">
                </span>&nbsp;
                Create {{ notifier.name }} Notifier
              </button>
            </template>
          </h3>

          <p>
            Configure notifiers that can be added to cron queries.
          </p>

          <hr>

          <div v-if="!notifiers || !Object.keys(notifiers).length"
            class="alert alert-info">
            <span class="fa fa-info-circle fa-lg">
            </span>
            <strong>
              You have no notifiers configured.
            </strong>
            <br>
            <br>
            Create one by clicking the create button above
            and add it to your cron queries on the cron tab!
          </div>

          <!-- new notifier -->
          <div class="row"
            v-if="newNotifier">
            <div class="col">
              <div class="card mb-3">
                <div class="card-body">
                  <!-- newNotifier title -->
                  <h4 class="mb-3">
                    Create new {{ newNotifier.type }} notifier
                    <span v-if="newNotifierError"
                      class="alert alert-sm alert-danger pull-right pr-2">
                      {{ newNotifierError }}
                      <span class="fa fa-close cursor-pointer"
                        @click="newNotifierError = ''">
                      </span>
                    </span>
                  </h4> <!-- /new notifier title -->
                  <!-- new notifier name -->
                  <div class="input-group mb-2">
                    <span class="input-group-prepend cursor-help"
                      :title="`Give your ${newNotifier.type} notifier a unique name`"
                      v-b-tooltip.hover.bottom-left>
                      <span class="input-group-text">
                        Name
                        <sup>*</sup>
                      </span>
                    </span>
                    <input class="form-control"
                      v-model="newNotifier.name"
                      type="text"
                    />
                  </div> <!-- /new notifier name -->
                  <!-- new notifier fields -->
                  <div v-for="field of newNotifier.fields"
                    :key="field.name">
                    <span class="mb-2"
                      :class="{'input-group':field.type !== 'checkbox'}">
                      <span class="input-group-prepend cursor-help"
                        v-if="field.type !== 'checkbox'"
                        :title="field.description"
                        v-b-tooltip.hover.bottom-left>
                        <span class="input-group-text">
                          {{ field.name }}
                          <sup v-if="field.required">*</sup>
                        </span>
                      </span>
                      <input :class="{'form-control':field.type !== 'checkbox'}"
                        v-model="field.value"
                        :type="getFieldInputType(field)"
                      />
                      <span v-if="field.type === 'secret'"
                        class="input-group-append cursor-pointer"
                        @click="toggleVisibleSecretField(field)">
                        <span class="input-group-text">
                          <span class="fa"
                            :class="{'fa-eye':field.type === 'secret' && !field.showValue, 'fa-eye-slash':field.type === 'secret' && field.showValue}">
                          </span>
                        </span>
                      </span>
                    </span>
                    <label v-if="field.type === 'checkbox'">
                      &nbsp;{{ field.name }}
                    </label>
                  </div> <!-- /new notifier fields -->
                  <!-- new notifier actions -->
                  <div class="row mt-3">
                    <div class="col-12">
                      <button type="button"
                        class="btn btn-sm btn-outline-warning cursor-pointer"
                        @click="clearNotifierFields">
                        Clear fields
                      </button>
                      <button type="button"
                        class="btn btn-sm btn-success cursor-pointer pull-right ml-1"
                        @click="createNotifier">
                        <span class="fa fa-plus">
                        </span>&nbsp;
                        Create {{ newNotifier.type }} Notifier
                      </button>
                      <button type="button"
                        class="btn btn-sm btn-warning cursor-pointer pull-right"
                        @click="newNotifier = undefined">
                        <span class="fa fa-ban">
                        </span>&nbsp;
                        Cancel
                      </button>
                    </div>
                  </div> <!-- /new notifier actions -->
                </div>
              </div>
            </div>
          </div> <!-- new notifier -->

          <!-- notifiers -->
          <div class="row"
            v-if="notifiers">
            <div class="col-12 col-xl-6"
              v-for="(notifier, key) of notifiers"
              :key="notifier.name">
              <div class="card mb-3">
                <div class="card-body">
                  <!-- notifier title -->
                  <h4 class="mb-3">
                    {{ notifier.type }} Notifier
                  </h4> <!-- /notifier title -->
                  <!-- notifier name -->
                  <div class="input-group mb-2">
                    <span class="input-group-prepend cursor-help"
                      :title="`Give your notifier a unique name`"
                      v-b-tooltip.hover.bottom-left>
                      <span class="input-group-text">
                        Name
                        <sup>*</sup>
                      </span>
                    </span>
                    <input class="form-control"
                      v-model="notifier.name"
                      type="text"
                    />
                  </div> <!-- /notifier name -->
                  <!-- notifier fields -->
                  <div v-for="field of notifier.fields"
                    :key="field.name">
                    <span class="mb-2"
                      :class="{'input-group':field.type !== 'checkbox'}">
                      <span class="input-group-prepend cursor-help"
                        v-if="field.type !== 'checkbox'"
                        :title="field.description"
                        v-b-tooltip.hover.bottom-left>
                        <span class="input-group-text">
                          {{ field.name }}
                          <sup v-if="field.required">*</sup>
                        </span>
                      </span>
                      <input :class="{'form-control':field.type !== 'checkbox'}"
                        v-model="field.value"
                        :type="getFieldInputType(field)"
                      />
                      <span v-if="field.type === 'secret'"
                        class="input-group-append cursor-pointer"
                        @click="toggleVisibleSecretField(field)">
                        <span class="input-group-text">
                          <span class="fa"
                            :class="{'fa-eye':field.type === 'secret' && !field.showValue, 'fa-eye-slash':field.type === 'secret' && field.showValue}">
                          </span>
                        </span>
                      </span>
                    </span>
                    <label v-if="field.type === 'checkbox'">
                      &nbsp;{{ field.name }}
                    </label>
                  </div> <!-- /notifier fields -->
                  <!-- notifier actions -->
                  <div class="row mt-3">
                    <div class="col-12">
                      <button type="button"
                        class="btn btn-sm btn-outline-warning cursor-pointer"
                        @click="testNotifier(notifier.name)">
                        <span class="fa fa-bell">
                        </span>&nbsp;
                        Test
                      </button>
                      <button type="button"
                        class="btn btn-sm btn-success cursor-pointer pull-right ml-1"
                        @click="updateNotifier(key, notifier)">
                        <span class="fa fa-save">
                        </span>&nbsp;
                        Save
                      </button>
                      <button type="button"
                        class="btn btn-sm btn-danger cursor-pointer pull-right"
                        @click="removeNotifier(notifier.name)">
                        <span class="fa fa-trash-o">
                        </span>&nbsp;
                        Delete
                      </button>
                    </div>
                  </div> <!-- /notifier actions -->
                </div>
              </div>
            </div>
          </div> <!-- notifiers -->

        </form>
        <!-- /notifiers settings -->

        <!-- shortcut settings -->
        <form class="form-horizontal"
          v-if="visibleTab === 'shortcuts'"
          id="shortcuts">

          <h3>Shortcuts</h3>

          <p>
            Create a list of values that can be used in queries as shortcuts.
            For example, create a list of IPs and use them in a query
            expression <code>ip.src == $MY_IPS</code>.
            <br>
            <strong>Tip:</strong>
            Use <code>$</code> to autocomplete shortcuts in search expressions.
          </p>

          <table v-if="shortcuts && shortcuts.length"
            class="table table-striped table-sm">
            <thead>
              <tr>
                <th>Shared</th>
                <th>Name</th>
                <th>Description</th>
                <th>Value(s)</th>
                <th>Type</th>
                <th>&nbsp;</th>
              </tr>
            </thead>
            <tbody>
              <!-- shortcuts -->
              <template v-for="(item, index) in shortcuts">
                <tr :key="`${item.id}-content`">
                  <td>
                    <input type="checkbox"
                      :disabled="!user.createEnabled && item.userId !== user.userId"
                      v-model="item.shared"
                      @input="toggleShortcutShared(item)"
                    />
                  </td>
                  <td>
                    {{ item.name }}
                  </td>
                  <td>
                    {{ item.description }}
                  </td>
                  <td class="shortcut-value">
                    {{ item.value }}
                  </td>
                  <td>
                    {{ item.type }}
                  </td>
                  <td class="shortcut-btns">
                    <span v-if="user.createEnabled || item.userId === user.userId">
                      <span v-if="!item.newValue">
                        <button type="button"
                          v-b-tooltip.hover
                          @click="toggleEditShortcut(item)"
                          title="Make changes to this shortcut's value"
                          class="btn btn-sm btn-theme-tertiary pull-right ml-1">
                          <span class="fa fa-pencil">
                          </span>
                        </button>
                        <button type="button"
                          v-b-tooltip.hover
                          title="Delete this shortcut"
                          class="btn btn-sm btn-danger pull-right"
                          @click="deleteShortcut(item, index)">
                          <span class="fa fa-trash-o">
                          </span>
                        </button>
                      </span>
                      <span v-else>
                        <button type="button"
                          v-b-tooltip.hover
                          @click="updateShortcut(item)"
                          title="Save changes to this shortcut's value"
                          class="btn btn-sm btn-theme-tertiary pull-right ml-1">
                          <span class="fa fa-save">
                          </span>
                        </button>
                        <button type="button"
                          v-b-tooltip.hover
                          title="Cancel changes to this shortcut's value"
                          class="btn btn-sm btn-warning pull-right"
                          @click="toggleEditShortcut(item)">
                          <span class="fa fa-ban">
                          </span>
                        </button>
                      </span>
                    </span>
                  </td>
                </tr>
                <tr :key="`${item.id}-edit`"
                  v-if="item.newValue">
                  <td colspan="6">
                    <textarea rows="5"
                      type="text"
                      class="form-control form-control-sm m-1"
                      v-model="item.newValue">
                    </textarea>
                  </td>
                </tr>
              </template> <!-- /shortcuts -->
              <!-- shortcuts list error -->
              <tr v-if="shortcutsListError">
                <td colspan="6">
                  <p class="text-danger mb-0">
                    <span class="fa fa-exclamation-triangle">
                    </span>&nbsp;
                    {{ shortcutsListError }}
                  </p>
                </td>
              </tr> <!-- /shortcuts list error -->
            </tbody>
          </table>
          <!-- new shortcut form -->
          <div class="row var-form mr-1 ml-1 mt-2">
            <div class="col">
              <div class="row mb-3 mt-4">
                <div class="col-10 offset-2">
                  <h3 class="mt-3">
                    New Shortcut
                  </h3>
                </div>
              </div>
              <div class="form-group row">
                <label for="newShortcutName"
                  class="col-2 col-form-label text-right">
                  Name<sup>*</sup>
                </label>
                <div class="col-10">
                  <input id="newShortcutName"
                    type="text"
                    class="form-control form-control-sm"
                    v-model="newShortcutName"
                    placeholder="MY_MOLOCH_VAR"
                  />
                </div>
              </div>
              <div class="form-group row">
                <label for="newShortcutDescription"
                  class="col-2 col-form-label text-right">
                  Description
                </label>
                <div class="col-10">
                  <input id="newShortcutDescription"
                    type="text"
                    class="form-control form-control-sm"
                    v-model="newShortcutDescription"
                  />
                </div>
              </div>
              <div class="form-group row">
                <label for="newShortcutValue"
                  class="col-2 col-form-label text-right">
                  Value(s)<sup>*</sup>
                </label>
                <div class="col-10">
                  <textarea id="newShortcutValue"
                    type="text"
                    rows="5"
                    class="form-control form-control-sm"
                    v-model="newShortcutValue"
                    placeholder="Enter a comma or newline separated list of values">
                  </textarea>
                </div>
              </div>
              <div class="form-group row">
                <label for="newShortcutType"
                  class="col-2 col-form-label text-right">
                  Type<sup>*</sup>
                </label>
                <div class="col-10">
                  <select id="newShortcutType"
                    v-model="newShortcutType"
                    class="form-control form-control-sm">
                    <option value="ip">IP(s)</option>
                    <option value="string">String(s)</option>
                    <option value="number">Number(s)</option>
                  </select>
                </div>
              </div>
              <div class="form-group row">
                <label for="newShortcutShared"
                  class="col-2 col-form-label text-right">
                  Shared
                </label>
                <div class="col-10">
                  <input id="newShortcutShared"
                    type="checkbox"
                    v-model="newShortcutShared"
                  />
                  <button class="btn btn-theme-tertiary btn-sm pull-right"
                    type="button"
                    @click="createShortcut">
                    <span class="fa fa-plus-circle">
                    </span>&nbsp;
                    Create
                  </button>
                </div>
              </div>
              <!-- shortcut form error -->
              <div class="row mb-4 text-right">
                <div class="col-12">
                  <p v-if="shortcutFormError"
                    class="small text-danger mb-0">
                    <span class="fa fa-exclamation-triangle">
                    </span>&nbsp;
                    {{ shortcutFormError }}
                  </p>
                </div>
              </div> <!-- /shortcut form error -->
            </div>
          </div> <!-- /new shortcut form -->

        </form> <!-- / shortcut settings -->

      </div>

    </div> <!-- /content -->

  </div> <!-- /settings content -->

</template>

<script>
import UserService from '../users/UserService';
import ConfigService from '../utils/ConfigService';
import FieldService from '../search/FieldService';
import customCols from '../sessions/customCols.json';
import MolochToast from '../utils/Toast';
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';
import MolochFieldTypeahead from '../utils/FieldTypeahead';
import ColorPicker from '../utils/ColorPicker';
import MolochPaging from '../utils/Pagination';

let clockInterval;

const defaultSpiviewConfig = { fields: ['dstIp', 'protocol', 'srcIp'] };
const defaultColConfig = {
  order: [['firstPacket', 'desc']],
  columns: ['firstPacket', 'lastPacket', 'src', 'srcPort', 'dst', 'dstPort', 'totPackets', 'dbby', 'node', 'info']
};

export default {
  name: 'Settings',
  components: {
    MolochError,
    MolochLoading,
    MolochToast,
    MolochFieldTypeahead,
    ColorPicker,
    MolochPaging
  },
  data: function () {
    return {
      // page vars
      userId: undefined,
      error: '',
      loading: true,
      msg: '',
      msgType: undefined,
      displayName: undefined,
      visibleTab: 'general', // default tab
      settings: {},
      fields: undefined,
      fieldsMap: undefined,
      fieldsPlus: undefined,
      columns: [],
      // general settings vars
      date: undefined,
      spiGraphField: undefined,
      spiGraphTypeahead: undefined,
      connSrcField: undefined,
      connSrcFieldTypeahead: undefined,
      connDstField: undefined,
      connDstFieldTypeahead: undefined,
      // view settings vars
      viewListError: '',
      viewFormError: '',
      newViewName: '',
      newViewExpression: '',
      newViewShared: false,
      // cron settings vars
      cronQueries: undefined,
      cronQueryListError: '',
      cronQueryFormError: '',
      newCronQueryName: '',
      newCronQueryExpression: '',
      newCronQueryTags: '',
      newCronQueryNotifier: undefined,
      newCronQueryProcess: '0',
      newCronQueryAction: 'tag',
      molochClusters: {},
      // column config settings vars
      colConfigs: undefined,
      colConfigError: '',
      defaultColConfig: defaultColConfig,
      // spiview field config settings vars
      spiviewConfigs: undefined,
      spiviewConfigError: '',
      defaultSpiviewConfig: defaultSpiviewConfig,
      // theme settings vars
      themeDisplays: [
        { name: 'Purp-purp', class: 'default-theme' },
        { name: 'Blue', class: 'blue-theme' },
        { name: 'Green', class: 'green-theme' },
        { name: 'Cotton Candy', class: 'cotton-candy-theme' },
        { name: 'Green on Black', class: 'dark-2-theme' },
        { name: 'Dark Blue', class: 'dark-3-theme' }
      ],
      creatingCustom: false,
      displayHelp: true,
      // password settings vars
      currentPassword: '',
      newPassword: '',
      confirmNewPassword: '',
      changePasswordError: '',
      multiviewer: this.$constants.MOLOCH_MULTIVIEWER,
      // notifiers settings vars
      notifiers: undefined,
      notifierTypes: [],
      notifiersError: '',
      newNotifier: undefined,
      newNotifierError: '',
      // shortcut settings vars
      shortcuts: undefined,
      shortcutsListError: '',
      newShortcutShared: false,
      newShortcutName: '',
      newShortcutDescription: '',
      newShortcutValue: '',
      newShortcutType: 'string',
      shortcutFormError: ''
    };
  },
  computed: {
    user: function () {
      return this.$store.state.user;
    },
    views: {
      get: function () {
        return this.$store.state.views;
      },
      set: function (newValue) {
        this.$store.commit('setViews', newValue);
      }
    },
    sortableColumns: function () {
      return this.columns.filter(column => !column.unsortable);
    }
  },
  created: function () {
    // does the url specify a tab in hash
    let tab = window.location.hash;
    if (tab) { // if there is a tab specified and it's a valid tab
      tab = tab.replace(/^#/, '');
      if (tab === 'general' || tab === 'views' || tab === 'cron' ||
        tab === 'col' || tab === 'theme' || tab === 'password' ||
        tab === 'spiview' || tab === 'notifiers' || tab === 'shortcuts') {
        this.visibleTab = tab;
      }

      if (tab === 'password' && this.multiviewer) {
        // multiviewer user can't change password
        this.openView('general');
      }
    }

    this.getThemeColors();

    UserService.getCurrent()
      .then((response) => {
        this.displayName = response.userId;
        // only admins can edit other users' settings
        if (response.createEnabled && this.$route.query.userId) {
          if (response.userId === this.$route.query.userId) {
            // admin editing their own user so the routeParam is unnecessary
            this.$router.push({
              hash: this.$route.hash,
              query: {
                ...this.$route.query,
                userId: undefined
              }
            });
          } else { // admin editing another user
            this.userId = this.$route.query.userId;
            this.displayName = this.$route.query.userId;
          }
        } else { // normal user has no permission, so remove the routeParam
          // (even if it's their own userId because it's unnecessary)
          this.$router.push({
            hash: this.$route.hash,
            query: {
              ...this.$route.query,
              userId: undefined
            }
          });
        }

        // always get the user's settings because current user is cached
        // so response.settings might be stale
        this.getSettings();

        // get all the other things!
        this.getViews();
        this.getCronQueries();
        this.getColConfigs();
        this.getSpiviewConfigs();
        this.getNotifierTypes();
        this.getNotifiers();
        this.getShortcuts();
      })
      .catch((error) => {
        this.error = error.text;
        this.loading = false;
      });

    ConfigService.getMolochClusters()
      .then((response) => {
        this.molochClusters = response;
      });
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    /* opens a specific settings tab */
    openView: function (tabName) {
      this.visibleTab = tabName;
      this.$router.push({
        hash: tabName
      });
    },
    /* remove the message when user is done with it or duration ends */
    messageDone: function () {
      this.msg = '';
      this.msgType = undefined;
    },
    /* GENERAL ------------------------------- */
    /**
     * saves the user's settings and displays a message
     * @param updateTheme whether to update the UI theme
     */
    update: function (updateTheme) {
      UserService.saveSettings(this.settings, this.userId)
        .then((response) => {
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';

          if (updateTheme) {
            let now = Date.now();
            if ($('link[href^="user.css"]').length) {
              $('link[href^="user.css"]').remove();
            }
            $('head').append(`<link rel="stylesheet"
                              href="user.css?v${now}"
                              type="text/css" />`);
          }
        })
        .catch((error) => {
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    },
    /* updates the displayed date for the timzeone setting
     * triggered by the user changing the timezone setting */
    updateTime: function (newTimezone) {
      this.tick();
      this.update();
    },
    updateSessionDetailFormat: function (newDetailFormat) {
      this.settings.detailFormat = newDetailFormat;
      this.update();
    },
    updateNumberOfPackets: function (newNumPackets) {
      this.settings.numPackets = newNumPackets;
      this.update();
    },
    updateShowPacketTimestamps: function (newShowTimestamps) {
      this.settings.showTimestamps = newShowTimestamps;
      this.update();
    },
    updateQueryOnPageLoad: function (newManualQuery) {
      this.settings.manualQuery = newManualQuery;
      this.update();
    },
    updateSortDirection: function (newSortDirection) {
      this.settings.sortDirection = newSortDirection;
      this.update();
    },
    spiGraphFieldSelected: function (field) {
      this.$set(this, 'spiGraphField', field);
      this.$set(this.settings, 'spiGraph', field.dbField);
      this.$set(this, 'spiGraphTypeahead', field.friendlyName);
      this.update();
    },
    connSrcFieldSelected: function (field) {
      this.$set(this, 'connSrcField', field);
      this.$set(this.settings, 'connSrcField', field.dbField);
      this.$set(this, 'connSrcFieldTypeahead', field.friendlyName);
      this.update();
    },
    connDstFieldSelected: function (field) {
      this.$set(this, 'connDstField', field);
      this.$set(this.settings, 'connDstField', field.dbField);
      this.$set(this, 'connDstFieldTypeahead', field.friendlyName);
      this.update();
    },
    /* starts the clock for the timezone setting */
    startClock: function () {
      this.tick();
      clockInterval = setInterval(() => {
        this.tick();
      }, 1000);
    },
    /* updates the date and format for the timezone setting */
    tick: function () {
      this.date = new Date();
      if (this.settings.timezone === 'gmt') {
        this.dateFormat = 'yyyy/MM/dd HH:mm:ss\'Z\'';
      } else {
        this.dateFormat = 'yyyy/MM/dd HH:mm:ss';
      }
    },
    /**
     * Displays the field.exp instead of field.dbField in the
     * field typeahead inputs
     * @param {string} value The dbField of the field
     */
    formatField: function (value) {
      for (let i = 0, len = this.fields.length; i < len; i++) {
        if (value === this.fields[i].dbField) {
          return this.fields[i].friendlyName;
        }
      }
    },
    /* VIEWS ------------------------------------------- */
    /* creates a view given the view name and expression */
    createView: function () {
      if (!this.newViewName || this.newViewName === '') {
        this.viewFormError = 'No view name specified.';
        return;
      }

      if (!this.newViewExpression || this.newViewExpression === '') {
        this.viewFormError = 'No view expression specified.';
        return;
      }

      let data = {
        shared: this.newViewShared,
        name: this.newViewName,
        expression: this.newViewExpression
      };

      UserService.createView(data, this.userId)
        .then((response) => {
          // add the view to the view list
          if (response.view && response.viewName) {
            if (this.views[response.viewName]) {
              // a shared view with this name already exists
              // so just get the list of views again
              this.getViews();
            } else {
              response.view.name = response.viewName;
              this.views[response.viewName] = response.view;
            }
          }
          // clear the inputs and any error
          this.viewFormError = false;
          this.newViewName = null;
          this.newViewExpression = null;
          this.newViewShared = false;
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    },
    /**
     * Deletes a view given its name
     * @param {Object} view The view to delete
     * @param {string} name The name of the view to delete
     */
    deleteView: function (view, name) {
      UserService.deleteView(view, this.userId)
        .then((response) => {
          // remove the view from the view list
          this.views[name] = null;
          delete this.views[name];
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    },
    /**
     * Sets a view as having been changed
     * @param {string} key The unique id of the changed view
     */
    viewChanged: function (key) {
      this.views[key].changed = true;
    },
    /**
     * Cancels a view change by retrieving the view
     * @param {string} key The unique id of the view
     */
    cancelViewChange: function (key) {
      UserService.getViews(this.userId)
        .then((response) => {
          this.views[key] = response[key];
        })
        .catch((error) => {
          this.viewListError = error.text;
        });
    },
    /**
     * Updates a view
     * @param {string} key The unique id of the view to update
     */
    updateView: function (key) {
      let data = this.views[key];

      if (!data) {
        this.msg = 'Could not find corresponding view';
        this.msgType = 'danger';
        return;
      }

      if (!data.changed) {
        this.msg = 'This view has not changed';
        this.msgType = 'warning';
        return;
      }

      data.key = key;

      UserService.updateView(data, this.userId)
        .then((response) => {
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';
          // set the view as unchanged
          data.changed = false;
        })
        .catch((error) => {
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    },
    /**
     * Shares or unshares a view given its name
     * @param {Object} view The view to share/unshare
     */
    toggleShared: function (view) {
      UserService.toggleShareView(view, view.user)
        .then((response) => {
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    },
    /* CRON QUERIES ------------------------------------ */
    /* creates a cron query given the name, expression, process, and tags */
    createCronQuery: function () {
      if (!this.newCronQueryName || this.newCronQueryName === '') {
        this.cronQueryFormError = 'No cron query name specified.';
        return;
      }

      if (!this.newCronQueryExpression || this.newCronQueryExpression === '') {
        this.cronQueryFormError = 'No cron query expression specified.';
        return;
      }

      if (!this.newCronQueryTags || this.newCronQueryTags === '') {
        this.cronQueryFormError = 'No cron query tags specified.';
        return;
      }

      let data = {
        enabled: true,
        name: this.newCronQueryName,
        query: this.newCronQueryExpression,
        action: this.newCronQueryAction,
        tags: this.newCronQueryTags,
        since: this.newCronQueryProcess
      };

      if (this.newCronQueryNotifier) {
        data.notifier = this.newCronQueryNotifier;
      }

      UserService.createCronQuery(data, this.userId)
        .then((response) => {
          // add the cron query to the view
          this.cronQueryFormError = false;
          data.count = 0; // initialize count to 0
          this.cronQueries[response.key] = data;
          // reset fields
          this.newCronQueryName = '';
          this.newCronQueryTags = '';
          this.newCronQueryExpression = '';
          this.newCronQueryNotifier = '';
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    },
    /**
     * Deletes a cron query given its key
     * @param {string} key The cron query's key
     */
    deleteCronQuery: function (key) {
      UserService.deleteCronQuery(key, this.userId)
        .then((response) => {
          // remove the cron query from the view
          this.cronQueries[key] = null;
          delete this.cronQueries[key];
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    },
    /**
     * Sets a cron query as having been changed
     * @param {string} key The unique id of the cron query
     */
    cronQueryChanged: function (key) {
      this.cronQueries[key].changed = true;
    },
    /**
     * Cancels a cron query change by retrieving the cron query
     * @param {string} key The unique id of the cron query
     */
    cancelCronQueryChange: function (key) {
      UserService.getCronQueries(this.userId)
        .then((response) => {
          this.cronQueries[key] = response[key];
        })
        .catch((error) => {
          this.cronQueryListError = error.text;
        });
    },
    /**
     * Updates a cron query
     * @param {string} key The unique id of the cron query to update
     */
    updateCronQuery: function (key) {
      let data = this.cronQueries[key];

      if (!data) {
        this.msg = 'Could not find corresponding cron query';
        this.msgType = 'danger';
        return;
      }

      if (!data.changed) {
        this.msg = 'This cron query has not changed';
        this.msgType = 'warning';
        return;
      }

      data.key = key;

      UserService.updateCronQuery(data, this.userId)
        .then((response) => {
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';
          // set the cron query as unchanged
          data.changed = false;
        })
        .catch((error) => {
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    },
    /* COLUMN CONFIGURATIONS --------------------------- */
    /**
     * Deletes a previously saved custom column configuration
     * @param {string} name The name of the column config to remove
     * @param {int} index   The index in the array of the column config to remove
     */
    deleteColConfig: function (name, index) {
      UserService.deleteColumnConfig(name, this.userId)
        .then((response) => {
          this.colConfigs.splice(index, 1);
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    },
    /* SPIVIEW FIELD CONFIGURATIONS -------------------- */
    /**
     * Deletes a previously saved custom spiview field configuration
     * @param {string} name The name of the field config to remove
     * @param {int} index   The index in the array of the field config to remove
     */
    deleteSpiviewConfig: function (name, index) {
      UserService.deleteSpiviewFieldConfig(name, this.userId)
        .then((response) => {
          this.spiviewConfigs.splice(index, 1);
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    },
    /* THEMES ------------------------------------------ */
    setTheme: function () {
      // default to default theme if the user has not set a theme
      if (!this.settings.theme) { this.settings.theme = 'default-theme'; }
      if (this.settings.theme.startsWith('custom')) {
        this.settings.theme = 'custom-theme';
        this.creatingCustom = true;
      }
    },
    /* changes the ui theme (picked from existing themes) */
    changeTheme: function (newTheme) {
      this.settings.theme = newTheme;

      $(document.body).removeClass();
      $(document.body).addClass(this.settings.theme);

      this.update();

      this.getThemeColors();
    },
    /* changes a color value of a custom theme and applies the theme */
    changeColor: function (newColor) {
      if (newColor) {
        this[newColor.name] = newColor.value;
      }

      $(document.body).removeClass();
      $(document.body).addClass('custom-theme');

      this.setThemeString();

      this.settings.theme = `custom1:${this.themeString}`;

      this.update(true);
    },
    updateThemeString: function () {
      let colors = this.themeString.split(',');

      this.background = colors[0];
      this.foreground = colors[1];
      this.foregroundAccent = colors[2];

      this.primary = colors[3];
      this.primaryLightest = colors[4];

      this.secondary = colors[5];
      this.secondaryLightest = colors[6];

      this.tertiary = colors[7];
      this.tertiaryLightest = colors[8];

      this.quaternary = colors[9];
      this.quaternaryLightest = colors[10];

      this.water = colors[11];
      this.land = colors[12];

      this.src = colors[13];
      this.dst = colors[14];

      this.changeColor();
    },
    /* PASSWORD ---------------------------------------- */
    /* changes the user's password given the current password, the new password,
     * and confirmation of the new password */
    changePassword: function () {
      if (!this.userId && (!this.currentPassword || this.currentPassword === '')) {
        this.changePasswordError = 'You must enter your current password';
        return;
      }

      if (!this.newPassword || this.newPassword === '') {
        this.changePasswordError = 'You must enter a new password';
        return;
      }

      if (!this.confirmNewPassword || this.confirmNewPassword === '') {
        this.changePasswordError = 'You must confirm your new password';
        return;
      }

      if (this.newPassword !== this.confirmNewPassword) {
        this.changePasswordError = 'Your passwords don\'t match';
        return;
      }

      let data = {
        newPassword: this.newPassword,
        currentPassword: this.currentPassword
      };

      UserService.changePassword(data, this.userId)
        .then((response) => {
          this.changePasswordError = false;
          this.currentPassword = null;
          this.newPassword = null;
          this.confirmNewPassword = null;
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    },
    /* NOTIFIERS --------------------------------------- */
    /* opens the form to create a new notifier */
    createNewNotifier: function (notifier) {
      this.newNotifier = JSON.parse(JSON.stringify(notifier));
    },
    /* gets the type of input associated with a field */
    getFieldInputType: function (field) {
      if (field.type === 'checkbox') {
        return 'checkbox';
      } else if (field.type === 'secret' && !field.showValue) {
        return 'password';
      } else {
        return 'text';
      }
    },
    /* clears the fields of the new notifier form */
    clearNotifierFields: function () {
      this.newNotifier.name = '';
      for (let field of this.newNotifier.fields) {
        field.value = '';
      }
    },
    /* creates a new notifier */
    createNotifier: function () {
      if (!this.newNotifier) {
        this.newNotifierError = 'No notifier chosen';
        return;
      }

      if (!this.newNotifier.name) {
        this.newNotifierError = 'Your new notifier must have a unique name';
        return;
      }

      // make sure required fields are filled
      for (let field of this.newNotifier.fields) {
        if (!field.value && field.required) {
          this.newNotifierError = `${field.name} is required`;
          return;
        }
      }

      this.$http.post('notifiers', { notifier: this.newNotifier })
        .then((response) => {
          // display success message to user
          this.msg = response.data.text || 'Successfully created new notifier.';
          this.msgType = 'success';
          this.notifiersError = '';
          // add notifier to the list
          this.notifiers[response.data.name] = this.newNotifier;
          this.newNotifier = undefined;
        })
        .catch((error) => {
          this.msg = error.text || 'Error creating new notifier.';
          this.msgType = 'danger';
        });
    },
    /* toggles the visibility of the value of secret fields */
    toggleVisibleSecretField: function (field) {
      this.$set(field, 'showValue', !field.showValue);
    },
    /* deletes a notifier */
    removeNotifier: function (name) {
      this.$http.delete(`notifiers/${name}`)
        .then((response) => {
          // display success message to user
          this.msg = response.data.text || 'Successfully deleted notifier.';
          this.msgType = 'success';
          delete this.notifiers[name];
          this.notifiersError = '';
        })
        .catch((error) => {
          this.msg = error.text || 'Error deleting notifier.';
          this.msgType = 'danger';
        });
    },
    /* updates a notifier */
    updateNotifier: function (key, notifier) {
      this.$http.put(`notifiers/${key}`, { notifier: notifier })
        .then((response) => {
          // display success message to user
          this.msg = response.data.text || 'Successfully updated notifier.';
          this.msgType = 'success';
          this.notifiers[response.data.name] = notifier;
          if (key !== response.data.name) {
            // the name has changed, delete the old one
            delete this.notifiers[key];
          }
          this.notifiersError = '';
        })
        .catch((error) => {
          this.msg = error.text || 'Error updating notifier.';
          this.msgType = 'danger';
        });
    },
    /* tests a notifier */
    testNotifier: function (name) {
      this.$http.post(`notifiers/${name}/test`, {})
        .then((response) => {
          // display success message to user
          this.msg = response.data.text || 'Successfully issued alert.';
          this.msgType = 'success';
        })
        .catch((error) => {
          this.msg = error.text || 'Error issuing alert.';
          this.msgType = 'danger';
        });
    },
    /* SHORTCUTS --------------------------------------- */
    /* toggles shared var on a shortcut and saves the shortcut */
    toggleShortcutShared: function (shortcut) {
      this.$set(shortcut, 'shared', !shortcut.shared);
      this.updateShortcut(shortcut);
    },
    /* opens up text area to edit shortcut value */
    toggleEditShortcut: function (shortcut) {
      if (!shortcut.newValue) {
        this.$set(shortcut, 'newValue', shortcut.value);
      } else {
        this.$set(shortcut, 'newValue', undefined);
      }
    },
    /* creates a new shortcut */
    createShortcut: function () {
      if (!this.newShortcutName) {
        this.shortcutFormError = 'Enter a unique shortcut name';
        return;
      }

      if (!this.newShortcutValue) {
        this.shortcutFormError = 'Enter a value for your new shortcut';
        return;
      }

      const data = {
        name: this.newShortcutName,
        type: this.newShortcutType,
        value: this.newShortcutValue,
        shared: this.newShortcutShared,
        description: this.newShortcutDescription
      };

      this.$http.post('lookups', { var: data })
        .then((response) => {
          // add it to the list
          this.shortcuts.push(response.data.var);
          // clear the inputs and any error
          this.shortcutFormError = false;
          this.newShortcutName = '';
          this.newShortcutValue = '';
          this.newShortcutShared = false;
          this.newShortcutDescription = '';
          // display success message to user
          this.msg = response.data.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          this.msg = error.text;
          this.msgType = 'danger';
        });
    },
    /* updates a specified shortcut (only shared and value are editable) */
    updateShortcut: function (shortcut) {
      let data = {
        name: shortcut.name,
        type: shortcut.type,
        value: shortcut.value,
        shared: shortcut.shared,
        userId: shortcut.userId,
        description: shortcut.description
      };

      if (shortcut.newValue) {
        data.value = shortcut.newValue;
      }

      this.$http.put(`lookups/${shortcut.id}`, { var: data })
        .then((response) => {
          // update value and clear out new value
          // so it can be used to determine editing
          if (shortcut.newValue) {
            this.$set(shortcut, 'value', response.data.var.value);
            this.$set(shortcut, 'newValue', undefined);
            delete shortcut.newValue;
          }
          // display success message to user
          this.msg = response.data.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          this.msg = error.text;
          this.msgType = 'danger';
        });
    },
    /* deletes a shortcut and removes it from the shortcuts array */
    deleteShortcut: function (shortcut, index) {
      this.$http.delete(`lookups/${shortcut.id}`)
        .then((response) => {
          // remove it from the array
          this.shortcuts.splice(index, 1);
          // display success message to user
          this.msg = response.data.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    },

    /* helper functions ---------------------------------------------------- */
    /* retrievs the theme colors from the document body's property values */
    getThemeColors: function () {
      let styles = window.getComputedStyle(document.body);

      this.background = styles.getPropertyValue('--color-background').trim() || '#FFFFFF';
      this.foreground = styles.getPropertyValue('--color-foreground').trim() || '#333333';
      this.foregroundAccent = styles.getPropertyValue('--color-foreground-accent').trim();

      this.primary = styles.getPropertyValue('--color-primary').trim();
      this.primaryLightest = styles.getPropertyValue('--color-primary-lightest').trim();

      this.secondary = styles.getPropertyValue('--color-secondary').trim();
      this.secondaryLightest = styles.getPropertyValue('--color-secondary-lightest').trim();

      this.tertiary = styles.getPropertyValue('--color-tertiary').trim();
      this.tertiaryLightest = styles.getPropertyValue('--color-tertiary-lightest').trim();

      this.quaternary = styles.getPropertyValue('--color-quaternary').trim();
      this.quaternaryLightest = styles.getPropertyValue('--color-quaternary-lightest').trim();

      this.water = styles.getPropertyValue('--color-water').trim();
      this.land = styles.getPropertyValue('--color-land').trim() || this.primary;

      this.src = styles.getPropertyValue('--color-src').trim() || '#CA0404';
      this.dst = styles.getPropertyValue('--color-dst').trim() || '#0000FF';

      this.setThemeString();
    },
    setThemeString: function () {
      this.themeString = `${this.background},${this.foreground},${this.foregroundAccent},${this.primary},${this.primaryLightest},${this.secondary},${this.secondaryLightest},${this.tertiary},${this.tertiaryLightest},${this.quaternary},${this.quaternaryLightest},${this.water},${this.land},${this.src},${this.dst}`;
    },
    /* retrieves the specified user's settings */
    getSettings: function () {
      UserService.getSettings(this.userId)
        .then((response) => {
          // set the user settings individually
          for (let key in response) {
            this.$set(this.settings, key, response[key]);
          }

          // set defaults if a user setting doesn't exists
          // so that radio buttons show the default value
          if (!response.timezone) {
            this.$set(this.settings, 'timezone', 'local');
          }
          if (!response.detailFormat) {
            this.$set(this.settings, 'detailFormat', 'last');
          }
          if (!response.numPackets) {
            this.$set(this.settings, 'numPackets', 'last');
          }
          if (!response.showTimestamps) {
            this.$set(this.settings, 'showTimestamps', 'last');
          }
          if (!response.manualQuery) {
            this.$set(this.settings, 'manualQuery', false);
          }

          this.getFields();

          this.loading = false;

          this.setTheme();
          this.startClock();
        })
        .catch((error) => {
          this.loading = false;
          if (error.text === 'User not found') {
            this.error = `<div class="text-center">
                            ${error.text}
                            <small><a href="settings">View your own settings?</a></small>
                          </div>`;
          } else {
            this.error = error.text;
          }
          this.displayName = '';
        });
    },
    /* retrieves moloch fields and visible column headers for sessions table
     * adds custom columns to fields
     * sets user settings for spigraph field & connections src/dst fields
     * creates fields map for quick lookups
     */
    getFields: function () {
      // get fields from field service then get sessionsNew state
      FieldService.get(true)
        .then((response) => {
          this.fields = JSON.parse(JSON.stringify(response));
          this.fieldsPlus = JSON.parse(JSON.stringify(response));
          this.fieldsPlus.push({
            dbField: 'ip.dst:port',
            exp: 'ip.dst:port',
            help: 'Destination IP:Destination Port',
            group: 'general',
            friendlyName: 'Dst IP:Dst Port'
          });

          // add custom columns to the fields array
          for (let key in customCols) {
            if (customCols.hasOwnProperty(key)) {
              this.fields.push(customCols[key]);
            }
          }

          // update the user settings for spigraph field & connections src/dst fields
          // NOTE: dbField is saved in settings, but show the field's friendlyName
          for (let i = 0, len = this.fieldsPlus.length; i < len; i++) {
            let field = this.fieldsPlus[i];
            if (this.settings.spiGraph === field.dbField) {
              this.$set(this, 'spiGraphField', field);
              this.$set(this, 'spiGraphTypeahead', field.friendlyName);
            }
            if (this.settings.connSrcField === field.dbField) {
              this.$set(this, 'connSrcField', field);
              this.$set(this, 'connSrcFieldTypeahead', field.friendlyName);
            }
            if (this.settings.connDstField === field.dbField) {
              this.$set(this, 'connDstField', field);
              this.$set(this, 'connDstFieldTypeahead', field.friendlyName);
            }
          }

          // build fields map for quick lookup by dbField
          this.fieldsMap = {};
          for (let i = 0, len = this.fields.length; i < len; ++i) {
            let field = this.fields[i];
            this.fieldsMap[field.dbField] = field;
          }

          // get the visible headers for the sessions table configuration
          UserService.getState('sessionsNew')
            .then((response) => {
              this.setupColumns(response.data.visibleHeaders);
              // if the sort column setting does not match any of the visible
              // headers, set the sort column setting to last
              if (response.data.visibleHeaders.indexOf(this.settings.sortColumn) === -1) {
                this.settings.sortColumn = 'last';
              }
            })
            .catch(() => {
              this.setupColumns(['firstPacket', 'lastPacket', 'src', 'srcPort', 'dst', 'dstPort', 'totPackets', 'dbby', 'node', 'info']);
            });
        });
    },
    /* retrieves the specified user's views */
    getViews: function () {
      UserService.getViews(this.userId)
        .then((response) => {
          this.views = response;
        })
        .catch((error) => {
          this.viewListError = error.text;
        });
    },
    /* retrieves the specified user's cron queries */
    getCronQueries: function () {
      UserService.getCronQueries(this.userId)
        .then((response) => {
          this.cronQueries = response;
        })
        .catch((error) => {
          this.cronQueryListError = error.text;
        });
    },
    /* retrieves the specified user's custom column configurations */
    getColConfigs: function () {
      UserService.getColumnConfigs(this.userId)
        .then((response) => {
          this.colConfigs = response;
        })
        .catch((error) => {
          this.colConfigError = error.text;
        });
    },
    /* retrieves the specified user's custom spiview fields configurations.
     * dissects the visible spiview fields for view consumption */
    getSpiviewConfigs: function () {
      UserService.getSpiviewFields(this.userId)
        .then((response) => {
          this.spiviewConfigs = response;

          for (let x = 0, xlen = this.spiviewConfigs.length; x < xlen; ++x) {
            let config = this.spiviewConfigs[x];
            let spiParamsArray = config.fields.split(',');

            // get each field from the spi query parameter and issue
            // a query for one field at a time
            for (let i = 0, len = spiParamsArray.length; i < len; ++i) {
              let param = spiParamsArray[i];
              let split = param.split(':');
              let fieldID = split[0];
              let count = split[1];

              let field;

              for (let key in this.fields) {
                if (this.fields[key].dbField === fieldID) {
                  field = this.fields[key];
                  break;
                }
              }

              if (field) {
                if (!config.fieldObjs) { config.fieldObjs = []; }

                field.count = count;
                config.fieldObjs.push(field);
              }
            }
          }
        })
        .catch((error) => {
          this.spiviewConfigError = error.text;
        });
    },
    /* retrieves the types of notifiers that can be configured */
    getNotifierTypes: function () {
      this.$http.get('notifierTypes')
        .then((response) => {
          this.notifierTypes = response.data;
        }, (error) => {
          this.notifiersError = error.text || error;
        });
    },
    /* retrieves the notifiers that have been configured */
    getNotifiers: function () {
      this.$http.get('notifiers')
        .then((response) => {
          this.notifiers = response.data;
        }, (error) => {
          this.notifiersError = error.text || error;
        });
    },
    getShortcuts: function () {
      let url = 'lookups';
      if (this.userId) { url += `?userId=${this.userId}`; }
      this.$http.get(url)
        .then((response) => {
          this.shortcuts = response.data;
        }, (error) => {
          this.shortcutsListError = error.text || error;
        });
    },
    /**
     * Setup this.columns with a list of field objects
     * @param {array} colIdArray The array of column ids
     */
    setupColumns: function (colIdArray) {
      this.columns = [];
      for (let i = 0, len = colIdArray.length; i < len; ++i) {
        this.columns.push(this.getField(colIdArray[i]));
      }
    },
    /**
     * Gets the field that corresponds to a field's dbField value
     * @param {string} dbField The fields dbField value
     * @returns {object} field The field that corresponds to the entered dbField
     */
    getField: function (dbField) {
      return this.fieldsMap[dbField];
    }
  },
  beforeDestroy: function () {
    if (clockInterval) { clearInterval(clockInterval); }

    // remove userId route query parameter so that when a user
    // comes back to this page, they are on their own settings
    this.$router.replace({
      query: {
        ...this.$route.query,
        userId: undefined
      }
    });
  }
};
</script>

<style>
/* settings page, navbar, and content styles - */
.settings-page {
  margin-top: 36px;
}
.settings-content {
  margin-top: 90px;
  margin-left: 0;
  margin-right: 0;
  overflow-x: hidden;
}
.settings-content .settings-right-panel {
  overflow-x: auto;
}

.settings-page .sub-navbar {
  z-index: 4;
}

/* fixed tab buttons */
.settings-page div.nav-pills {
  position: fixed;
}

/* make sure the form is taller than the nav pills */
.settings-page form {
  min-height: 280px;
}

.settings-page .settings-error {
  margin-top: 6rem;
  margin-bottom: 1rem;
}

/* apply theme color to notifier cards */
.card {
  box-shadow: inset 0 1px 1px rgba(0, 0, 0, .05);
  background-color: var(--color-gray-lighter);
  border: 1px solid var(--color-gray-light);
}

/* shortcuts form */
.settings-page .var-form {
  box-shadow: inset 0 1px 1px rgba(0, 0, 0, .05);
  background-color: var(--color-gray-lighter);
  border: 1px solid var(--color-gray-light);
  border-radius: 3px;
}
.settings-page .var-form input[type='checkbox'] {
  margin-top: 0.75rem;
}

/* shorcuts table */
.settings-page .shortcut-value {
  max-width: 400px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.settings-page .shortcut-btns {
  min-width: 80px;
}

/* theme displays ----------------- */
.field {
  cursor: pointer;
  padding: 0 1px;
  border-radius: 3px;
  border: 1px solid transparent;
}
.field .fa {
  opacity: 0;
  visibility: hidden;
}
.field:hover .fa {
  opacity: 1;
  visibility: visible;
}

.settings-page .control-label {
  font-weight: bolder;
}

.settings-page .theme-display {
  overflow: hidden;
  border-radius: 6px;
  padding-bottom: 20px;
}

.settings-page .navbar {
  min-height: 20px;
  height: 30px;
  border-radius: 6px 6px 0 0;
  z-index: 1;
}

.settings-page .navbar .moloch-logo {
  height: 36px;
  position: absolute;
  top: 2px;
  left: 6px;
}

.settings-page .navbar .nav {
  position: absolute;
  left: 50px
}

.settings-page .navbar .navbar-nav {
  margin-right: -8px;
}

.settings-page .navbar .navbar-nav .health-green {
  color: #00aa00;
}

.settings-page .navbar-dark a.active {
  color: #FFFFFF;
}
.settings-page .navbar-dark a {
  color: rgba(255, 255, 255, 0.5);
}
.settings-page .navbar-dark a:not(.active):hover {
  color: rgba(255, 255, 255, 0.75);
}

.settings-page .display-sub-navbar {
  height: 40px;
  position: relative;
  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}
.settings-page .display-sub-navbar .input-group {
  padding-top: 4px;
}

.settings-page .display-sub-navbar .display-sub-navbar-buttons {
  margin-top: 4px;
  margin-right: 4px;
  margin-left: -10px;
}

.settings-page .display-sub-navbar .text-theme-accent {
  display: inline-block;
  margin-left: -20px;
  padding-top: 11px;
  font-size: 12px;
}

.settings-page .display-sub-sub-navbar {
  border-radius: 0 0 6px 6px;
  margin-top: -6px;
  padding-top: 6px;
}

/* default */
.settings-page .default-theme .navbar {
  background-color: #530763;
  border-color: #360540;
}

.settings-page .default-theme .input-group-prepend > .input-group-text {
  color: #333333 !important;
  background-color: #EEEEEE !important;
  border-color: #CCCCCC !important;
}

.settings-page .default-theme .display-sub-navbar {
  background-color: #EDFCFF;
}

.settings-page .default-theme .display-sub-sub-navbar {
  background-color: #FFF7E5;
}

.settings-page .default-theme .display-sub-navbar .text-theme-accent {
  color: #76207d;
}

.settings-page .default-theme .display-sub-navbar .btn-theme-primary-display {
  color: #FFFFFF;
  background-color: #830B9C;
  border-color: #530763;
}
.settings-page .default-theme .display-sub-navbar .btn-theme-secondary-display {
  color: #FFFFFF;
  background-color: #1F1FA5;
  border-color: #1A1A87;
}
.settings-page .default-theme .display-sub-navbar .btn-theme-tertiary-display {
  color: #FFFFFF;
  background-color: #079B72;
  border-color: #077D5C;
}
.settings-page .default-theme .display-sub-navbar .btn-theme-quaternary-display {
  color: #FFFFFF;
  background-color: #ECB30A;
  border-color: #CD9A09;
}

.settings-page .default-theme .dropdown-menu {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .default-theme .dropdown-menu .dropdown-item {
  color: #212529;
  padding: 2px 8px;
}
.settings-page .default-theme .dropdown-menu .dropdown-item:hover {
  background-color: #EEEEEE;
  color: #212529;
}
.settings-page .default-theme .dropdown-menu .dropdown-item:focus {
  background-color: #FFFFFF;
  color: #212529;
}
.settings-page .default-theme .dropdown-menu .dropdown-item.active {
  background-color: #830B9C;
  color: #FFFFFF;
}

.settings-page .default-theme .field {
  color: #76207d;
}
.settings-page .default-theme .field:hover {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
}

/* blue */
.settings-page .blue-theme .navbar {
  background-color: #163254;
  border-color: #000000;
}

.settings-page .blue-theme .input-group-prepend > .input-group-text {
  color: #333333 !important;
  background-color: #EEEEEE !important;
  border-color: #CCCCCC !important;
}

.settings-page .blue-theme .display-sub-navbar {
  background-color: #DBECE7;
}

.settings-page .blue-theme .display-sub-sub-navbar {
  background-color: #FFF7E5;
}

.settings-page .blue-theme .display-sub-navbar .text-theme-accent {
  color: #9A4E93;
}

.settings-page .blue-theme .display-sub-navbar .btn-theme-primary-display {
  color: #FFFFFF;
  background-color: #214B78;
  border-color: #530763;
}
.settings-page .blue-theme .display-sub-navbar .btn-theme-secondary-display {
  color: #FFFFFF;
  background-color: #3D7B7E;
  border-color: #306264;
}
.settings-page .blue-theme .display-sub-navbar .btn-theme-tertiary-display {
  color: #FFFFFF;
  background-color: #42B7C5;
  border-color: #33919b;
}
.settings-page .blue-theme .display-sub-navbar .btn-theme-quaternary-display {
  color: #FFFFFF;
  background-color: #ECB30A;
  border-color: #CD9A09;
}

.settings-page .blue-theme .dropdown-menu {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .blue-theme .dropdown-menu .dropdown-item {
  color: #212529;
  padding: 2px 8px;
}
.settings-page .blue-theme .dropdown-menu .dropdown-item:hover {
  background-color: #EEEEEE;
  color: #212529;
}
.settings-page .blue-theme .dropdown-menu .dropdown-item:focus {
  background-color: #FFFFFF;
  color: #212529;
}
.settings-page .blue-theme .dropdown-menu .dropdown-item.active {
  background-color: #214B78;
  color: #FFFFFF;
}

.settings-page .green-theme .field {
  color: #9A4E93;
}
.settings-page .green-theme .field:hover {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
}

/* green */
.settings-page .green-theme .navbar {
  background-color: #2A6E3d;
  border-color: #235A32;
}

.settings-page .green-theme .input-group-prepend > .input-group-text {
  color: #333333 !important;
  background-color: #EEEEEE !important;
  border-color: #CCCCCC !important;
}

.settings-page .green-theme .display-sub-navbar {
  background-color: #DBECE7;
}

.settings-page .green-theme .display-sub-sub-navbar {
  background-color: #FDFFE2;
}

.settings-page .green-theme .display-sub-navbar .text-theme-accent {
  color: #38738d;
}

.settings-page .green-theme .display-sub-navbar .btn-theme-primary-display {
  color: #FFFFFF;
  background-color: #2A7847;
  border-color: #2A6E3d;
}
.settings-page .green-theme .display-sub-navbar .btn-theme-secondary-display {
  color: #FFFFFF;
  background-color: #3D7B7E;
  border-color: #306264;
}
.settings-page .green-theme .display-sub-navbar .btn-theme-tertiary-display {
  color: #FFFFFF;
  background-color: #91C563;
  border-color: #7EAA57;
}
.settings-page .green-theme .display-sub-navbar .btn-theme-quaternary-display {
  color: #FFFFFF;
  background-color: #BECF14;
  border-color: #ADBC12;
}

.settings-page .green-theme .dropdown-menu {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .green-theme .dropdown-menu .dropdown-item {
  color: #212529;
  padding: 2px 8px;
}
.settings-page .green-theme .dropdown-menu .dropdown-item:hover {
  background-color: #EEEEEE;
  color: #212529;
}
.settings-page .green-theme .dropdown-menu .dropdown-item:focus {
  background-color: #FFFFFF;
  color: #212529;
}
.settings-page .green-theme .dropdown-menu .dropdown-item.active {
  background-color: #2A7847;
  color: #FFFFFF;
}

.settings-page .blue-theme .field {
  color: #38738d;
}
.settings-page .blue-theme .field:hover {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
}

/* cotton candy */
.settings-page .cotton-candy-theme .navbar {
  background-color: #B0346D;
  border-color: #9B335A;
}

.settings-page .cotton-candy-theme .input-group-prepend > .input-group-text {
  color: #333333 !important;
  background-color: #EEEEEE !important;
  border-color: #CCCCCC !important;
}

.settings-page .cotton-candy-theme .display-sub-navbar {
  background-color: #D7F1FF;
}

.settings-page .cotton-candy-theme .display-sub-sub-navbar {
  background-color: #FFF8DD;
}

.settings-page .cotton-candy-theme .display-sub-navbar .text-theme-accent {
  color: #9A4E93;
}

.settings-page .cotton-candy-theme .display-sub-navbar .btn-theme-primary-display {
  color: #FFFFFF;
  background-color: #C43D75;
  border-color: #B0346D;
}
.settings-page .cotton-candy-theme .display-sub-navbar .btn-theme-secondary-display {
  color: #FFFFFF;
  background-color: #3CAED2;
  border-color: #389BBE;
}
.settings-page .cotton-candy-theme .display-sub-navbar .btn-theme-tertiary-display {
  color: #FFFFFF;
  background-color: #079B72;
  border-color: #077D5C;
}
.settings-page .cotton-candy-theme .display-sub-navbar .btn-theme-quaternary-display {
  color: #FFFFFF;
  background-color: #F39C12;
  border-color: #D78A10;
}

.settings-page .cotton-candy-theme .dropdown-menu {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .cotton-candy-theme .dropdown-menu .dropdown-item {
  color: #212529;
  padding: 2px 8px;
}
.settings-page .cotton-candy-theme .dropdown-menu .dropdown-item:hover {
  background-color: #EEEEEE;
  color: #212529;
}
.settings-page .cotton-candy-theme .dropdown-menu .dropdown-item:focus {
  background-color: #FFFFFF;
  color: #212529;
}
.settings-page .cotton-candy-theme .dropdown-menu .dropdown-item.active {
  background-color: #C43D75;
  color: #FFFFFF;
}

.settings-page .cotton-candy-theme .field {
  color: #9A4E93;
}
.settings-page .cotton-candy-theme .field:hover {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
}

/* green on black */
.settings-page .dark-2-theme .navbar {
  background-color: #363A7D;
  border-color: #2F2F5F;
}

.settings-page .dark-2-theme .display-sub-navbar {
  background-color: #0F2237;
}

.settings-page .dark-2-theme .display-sub-sub-navbar {
  background-color: #191919;
}

.settings-page .dark-2-theme .display-sub-navbar .text-theme-accent {
  color: #00CA16;
}

.settings-page .dark-2-theme .display-sub-navbar .btn-theme-primary-display {
  color: #FFFFFF;
  background-color: #444A9B;
  border-color: #363A7D;
}
.settings-page .dark-2-theme .display-sub-navbar .btn-theme-secondary-display {
  color: #FFFFFF;
  background-color: #2E5D9B;
  border-color: #264B7E;
}
.settings-page .dark-2-theme .display-sub-navbar .btn-theme-tertiary-display {
  color: #FFFFFF;
  background-color: #00BB20;
  border-color: #009D1D;
}
.settings-page .dark-2-theme .display-sub-navbar .btn-theme-quaternary-display {
  color: #FFFFFF;
  background-color: #686868;
  border-color: #4B4B4B;
}

.settings-page .dark-2-theme .input-group-prepend > .input-group-text {
  color: #C7C7C7 !important;
  background-color: #111111 !important;
  border-color: #AAAAAA !important;
}

.settings-page .dark-2-theme .dropdown-menu {
  background-color: #111111;
  border-color: #555555;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .dark-2-theme .dropdown-menu .dropdown-item {
  color: #C7C7C7;
  padding: 2px 8px;
}
.settings-page .dark-2-theme .dropdown-menu .dropdown-item:hover {
  background-color: #555555;
  color: #C7C7C7;
}
.settings-page .dark-2-theme .dropdown-menu .dropdown-item:focus {
  background-color: #111111;
  color: #C7C7C7;
}
.settings-page .dark-2-theme .dropdown-menu .dropdown-item.active {
  background-color: #444A9B;
  color: #333333;
}

.settings-page .dark-2-theme .field {
  color: #00CA16;
}
.settings-page .dark-2-theme .field:hover {
  background-color: #111111;
  border-color: #555555;
}

/* Dark Blue */
.settings-page .dark-3-theme .navbar {
  background-color: #23837b;
  border-color: #1B655F;
}

.settings-page .dark-3-theme .navbar-dark li.active a {
  color: #FFFFFF;
  background-color: #1B655F !important;
}

.settings-page .dark-3-theme .display-sub-navbar {
  background-color: #154369;
}

.settings-page .dark-3-theme .display-sub-sub-navbar {
  background-color: #460C3A;
}
.settings-page .dark-3-theme .display-sub-navbar .text-theme-accent{
  color: #A6A8E2;
}

.settings-page .dark-3-theme .display-sub-navbar .btn-theme-primary-display {
  color: #FFFFFF;
  background-color: #2AA198;
  border-color: #23837b;
}
.settings-page .dark-3-theme .display-sub-navbar .btn-theme-secondary-display {
  color: #FFFFFF;
  background-color: #268BD2;
  border-color: #1F76B4;
}
.settings-page .dark-3-theme .display-sub-navbar .btn-theme-tertiary-display {
  color: #FFFFFF;
  background-color: #FF6E67;
  border-color: #D75F59;
}
.settings-page .dark-3-theme .display-sub-navbar .btn-theme-quaternary-display {
  color: #FFFFFF;
  background-color: #D33682;
  border-color: #B42C72;
}

.settings-page .dark-3-theme .input-group-prepend > .input-group-text {
  color: #ADC1C3 !important;
  background-color: #002833 !important;
  border-color: #AAAAAA !important;
}

.settings-page .dark-3-theme .dropdown-menu {
  background-color: #002833;
  border-color: #555555;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .dark-3-theme .dropdown-menu .dropdown-item {
  color: #ADC1C3;
  padding: 2px 8px;
}
.settings-page .dark-3-theme .dropdown-menu .dropdown-item:hover {
  background-color: #555555;
  color: #ADC1C3;
}
.settings-page .dark-3-theme .dropdown-menu .dropdown-item:focus {
  background-color: #002833;
  color: #ADC1C3;
}
.settings-page .dark-3-theme .dropdown-menu .dropdown-item.active {
  background-color: #2AA198;
  color: #333333;
}

.settings-page .dark-3-theme .field {
  color: #A6A8E2;
}
.settings-page .dark-3-theme .field:hover {
  background-color: #002833;
  border-color: #555555;
}

/* Custom */
.settings-page .custom-theme .theme-display {
  background-color: var(--color-background);
  color: var(--color-foreground);
  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

.settings-page .custom-theme .navbar {
  background-color: var(--color-primary-dark);
  border-color: var(--color-primary-darker);
}

.settings-page .custom-theme .display-sub-navbar {
  background-color: var(--color-secondary-lightest);
}

.settings-page .custom-theme .display-sub-sub-navbar {
  border-radius: 0;
  height: 46px;
  background-color: var(--color-quaternary-lightest);
  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

.settings-page .custom-theme .display-sub-navbar .btn-theme-primary-display {
  color: #FFFFFF;
  background-color: var(--color-primary);
  border-color: var(--color-primary-dark);
}
.settings-page .custom-theme .display-sub-navbar .btn-theme-secondary-display {
  color: #FFFFFF;
  background-color: var(--color-secondary);
  border-color: var(--color-secondary-dark);
}
.settings-page .custom-theme .display-sub-navbar .btn-theme-tertiary-display {
  color: #FFFFFF;
  background-color: var(--color-tertiary);
  border-color: var(--color-tertiary-dark);
}
.settings-page .custom-theme .display-sub-navbar .btn-theme-quaternary-display {
  color: #FFFFFF;
  background-color: var(--color-quaternary);
  border-color: var(--color-quaternary-dark);
}

.settings-page .custom-theme .dropdown-menu {
  color: var(--color-foreground);
  background-color: var(--color-background, #FFFFFF);
  border-color: var(--color-gray-light);
}

.settings-page .custom-theme .field {
  color: var(--color-foreground-accent);
}
.settings-page .custom-theme .field:hover {
  z-index: 4;
  background-color: var(--color-white);
  border: 1px solid var(--color-gray-light);
}

.settings-page .custom-theme .session-detail-ts {
  color: var(--color-foreground-accent);
  padding: 0 4px;
}
.settings-page .custom-theme .sessionsrc > pre {
  color: var(--color-src, #CA0404);
  padding: 2px 4px;
}
.settings-page .custom-theme .sessiondst > pre {
  color: var(--color-dst, #0000FF);
  padding: 2px 4px;
}
</style>
