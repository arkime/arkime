import { Component, OnInit, OnDestroy } from '@angular/core';
import { trigger, style, transition, animate, keyframes, query } from '@angular/animations';

import { TimerObservable } from 'rxjs/observable/TimerObservable';

import { ParliamentService } from './parliament.service';
import { AuthService } from '../auth/auth.service';
import { Parliament } from './parliament';
import { Auth, Login } from '../auth/auth';

@Component({
  templateUrl : './parliament.html',
  styleUrls   : [ './parliament.css' ],
  animations  : [
    trigger('clusters', [
      transition('* => *', [
        query(':enter',
          animate('.4s ease-in', keyframes([
            style({opacity: 0, width: '0px'}),
            style({opacity: .5, width: '50px'}),
            style({opacity: 1, width: '100%'}),
          ])), {optional: true}),
        query(':leave',
          animate('.4s ease-out', keyframes([
            style({opacity: 1, width: '100%'}),
            style({opacity: .5, width: '50px'}),
            style({opacity: 0, width: '0px'}),
          ])), {optional: true}),
      ])
    ])
  ]
})
export class ParliamentComponent implements OnInit, OnDestroy {

  /* setup ----------------------------------------------------------------- */
  // subscriber for timer that issues requests for the parliament
  private timerSubscriber;
  // subscriber for the refresh interval variable
  private refreshIntervalSubscriber;
  // timeout for debouncing the search input
  private timeout;
  // save old parliament for when reordering items in the parliament fails
  private oldParliamentOrder;
  // data refresh interval default
  private refreshInterval = '15000';
  // whether the parliament data has been initialized
  private initialized = false;

  // display error messages
  error = '';

  // page data
  parliament = { groups: [] };

  // search term input default
  searchTerm = '';

  // keeps track of the number of filtered clusters (from search input)
  numFilteredClusters: number;

  // new group form variales
  showNewGroupForm = false;
  newGroupTitle = '';
  newGroupDescription = '';

  // dnd flags to manage where a user can drag/drop clusters/groups
  dragClusters = true;
  dragGroups = true;

  constructor(
    private parliamentService: ParliamentService,
    public authService: AuthService
  ) {
    this.refreshIntervalSubscriber = parliamentService.refreshInterval$.subscribe(
      (interval) => {
        this.refreshInterval = interval;

        if (this.refreshInterval) {
         if (this.initialized) { this.loadData(); }
         this.startAutoRefresh();
       } else {
         this.stopAutoRefresh();
       }
      }
    );
  }

  ngOnInit() {
    this.loadData();
  }

  ngOnDestroy() {
    this.stopAutoRefresh();
    this.refreshIntervalSubscriber.unsubscribe();
  }

  /* controller functions -------------------------------------------------- */
  // Loads the parliament or displays an error
  loadData() {
    this.parliamentService.getParliament()
      .subscribe(
        (data) => {
          this.error = '';
          this.updateParliament(data);
          this.filterClusters();
          this.oldParliamentOrder = JSON.parse(JSON.stringify(this.parliament));
        },
        (err) => {
          this.error = err.error.text ||
            `Error fetching health and status information about Molochs in your parliament.
             The information displayed below is likely out of date`;
        }
      );
  }

  startAutoRefresh() {
    if (!this.refreshInterval) { return; }
    const timer = TimerObservable.create(parseInt(this.refreshInterval, 10), parseInt(this.refreshInterval, 10));
    this.timerSubscriber = timer.subscribe(() => {
      if (this.refreshInterval) { this.loadData(); }
    });
  }

  stopAutoRefresh() {
    if (this.timerSubscriber) { this.timerSubscriber.unsubscribe(); }
  }

