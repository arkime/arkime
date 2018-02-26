import { Component, OnInit, OnDestroy } from '@angular/core';

import { AuthService } from '../auth/auth.service';
import { SettingsService } from './settings.service';
import { Auth } from '../auth/auth';

@Component({
  templateUrl: './settings.html',
  providers  : [ SettingsService ]
})
export class SettingsComponent implements OnInit {

  /* setup ----------------------------------------------------------------- */
  // subscriber for the logged in variable
  private loggedInSubscriber;

  // user auth flag
  auth: Auth = { hasAuth: false };

  // display error messages
  error = '';

  // page data
  settings = {};
  currentPassword = '';
  newPassword = '';
  newPasswordConfirm = '';
  passwordChanged = false;

  // whether settings have been updated by the user
  changed = false;

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
        } else { // otherwise clear the settings
          this.settings = {};
        }
      }
    );
  }

  ngOnInit() {
    this.authService.hasAuth()
      .subscribe((response) => {
        this.auth.hasAuth = response.hasAuth;
      });
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

  saveSettings() {
    this.settingsService.saveSettings(this.settings)
      .subscribe(
        (response) => {
          this.error = '';
          this.changed = false;
        },
        (err) => {
          this.error = err.error.text || 'Error saving settings.';
        }
      );
  }

  toggleNotifier(notifier) {
    notifier.on = !notifier.on;
    this.saveSettings();
  }

  testNotifier(notifier) {
    this.settingsService.testNotifier(notifier.name)
      .subscribe(
        (response) => {
          this.error = '';
        },
        (err) => {
          this.error = err.error.text || 'Error issuing alert.';
        }
      );
  }

  clearNotifierFields(notifier) {
    for (const field of notifier.fields) {
      field.value = '';
    }

    this.changed = true;
  }

  getFieldInputType(field) {
    return (field.secret && !field.showValue) ? 'password' : 'text';
  }

  cancelChangePassword() {
    this.currentPassword = '';
    this.newPassword = '';
    this.newPasswordConfirm = '';
    this.passwordChanged = false;
  }

  updatePassword() {
    if (!this.currentPassword && this.auth.hasAuth) {
      this.error = 'You must provide your current password.';
    }

    if (!this.newPassword) {
      this.error = 'You must provide a new password.';
      return;
    }

    if (!this.newPasswordConfirm) {
      this.error = 'You must confirm your new password.';
      return;
    }

    if (this.newPassword !== this.newPasswordConfirm) {
      this.error = 'Passwords must match.';
      this.newPassword = '';
      this.newPasswordConfirm = '';
      return;
    }

    this.authService.updatePassword(this.currentPassword, this.newPassword)
      .then((response) => {
        this.error = '';
        this.cancelChangePassword();
        this.auth.hasAuth = true;
      })
      .catch((err) => {
        this.error = err.error.text || 'Error saving password.';
        this.cancelChangePassword();
      });
  }

}
