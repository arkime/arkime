import { Component, OnInit, OnDestroy } from '@angular/core';

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
  // data refresh interval default
  private refreshInterval = '15000';
  // whether the issue data has been initialized
  private initialized = false;

  error = '';
  issues = [];

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
  loadData() {
    this.parliamentService.getIssues()
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
    if ($event.success) {
      this.error = '';
    } else {
      this.error = $event.message;
    }
  }

}