  // Updates fetched parliament with current view flags and values
  // Assumes that groups and clusters within groups are in the same order
  updateParliament(data) {
    if (!this.initialized) {
      this.parliament   = data;
      this.initialized  = true;
      return;
    }

    for (let g = 0, glen = data.groups.length; g < glen; ++g) {
      const newGroup = data.groups[g];
      const oldGroup = this.parliament.groups[g];

      newGroup.error                  = oldGroup.error;
      newGroup.newTitle               = oldGroup.newTitle;
      newGroup.newDescription         = oldGroup.newDescription;
      newGroup.filteredClusters       = oldGroup.filteredClusters;
      newGroup.showEditGroupForm      = oldGroup.showEditGroupForm;
      newGroup.showNewClusterForm     = oldGroup.showNewClusterForm;
      newGroup.newClusterTitle        = oldGroup.newClusterTitle;
      newGroup.newClusterDescription  = oldGroup.newClusterDescription;
      newGroup.newClusterUrl          = oldGroup.newClusterUrl;
      newGroup.newClusterLocalUrl     = oldGroup.newClusterLocalUrl;
      newGroup.newClusterMultiviewer  = oldGroup.newClusterMultiviewer;
      newGroup.newClusterDisabled     = oldGroup.newClusterDisabled;

      for (let c = 0, clen = newGroup.clusters.length; c < clen; ++c) {
        const newCluster = newGroup.clusters[c];
        const oldCluster = oldGroup.clusters[c];

        newCluster.error                = oldCluster.error;
        newCluster.newTitle             = oldCluster.newTitle;
        newCluster.newDescription       = oldCluster.newDescription;
        newCluster.newUrl               = oldCluster.newUrl;
        newCluster.newLocalUrl          = oldCluster.newLocalUrl;
        newCluster.newMultiviewer       = oldCluster.newMultiviewer;
        newCluster.newDisabled          = oldCluster.newDisabled;
        newCluster.showEditClusterForm  = oldCluster.showEditClusterForm;
      }
    }

    this.parliament = data;
  }

  // Remove UI only properties from groups and clusters in a parliament
  sanitizeParliament(data) {
    for (const group of data.groups) {
      group.error                 = undefined;
      group.newTitle              = undefined;
      group.newDescription        = undefined;
      group.filteredClusters      = undefined;
      group.showEditGroupForm     = undefined;
      group.showNewClusterForm    = undefined;
      group.newClusterTitle       = undefined;
      group.newClusterDescription = undefined;
      group.newClusterUrl         = undefined;
      group.newClusterLocalUrl    = undefined;
      group.newClusterMultiviewer = undefined;
      group.newClusterDisabled    = undefined;
      group.filteredClusters      = undefined;

      for (const cluster of group.clusters) {
        cluster.error               = undefined;
        cluster.newTitle            = undefined;
        cluster.newDescription      = undefined;
        cluster.newUrl              = undefined;
        cluster.newLocalUrl         = undefined;
        cluster.newMultiviewer      = undefined;
        cluster.newDisabled         = undefined;
        cluster.showEditClusterForm = undefined;
      }
    }
  }

  // Removes clusters that don't match the search term provided
  filterClusters() {
    this.numFilteredClusters = 0;

    for (const group of this.parliament.groups) {
      if (!this.searchTerm) {
        group.filteredClusters = Object.assign([], group.clusters);
        this.numFilteredClusters += group.filteredClusters.length;
        continue;
      }
      group.filteredClusters = Object.assign([], group.clusters)
        .filter((item) => {
          return item.title.toLowerCase().indexOf(this.searchTerm.toLowerCase()) > -1;
        });
      this.numFilteredClusters += group.filteredClusters.length;
    }
  }


  /* page functions -------------------------------------------------------- */
  getTrackingId(index, item) {
    return item.id;
  }

  getIssueTrackingId(index, issue) {
    if (issue.node) {
      return `${issue.node.replace(/\s/g, '')}-${issue.type}`;
    } else {
      return issue.type;
    }
  }

  debounceSearch() {
    if (this.timeout) { clearTimeout(this.timeout); }
    this.timeout = setTimeout(() => {
      this.filterClusters();
    }, 400);
  }

  // Creates a new group in the parliament
  // newGroupTitle is required/
  createNewGroup() {
    this.error = '';

    if (!this.newGroupTitle) {
      this.error = 'A group must have a title';
      return;
    }

    const newGroup = {
      title       : this.newGroupTitle,
      description : this.newGroupDescription
    };

    this.parliamentService.createGroup(newGroup)
      .subscribe(
        (data) => {
          this.showNewGroupForm = false;
          this.parliament.groups.push(data.group);
          this.filterClusters();
        },
        (err) => {
          this.error = err.error.text || 'Unable to create group';
        }
      );
  }

