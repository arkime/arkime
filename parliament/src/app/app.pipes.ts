import { Pipe, PipeTransform } from '@angular/core';

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
