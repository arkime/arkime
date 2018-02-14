import { Component, OnInit, OnDestroy } from '@angular/core';

import { AuthService } from '../auth/auth.service';
import { SettingsService } from './settings.service';

@Component({
  templateUrl: './settings.html',
  providers  : [ SettingsService ]
})
export class SettingsComponent implements OnInit {

  /* setup ----------------------------------------------------------------- */
  // subscriber for the logged in variable
  private loggedInSubscriber;

  // display error messages
  error = '';

  // page data
  settings = {};

  constructor(
    public authService: AuthService,
    private settingsService: SettingsService
  ) {
    // listen for logged in var
    this.loggedInSubscriber = authService.loggedIn$.subscribe(
      (isLoggedIn) => {
        if (isLoggedIn) { // update the view with data if user is logged in
          this.error = '';
          if (!Object.keys(this.settings).length) {
            this.loadData();
          }
        } else { // otherwise clear the settings and show an error
          this.settings = {};
          if (!this.error) {
            this.error = 'This page requires admin privileges. Please login.';
          }
        }
      }
    );
  }

  ngOnInit() {
    this.loadData();
  }

  /* controller functions -------------------------------------------------- */
  loadData() {
    this.settingsService.getSettings()
      .subscribe(
        (response) => {
          this.error = '';
          this.settings = response;
        },
        (err) => {
          this.error = err.error.text || 'Error fetching settings.';
        }
      );
  }

  /* page functions -------------------------------------------------------- */
  trackByName(index, item) {
    return item.name;
  }

  saveSettings(notifier) {
    this.settingsService.saveSettings(this.settings)
      .subscribe(
        (response) => {
          this.error = '';
          notifier.changed = false;
        },
        (err) => {
          this.error = err.error.text || 'Error saving settings.';
        }
      );
  }

  toggleNotifier(notifier) {
    notifier.on = !notifier.on;
    this.saveSettings(notifier);
  }

}