  /**
   * Displays form fields to edit a group's title and description
   * Prefills the inputs with the existing group's title and description
   * @param {object} group - the group to display a form for
   */
  displayEditGroupForm(group) {
    group.showEditGroupForm = true;
    group.newTitle = group.title;
    group.newDescription = group.description;
  }

  /**
   * Sends request to edit a group
   * If succesful, updates the group in the view, otherwise displays error
   * @param {object} group - the group to edit
   */
  editGroup(group) {
    group.error = false;

    if (!group.newTitle) {
      group.error = 'A group must have a title';
      return;
    }

    const updatedGroup = {
      title       : group.newTitle,
      description : group.newDescription
    };

    this.parliamentService.editGroup(group.id, updatedGroup)
      .subscribe(
        (data) => {
          // update group with new values and close form
          group.error = false;
          group.title = group.newTitle;
          group.description = group.newDescription;
          group.showEditGroupForm = false;
        },
        (err) => {
          group.error = err.error.text || 'Unable to udpate this group';
        }
      );
  }

  /**
   * Sends request to delete a group
   * If succesful, removes the group from the view, otherwise displays error
   * @param {object} group - the group to delete
   */
  deleteGroup(group) {
    group.error = false;

    this.parliamentService.deleteGroup(group.id)
      .subscribe(
        (data) => {
          group.error = false;
          let index = 0; // remove the group from the parliament
          for (const g of this.parliament.groups) {
            if (g.title === group.title) {
              this.parliament.groups.splice(index, 1);
              break;
            }
            ++index;
          }
        },
        (err) => {
          group.error = err.error.text || 'Unable to delete this group';
        }
      );
  }

  /**
   * Sends a request to create a new cluster within a group
   * If succesful, updates the group in the view, otherwise displays error
   * @param {object} group - the group to add the cluster
   */
  createNewCluster(group) {
    group.error = '';

    if (!group.newClusterTitle) {
      group.error = 'A cluster must have a title';
      return;
    }
    if (!group.newClusterUrl) {
      group.error = 'A cluster must have a url';
      return;
    }

    const newCluster = {
      title       : group.newClusterTitle,
      description : group.newClusterDescription,
      url         : group.newClusterUrl,
      localUrl    : group.newClusterLocalUrl,
      multiviewer : group.newClusterMultiviewer,
      disabled    : group.newClusterDisabled
    };

    this.parliamentService.createCluster(group.id, newCluster)
      .subscribe(
        (data) => {
          group.error = '';
          group.showNewClusterForm = false;
          group.clusters.push(data.cluster);
          this.updateParliament(data.parliament);
          this.filterClusters();
        },
        (err) => {
          group.error = err.error.text || 'Unable to add a cluster to this group';
        }
      );
  }

  /**
   * Displays form fields to edit a cluster's data
   * Prefills the inputs with the existing cluster's data
   * @param {object} cluster - the cluster to display a form for
   */
  displayEditClusterForm(cluster) {
    cluster.showEditClusterForm = true;
    cluster.newTitle        = cluster.title;
    cluster.newDescription  = cluster.description;
    cluster.newUrl          = cluster.url;
    cluster.newLocalUrl     = cluster.localUrl;
    cluster.newMultiviewer  = cluster.multiviewer;
    cluster.newDisabled     = cluster.disabled;
  }

  /**
   * Sends request to edit a cluster
   * If succesful, updates the cluster in the view, otherwise displays error
   * @param {object} group - the group containing the cluster
   * @param {object} cluster - the cluster to update
   */
  editCluster(group, cluster) {
    cluster.error = false;

    if (!cluster.newTitle) {
      cluster.error = 'A cluster must have a title';
      return;
    }
    if (!cluster.newUrl) {
      cluster.error = 'A cluster must have a url';
      return;
    }

    const updatedCluster = {
      title         : cluster.newTitle,
      description   : cluster.newDescription,
      url           : cluster.newUrl,
      localUrl      : cluster.newLocalUrl,
      multiviewer   : cluster.newMultiviewer,
      disabled      : cluster.newDisabled,
      hideDeltaBPS  : cluster.hideDeltaBPS,
      hideDataNodes : cluster.hideDataNodes,
      hideDeltaTDPS : cluster.hideDeltaTDPS,
      hideTotalNodes: cluster.hideTotalNodes
    };

    this.parliamentService.editCluster(group.id, cluster.id, updatedCluster)
      .subscribe(
        (data) => {
          cluster.error = false;
          cluster.showEditClusterForm = false;
          cluster.title       = cluster.newTitle;
          cluster.description = cluster.newDescription;
          cluster.url         = cluster.newUrl;
          cluster.localUrl    = cluster.newLocalUrl;
          cluster.multiviewer = cluster.newMultiviewer;
          cluster.disabled    = cluster.newDisabled;
        },
        (err) => {
          cluster.error = err.error.text || 'Unable to update this cluster';
        }
      );
  }

