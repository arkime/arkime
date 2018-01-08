import { BrowserModule } from '@angular/platform-browser';
import { BrowserAnimationsModule } from '@angular/platform-browser/animations';
import { HttpClientModule } from '@angular/common/http';
import { HTTP_INTERCEPTORS } from '@angular/common/http';
import { NgModule } from '@angular/core';
import { FormsModule } from '@angular/forms';

import { NgbModule } from '@ng-bootstrap/ng-bootstrap';
import { DndModule } from 'ng2-dnd';

import { ParliamentComponent } from './parliament.component';
import { CommaStringPipe } from './app.pipes';
import { TokenInterceptor } from './token.interceptor';
import { AuthService } from './auth.service';
import { AutofocusDirective } from './directives';


@NgModule({
  declarations: [
    ParliamentComponent,
    CommaStringPipe,
    AutofocusDirective
  ],
  imports     : [
    BrowserModule,
    BrowserAnimationsModule,
    HttpClientModule,
    FormsModule,
    NgbModule.forRoot(),
    DndModule.forRoot()
  ],
  bootstrap   : [ ParliamentComponent ],
  providers   : [
    AuthService,
    {
      provide   : HTTP_INTERCEPTORS,
      useClass  : TokenInterceptor,
      multi     : true
    }
  ]
})
export class AppModule { }
