import { Injectable } from '@angular/core';
import { HttpClient, HttpHeaders } from '@angular/common/http';

import { BehaviorSubject } from 'rxjs/BehaviorSubject';
import { Observable } from 'rxjs/Observable';

import { Auth, Login, LoggedIn } from './auth';


@Injectable()
export class AuthService {

  private loggedIn = false;
  private _loggedIn = new BehaviorSubject<boolean>(false);
  loggedIn$ = this._loggedIn.asObservable();

  constructor(private http: HttpClient) {}

  // login the user using the supplied password
  login(password): Observable<Login> {
    return this.http.post<Login>('api/auth', { password: password });
  }

  // save the token in local storage and indicate the user is logged in
  saveToken(token): boolean {
    this.loggedIn = !!token;

    if (!token) { token = ''; }

    localStorage.setItem('token', token);

    // notify page whether the user is logged in
    this._loggedIn.next(this.loggedIn);

    return this.loggedIn;
  }

  // get the token from local storage
  getToken(): string {
    return localStorage.getItem('token') || '';
  }

  // determine whether the user is logged in by checking the jwt with the server
  isLoggedIn(): boolean {
    this.http.get<LoggedIn>('api/auth/loggedin')
      .subscribe(
        (data) => {
          this.loggedIn = data.loggedin;
          this._loggedIn.next(this.loggedIn);
          return this.loggedIn;
        },
        (err) => {
          this.loggedIn = false;
          this._loggedIn.next(this.loggedIn);
          return this.loggedIn;
        }
      );
  }

  // determine whether the server has authentication enabled
  hasAuth(): Observable<Auth> {
    return this.http.get<Auth>('api/auth');
  }

}
