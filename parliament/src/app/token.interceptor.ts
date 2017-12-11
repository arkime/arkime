import { Injectable, Injector } from '@angular/core';
import {
  HttpRequest,
  HttpResponse,
  HttpHandler,
  HttpEvent,
  HttpInterceptor,
  HttpErrorResponse
} from '@angular/common/http';

import { Observable } from 'rxjs/Observable';
import 'rxjs/add/operator/do';

import { AuthService } from './auth.service';


@Injectable()
export class TokenInterceptor implements HttpInterceptor {

  private authService: AuthService;

  constructor(private inj: Injector) {}

  intercept(request: HttpRequest<any>, next: HttpHandler): Observable<HttpEvent<any>> {

    // Cyclic dependency error with HttpInterceptor
    // https://github.com/angular/angular/issues/18224
    const authService = this.inj.get(AuthService);

    request = request.clone({
      setHeaders: { 'x-access-token': authService.getToken() }
    });

    return next.handle(request).do((event: HttpEvent<any>) => {},
      (err: any) => {
        if (err.error && !err.error.success && err.error.tokenError) {
          authService.saveToken(''); // user no longer has access
        }
    });

  }

}
