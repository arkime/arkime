import { BrowserModule } from '@angular/platform-browser';
import { BrowserAnimationsModule } from '@angular/platform-browser/animations';
import { HttpClientModule } from '@angular/common/http';
import { HTTP_INTERCEPTORS } from '@angular/common/http';
import { NgModule } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { RouterModule, Routes } from '@angular/router';

import { NgbModule } from '@ng-bootstrap/ng-bootstrap';
import { DndModule } from 'ng2-dnd';

import { AppComponent } from './app.component';
import { ParliamentComponent } from './parliament.component';
import { IssuesComponent } from './issues.component';
import { CommaStringPipe, IssueValuePipe } from './app.pipes';
import { TokenInterceptor } from './token.interceptor';
import { AuthService } from './auth.service';
import { ParliamentService } from './parliament.service';
import { AutofocusDirective } from './directives';


const appRoutes: Routes = [
  { path: 'issues', component: IssuesComponent },
  { path: '', component: ParliamentComponent }
];


@NgModule({
  declarations: [
    AppComponent,
    ParliamentComponent,
    IssuesComponent,
    CommaStringPipe,
    IssueValuePipe,
    AutofocusDirective
  ],
  imports     : [
    BrowserModule,
    BrowserAnimationsModule,
    HttpClientModule,
    FormsModule,
    NgbModule.forRoot(),
    DndModule.forRoot(),
    RouterModule.forRoot(appRoutes)
  ],
  exports     : [ RouterModule ],
  bootstrap   : [ AppComponent ],
  providers   : [
    AuthService,
    ParliamentService,
    {
      provide   : HTTP_INTERCEPTORS,
      useClass  : TokenInterceptor,
      multi     : true
    }
  ]
})
export class AppModule { }
