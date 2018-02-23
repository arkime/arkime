import { Component, OnInit } from '@angular/core';

import { ParliamentService } from './parliament/parliament.service';
import { AuthService } from './auth/auth.service';
import { Auth, Login } from './auth/auth';

@Component({
  selector    : 'app-root',
  styleUrls   : [ './app.css' ],
  templateUrl : './app.html'
})
export class AppComponent implements OnInit {

  /* setup ----------------------------------------------------------------- */
  // user auth flag
  auth: Auth = { hasAuth: false };

  // password input variables
  password = '';
  showLoginInput = false;
  focusOnPasswordInput = false;

  // data refresh interval default
  refreshInterval = '15000';

  // display error messages
  error = '';

  constructor(
    private parliamentService: ParliamentService,
    public authService: AuthService
  ) {
    parliamentService.refreshInterval$.subscribe((interval) => {
      this.refreshInterval = interval;
    });
  }

  ngOnInit() {
    this.authService.hasAuth()
      .subscribe((response) => {
        this.auth.hasAuth = response.hasAuth;
      });

    this.authService.isLoggedIn();
  }

  /* page functions -------------------------------------------------------- */
  login() {
    this.showLoginInput = !this.showLoginInput;
    this.focusOnPasswordInput = this.showLoginInput;

    if (!this.showLoginInput) {
      if (!this.password) {
        this.error = 'Must provide a password to login.';
        return;
      }

      this.authService.login(this.password)
        .then((response) => {
          this.error    = '';
          this.password = '';
        })
        .catch((err) => {
          this.password = '';
          this.error    = err.error.text || 'Unable to login';
        });
    }
  }

  cancelLogin() {
    this.showLoginInput       = false;
    this.focusOnPasswordInput = false;
    this.error                = '';
  }

  logout() {
    this.authService.logout();
  }

  // Fired when interval refresh select input is changed
  changeRefreshInterval() {
    this.parliamentService.setRefreshInterval(this.refreshInterval);
  }

}