  /**
   * Sends a request to delete a cluster within a group
   * If succesful, updates the group in the view, otherwise displays error
   * @param {object} group - the group to remove the cluster from
   * @param {object} cluster - the cluster to remove
   */
  deleteCluster(group, cluster) {
    group.error = '';

    this.parliamentService.deleteCluster(group.id, cluster.id)
      .subscribe(
        (data) => {
          group.error = '';
          let index = 0;
          for (const c of group.clusters) {
            if (c.id === cluster.id) {
              group.clusters.splice(index, 1);
              break;
            }
            ++index;
          }
          this.filterClusters();
        },
        (err) => {
          group.error = err.error.text || 'Unable to remove cluster from this group';
        }
      );
  }

  /**
   * Fired when a cluster is dragged and dropped
   * Saves the new order of the parliament clusters
   * @param {object} event - the dnd event fired on drop
   */
  onDrop($event) {
    // filteredClusters (displayed in the view) are updated by drag and drop
    // but we still need to update all the cluster arrays
    for (const group of this.parliament.groups) {
      group.clusters = group.filteredClusters;
    }

    const parliamentClone = JSON.parse(JSON.stringify(this.parliament));
    this.sanitizeParliament(parliamentClone);

    this.parliamentService.updateClusterOrder(parliamentClone)
      .subscribe(
        (data) => {
          // save the new parliament for when reordering items in the parliament fails
          this.oldParliamentOrder = parliamentClone;
        },
        (err) => {
          // reset the parliament to the old order because of a failure to save it
          this.parliament = JSON.parse(JSON.stringify(this.oldParliamentOrder));
          this.error = err.error.text || 'Unable to move this item';
        }
      );

    this.dragGroups   = true;
    this.dragClusters = true;
    this.startAutoRefresh();
  }

  // Fired when a user starts to drag a cluster
  // Stops autorefresh (so the parliament is not updated)
  // Disables group dragging (https://github.com/akserg/ng2-dnd/issues/20)
  onClusterDragStart() {
    this.dragGroups = false;
    this.stopAutoRefresh();
  }

  // Fired when a user starts to drag a group
  // Stops autorefresh (so the parliament is not updated)
  // Disables cluster dragging (https://github.com/akserg/ng2-dnd/issues/20)
  onGroupDragStart() {
    this.dragClusters = false;
    this.stopAutoRefresh();
  }

  // Fired when an issue is changed within the issue.actions.component
  issueChange($event) {
    if ($event.success) {
      this.error = '';
    } else {
      this.error = $event.message;
    }
  }

  /**
   * Fired when the dismiss all button is clicked
   * Sends a request to remove all the issues within a cluster
   * If succesful, updates the cluster in the view, otherwise displays error
   * @param {object} groupId - the id of the group that the cluster belongs to
   * @param {object} cluster - the cluster to remove issues from
   */
  dismissAll(groupId, cluster) {
    this.parliamentService.dismissAllIssues(groupId, cluster.id)
      .subscribe(
        (data) => {
          for (const group of this.parliament.groups) {
            if (group.id === groupId) {
              for (const c of group.clusters) {
                if (c.id === cluster.id) {
                  if (c.issues) {
                    for (const issue of c.issues) {
                      issue.dismissed = data.dismissed;
                    }
                  }
                }
              }
            }
          }
        },
        (err) => {
          this.error = err.error.text || 'Unable to dismiss all of the issues in this cluster';
        }
      );
  }

}
