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
  passwordConfirm = '';
  updatingPassword = false;
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
        .subscribe(
          (data) => {
            this.error    = '';
            this.password = '';
          },
          (err) => {
            this.password = '';
            this.error    = err.error.text || 'Unable to login';
          }
        );
    }
  }

  cancelLogin() {
    this.showLoginInput       = false;
    this.focusOnPasswordInput = false;
    this.updatingPassword     = false;
    this.passwordConfirm      = '';
    this.password             = '';
    this.error                = '';
  }

  logout() {
    this.authService.logout();
    localStorage.setItem('token', ''); // clear token
  }

  updatePassword() {
    this.updatingPassword     = !this.updatingPassword;
    this.showLoginInput       = !this.showLoginInput;
    this.focusOnPasswordInput = this.showLoginInput;

    if (!this.showLoginInput) {
      if (!this.password) {
        this.error = 'Must provide a password.';
        return;
      }

      if (!this.passwordConfirm) {
        this.error = 'Must confirm your password.';
        return;
      }

      if (this.password !== this.passwordConfirm) {
        this.error = 'Passwords must match.';
        this.password = '';
        this.passwordConfirm = '';
        return;
      }

      this.authService.updatePassword(this.password)
        .subscribe(
          (data) => {
            this.error    = '';
            this.password = '';
            this.auth.hasAuth = true;
            this.authService.saveToken(data.token);
          },
          (err) => {
            console.error('update password error:', err);
            this.password = '';
            this.error    = err.error.text || 'Unable to update your password.';
            this.authService.saveToken('');
          }
        );
    }
  }

  // Fired when the user clicks enter on the password input
  passwordInputSubmit() {
    if (this.updatingPassword) {
      this.updatePassword();
    } else { this.login(); }
  }

  // Fired when interval refresh select input is changed
  changeRefreshInterval() {
    this.parliamentService.setRefreshInterval(this.refreshInterval);
  }

}
