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
import { ParliamentComponent } from './parliament/parliament.component';
import { ParliamentService } from './parliament/parliament.service';
import { IssuesComponent } from './issues/issues.component';
import { IssueActionsComponent } from './issues/issue.actions.component';
import { SettingsComponent } from './settings/settings.component';
import { CommaStringPipe, IssueValuePipe } from './app.pipes';
import { TokenInterceptor } from './auth/token.interceptor';
import { AuthService } from './auth/auth.service';
import { AutofocusDirective } from './app.directives';


const appRoutes: Routes = [
  { path: 'settings', component: SettingsComponent },
  { path: 'issues', component: IssuesComponent },
  { path: '', component: ParliamentComponent }
];


@NgModule({
  declarations: [
    AppComponent,
    ParliamentComponent,
    IssuesComponent,
    IssueActionsComponent,
    SettingsComponent,
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
