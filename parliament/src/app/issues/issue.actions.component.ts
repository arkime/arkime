import { Component, Input, Output, OnInit, EventEmitter } from '@angular/core';

import { ParliamentService } from '../parliament/parliament.service';
import { Issue } from './issue';

@Component({
  selector    : 'app-issue-actions',
  templateUrl: './issue.actions.html'
})
export class IssueActionsComponent implements OnInit {

  /* setup ----------------------------------------------------------------- */
  @Input()
  issue: Issue;
  @Input()
  groupId: number;
  @Input()
  clusterId: number;

  @Output()
  change = new EventEmitter();

  constructor(private parliamentService: ParliamentService) {}

  ngOnInit() {
    if (this.groupId === undefined)   { this.groupId = this.issue.groupId; }
    if (this.clusterId === undefined) { this.clusterId = this.issue.clusterId; }
  }

  /* page functions -------------------------------------------------------- */
  /**
   * Sends a request to dismiss an issue
   * If succesful, updates the issue in the view, otherwise displays error
   */
  dismissIssue() {
    this.parliamentService.dismissIssue(this.groupId, this.clusterId, this.issue)
      .subscribe(
        (data) => {
          this.issue.dismissed = data.dismissed;
          this.change.emit({ success: true, issue: this.issue });
        },
        (err) => {
          this.change.emit({ success: false, message: err.error.text || 'Unable to dismiss this issue' });
        }
      );
  }

  /**
   * Sends a request to ignore an issue
   * If succesful, updates the issue in the view, otherwise displays error
   * @param {number} forMs - the amount of time (in ms) that the issue should be ignored
   */
  ignoreIssue(forMs) {
    this.parliamentService.ignoreIssue(this.groupId, this.clusterId, this.issue, forMs)
      .subscribe(
        (data) => {
          this.issue.ignoreUntil = data.ignoreUntil;
          this.change.emit({ success: true, issue: this.issue });
        },
        (err) => {
          this.change.emit({ success: false, message: err.error.text || 'Unable to ignore this issue' });
        }
      );
  }

  /**
   * Sends a request to remove an ignore for an issue
   * If succesful, updates the issue in the view, otherwise displays error
   */
  removeIgnore() {
    this.parliamentService.removeIgnoreIssue(this.groupId, this.clusterId, this.issue)
      .subscribe(
        (data) => {
          this.issue.ignoreUntil = undefined;
          this.change.emit({ success: true, issue: this.issue });
        },
        (err) => {
          this.change.emit({ success: false, message: err.error.text || 'Unable to remove ignore for this issue' });
        }
      );
  }

}
