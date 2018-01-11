import { Pipe, PipeTransform } from '@angular/core';
import { DatePipe } from '@angular/common';

/**
 * http://stackoverflow.com/questions/2901102
 * separate number with commas
 * @param {number} input the number to add commas to
 * @returns {string} number string with appropriate commas
 */
@Pipe({name: 'commaString'})
export class CommaStringPipe implements PipeTransform {
  transform(input: number, fallback = '0'): string {
    if (isNaN(input)) { return fallback; }
    return input.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ',');
  }
}

@Pipe({name: 'issueValue'})
export class IssueValuePipe implements PipeTransform {
  transform(input: any, type: string): string {
    let result = input;

    if (input === undefined) { return ''; }

    if (type === 'esDropped') {
      result = new CommaStringPipe().transform(input);
    } else if (type === 'outOfDate') {
      result = new DatePipe('en-US').transform(input, 'yyyy/MM/dd HH:mm:ss');
    }

    return result;
  }
}
