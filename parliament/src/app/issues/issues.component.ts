import { Component, OnInit, OnDestroy } from '@angular/core';
import { Router, ActivatedRoute, ParamMap } from '@angular/router';

import { TimerObservable } from 'rxjs/observable/TimerObservable';

import { ParliamentService } from '../parliament/parliament.service';
import { AuthService } from '../auth/auth.service';

@Component({
  templateUrl: './issues.html'
})
export class IssuesComponent implements OnInit, OnDestroy {

  /* setup ----------------------------------------------------------------- */
  // subscriber for timer that issues requests for issues
  private timerSubscriber;
  // subscriber for the refresh interval variable
  private refreshIntervalSubscriber;
  // subscriber for url query parameters
  private queryParamsSubscriber;
  // data refresh interval default
  private refreshInterval = '15000';
  // whether the issue data has been initialized
  private initialized = false;

  // display error messages
  error = '';

  // page data
  issues = [];

  // query paramters
  order = 'desc';
  sort = '';

  constructor(
    private router: Router,
    private route: ActivatedRoute,
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
    // apply route parameters to the initial query
    this.queryParamsSubscriber = this.route.queryParams.subscribe(
      (params) => {
        if (params.sort)  { this.sort   = params.sort; }
        if (params.order) { this.order  = params.order; }

        this.loadData();
      }
    );
  }

  ngOnDestroy() {
    this.stopAutoRefresh();
    this.queryParamsSubscriber.unsubscribe();
    this.refreshIntervalSubscriber.unsubscribe();
  }

  /* controller functions -------------------------------------------------- */
  loadData() {
    let query = {}; // set up query parameters (order and sort)
    if (this.sort) { query = { order: this.order, sort: this.sort }; }

    this.parliamentService.getIssues(query)
      .subscribe(
        (response) => {
          this.error = '';
          this.issues = response.issues;
        },
        (err) => {
          this.error = err.error.text || 'Error fetching issues. The issues below are likely out of date';
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

  /* page functions -------------------------------------------------------- */
  getIssueTrackingId(index, issue) {
    let id = `${issue.groupId}-${issue.clusterId}`;
    if (issue.node) { id += `-${issue.node.replace(/\s/g, '')}`; }
    id += `-${issue.type}`;
  }

  getIssueRowClass(issue) {
    if (issue.ignoreUntil) {
      return 'table-secondary text-muted';
    } else if (issue.severity === 'red') {
      return 'table-danger';
    } else if (issue.severity === 'yellow') {
      return 'table-warning';
    }

    return '';
  }

  // Fired when an issue is changed within the issue.actions.component
  issueChange($event) {
    this.error = $event.success ? '' : $event.message;

    if (!this.error) {
      // update the list of issues
      for (let [index, issue] of this.issues.entries()) {
        if (issue.type === $event.issue.type && issue.clusterId === $event.issue.clusterId &&
          issue.groupId === $event.issue.groupId && issue.node === $event.issue.node) {
          if ($event.issue.dismissed) { // remove dismissed issues
            this.issues.splice(index, 1);
          } else { // update ignored issues
            issue = $event.issue;
          }
        }
      }
    }
  }

  // Fired when a sortable column is clicked
  sortBy(property) {
    if (this.sort === property) { // same sort field, so toggle order direction
      this.order  = this.order === 'asc' ? 'desc' : 'asc';
    } else { // new sort field, so set default order (desc)
      this.sort   = property;
      this.order  = 'desc';
    }

    // update url with sort and order parameters
    // the query params change triggers queryParamsSubscriber and loads data
    const params = { 'sort': this.sort, 'order': this.order };
    this.router.navigate(['/issues'], { queryParams: params });
  }

}
