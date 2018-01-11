import { Component, OnInit, OnDestroy } from '@angular/core';

import { TimerObservable } from 'rxjs/observable/TimerObservable';

import { ParliamentService } from './parliament.service';
import { AuthService } from './auth.service';

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

  /**
   * Sends a request to dismiss an issue
   * If succesful, updates the issue in the view, otherwise displays error
   * @param {object} issue - the issue to be dismissed
   */
  dismissIssue(issue) {
    this.parliamentService.dismissIssue(issue.groupId, issue.clusterId, issue)
      .subscribe(
        (data) => {
          this.error = '';
          issue.dismissed = data.dismissed;
        },
        (err) => {
          this.error = err.error.text || 'Unable to dismiss this issue';
        }
      );
  }

  /**
   * Sends a request to ignore an issue
   * If succesful, updates the issue in the view, otherwise displays error
   * @param {object} issue - the issue to be ignored
   * @param {number} forMs - the amount of time (in ms) that the issue should be ignored
   */
  ignoreIssue(issue, forMs) {
    this.parliamentService.ignoreIssue(issue.groupId, issue.clusterId, issue, forMs)
      .subscribe(
        (data) => {
          this.error = '';
          issue.ignoreUntil = data.ignoreUntil;
        },
        (err) => {
          this.error = err.error.text || 'Unable to ignore this issue';
        }
      );
  }

  /**
   * Sends a request to remove an ignore for an issue
   * If succesful, updates the issue in the view, otherwise displays error
   * @param {object} issue - the issue to remove the ignore for
   */
  removeIgnore(issue) {
    this.parliamentService.removeIgnoreIssue(issue.groupId, issue.clusterId, issue)
      .subscribe(
        (data) => {
          this.error = '';
          issue.ignoreUntil = undefined;
        },
        (err) => {
          this.error = err.error.text || 'Unable to ignore this issue';
        }
      );
  }

}
