import { Injectable } from '@angular/core';
import { HttpClient, HttpHeaders } from '@angular/common/http';

import { Observable } from 'rxjs/Observable';
import { BehaviorSubject } from 'rxjs/BehaviorSubject';

import { Auth, Login, LoggedIn } from './auth';


@Injectable()
export class AuthService {

  loggedIn  = false;
  _loggedIn = new BehaviorSubject<boolean>(false);
  loggedIn$ = this._loggedIn.asObservable();


  constructor(private http: HttpClient) {}

  // login the user using the supplied password
  login(password): Promise<Login> {
    return new Promise<Login>((resolve, reject) => {
      this.http.post<Login>('api/auth', { password: password })
        .subscribe(
          (response) => {
            this.saveToken(response.token);
            resolve(response);
          },
          (error) => {
            this.saveToken('');
            reject(error);
          }
        );
    });
  }

  // logout the user by saving a blank token
  logout(): void {
    this.saveToken('');
  }

  // save the token in local storage and indicate if the user is logged in
  saveToken(token): boolean {
    localStorage.setItem('token', token);

    this.loggedIn = !!token;

    this._loggedIn.next(this.loggedIn);

    if (!token) { token = ''; }

    return this.loggedIn;
  }

  // get the token from local storage
  getToken(): string {
    return localStorage.getItem('token') || '';
  }

  // determine whether the user is logged in by checking the jwt with the server
  isLoggedIn(): any {
    this.http.get<LoggedIn>('api/auth/loggedin')
      .subscribe(
        (data) => {
          this.loggedIn = data.loggedin;
          this._loggedIn.next(data.loggedin);
          return this.loggedIn;
        },
        (err) => {
          this.loggedIn = false;
          this._loggedIn.next(false);
          return false;
        }
      );
  }

  // determine whether the server has authentication enabled
  hasAuth(): Observable<Auth> {
    return this.http.get<Auth>('api/auth');
  }

  // update (or create) a password
  updatePassword(currentPassword, newPassword): Promise<Login> {
    return new Promise<Login>((resolve, reject) => {
      this.http.put<Login>('api/auth/update', {
        newPassword: newPassword,
        currentPassword: currentPassword
      }).subscribe(
        (response) => {
          this.saveToken(response.token);
          resolve(response);
        },
        (error) => {
          this.saveToken('');
          reject(error);
        }
      );
    });
  }

}
