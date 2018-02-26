import { Injectable } from '@angular/core';
import { HttpClient, HttpHeaders } from '@angular/common/http';

import { Observable } from 'rxjs/Observable';

import { Settings } from './settings';

@Injectable()
export class SettingsService {

  constructor(private http: HttpClient) {}

  getSettings(): Observable<Settings> {
    return this.http.get<Settings>('api/settings');
  }

  saveSettings(settings): Observable<any> {
    return this.http.put<any>(`api/settings`, { settings: settings });
  }

  testNotifier(notifierName): Observable<any> {
    return this.http.post<any>(`api/testAlert`, { notifier: notifierName });
  }

}
