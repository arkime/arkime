import { Injectable } from '@angular/core';
import { HttpClient, HttpHeaders } from '@angular/common/http';

import { Observable } from 'rxjs/Observable';

import { Parliament, GroupCreated, ClusterCreated, Response } from './parliament';

@Injectable()
export class ParliamentService {

  constructor(private http: HttpClient) {}

  getParliament(): Observable<Parliament> {
    return this.http.get<Parliament>('parliament/api/parliament');
  }

  createGroup(group): Observable<GroupCreated> {
    return this.http.post<GroupCreated>('parliament/api/groups', group);
  }

  editGroup(id, group): Observable<Response> {
    return this.http.put<Response>(`parliament/api/groups/${id}`, group);
  }

  deleteGroup(id): Observable<Response> {
    return this.http.delete<Response>(`parliament/api/groups/${id}`);
  }

  createCluster(id, cluster): Observable<ClusterCreated> {
    return this.http.post<ClusterCreated>(`parliament/api/groups/${id}/clusters`, cluster);
  }

  editCluster(groupId, clusterId, cluster): Observable<Response> {
    return this.http.put<Response>(`parliament/api/groups/${groupId}/clusters/${clusterId}`, cluster);
  }

  deleteCluster(groupId, clusterId): Observable<Response> {
    return this.http.delete<Response>(`parliament/api/groups/${groupId}/clusters/${clusterId}`);
  }

  updateClusterOrder(reorderedParliament): Observable<Response> {
    return this.http.put<Response>(`parliament/api/parliament`, { reorderedParliament: reorderedParliament });
  }
}
