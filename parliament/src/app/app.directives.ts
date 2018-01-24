import { Directive, ElementRef, Input, OnInit } from '@angular/core';

@Directive({
  selector: '[appAutofocus]'
})
export class AutofocusDirective implements OnInit {

  private _autofocus;
  constructor(private el: ElementRef) {}

  ngOnInit() {
    if (this._autofocus || typeof this._autofocus === 'undefined') {
      this.el.nativeElement.focus();
    }
  }

  @Input() set appAutofocus(condition: boolean) {
    this._autofocus = condition !== false;
    if (condition) {
      this.el.nativeElement.focus();
    }
  }

}
