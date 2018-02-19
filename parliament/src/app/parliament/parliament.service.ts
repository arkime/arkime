import { Injectable } from '@angular/core';
import { HttpClient, HttpHeaders } from '@angular/common/http';

import { Observable } from 'rxjs/Observable';
import { BehaviorSubject } from 'rxjs/BehaviorSubject';

import { Parliament, GroupCreated, ClusterCreated, Response, Issues } from './parliament';

@Injectable()
export class ParliamentService {

  refreshInterval = new BehaviorSubject<string>('15000');
  refreshInterval$ = this.refreshInterval.asObservable();

  constructor(private http: HttpClient) {
    const interval = localStorage.getItem('refreshInterval');
    if (interval !== null) {
      this.setRefreshInterval(interval);
    }
  }

  setRefreshInterval(interval: string): void {
    this.refreshInterval.next(interval);
    localStorage.setItem('refreshInterval', interval);
  }

  getParliament(): Observable<Parliament> {
    return this.http.get<Parliament>('api/parliament');
  }

  createGroup(group): Observable<GroupCreated> {
    return this.http.post<GroupCreated>('api/groups', group);
  }

  editGroup(id, group): Observable<Response> {
    return this.http.put<Response>(`api/groups/${id}`, group);
  }

  deleteGroup(id): Observable<Response> {
    return this.http.delete<Response>(`api/groups/${id}`);
  }

  createCluster(id, cluster): Observable<ClusterCreated> {
    return this.http.post<ClusterCreated>(`api/groups/${id}/clusters`, cluster);
  }

  editCluster(groupId, clusterId, cluster): Observable<Response> {
    return this.http.put<Response>(`api/groups/${groupId}/clusters/${clusterId}`, cluster);
  }

  deleteCluster(groupId, clusterId): Observable<Response> {
    return this.http.delete<Response>(`api/groups/${groupId}/clusters/${clusterId}`);
  }

  updateClusterOrder(reorderedParliament): Observable<Response> {
    return this.http.put<Response>(`api/parliament`, { reorderedParliament: reorderedParliament });
  }

  getIssues(query): Observable<Issues> {
    return this.http.get<Issues>(`api/issues`, { params: query });
  }

  dismissIssue(groupId, clusterId, issue): Observable<any> {
    return this.http.put<Response>(
      `api/groups/${groupId}/clusters/${clusterId}/dismissIssue`,
      { type: issue.type, node: issue.node }
    );
  }

  ignoreIssue(groupId, clusterId, issue, forMs): Observable<any> {
    return this.http.put<Response>(
      `api/groups/${groupId}/clusters/${clusterId}/ignoreIssue`,
      { type: issue.type, node: issue.node, ms: forMs }
    );
  }

  removeIgnoreIssue(groupId, clusterId, issue): Observable<any> {
    return this.http.put<Response>(
      `api/groups/${groupId}/clusters/${clusterId}/removeIgnoreIssue`,
      { type: issue.type, node: issue.node }
    );
  }

  dismissAllIssues(groupId, clusterId): Observable<any> {
    return this.http.put<Response>(
      `api/groups/${groupId}/clusters/${clusterId}/dismissAllIssues`, {}
    );
  }
}
