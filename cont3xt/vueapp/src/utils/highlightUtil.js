/**
 * Utility functions for highlighting text based on keywords or regex patterns
 *
 * USAGE:
 * Add a 'highlight' query parameter to the URL with comma-separated patterns:
 *
 * Examples:
 *   ?highlight=192.168        - Highlights literal text "192.168"
 *   ?highlight=malware,trojan - Highlights "malware" OR "trojan"
 *   ?highlight=/\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}/gi - Highlights regex pattern (IP addresses)
 *   ?highlight=/ERROR|WARN/gi - Highlights "ERROR" or "WARN" (case-insensitive)
 *   ?highlight=regex:/port \d+/gi - Alternative regex syntax
 *
 * Regex patterns:
 *   - Use /pattern/flags format (flags optional)
 *   - Can also use regex:/pattern/flags format
 *   - Common flags: g (global), i (case-insensitive), m (multiline)
 *
 * Plain keywords:
 *   - Treated as case-insensitive literal matches
 *   - Special regex characters are automatically escaped
 *
 * Multiple patterns are combined with OR logic.
 */

/**
 * Parses highlight patterns from a query string parameter
 * @param {string} highlightParam - The highlight query parameter value
 * @returns {Array<{pattern: string, isRegex: boolean}>} Array of pattern objects
 */
export function parseHighlightPatterns (highlightParam) {
  if (!highlightParam || typeof highlightParam !== 'string') {
    return [];
  }

  const patterns = [];
  const parts = highlightParam.split(',').map(p => p.trim()).filter(p => p);

  for (const part of parts) {
    // Check if it's a regex pattern (format: regex:/pattern/flags or /pattern/flags)
    const regexMatch = part.match(/^(?:regex:)?\/(.+?)\/([gimsuvy]*)$/);
    if (regexMatch) {
      patterns.push({
        pattern: regexMatch[1],
        flags: regexMatch[2] || 'gi',
        isRegex: true
      });
    } else {
      // Plain keyword - will be treated as case-insensitive literal match
      patterns.push({
        pattern: part,
        isRegex: false
      });
    }
  }

  return patterns;
}

/**
 * Finds all matches of patterns in text and returns highlight spans
 * @param {string} text - The text to search in
 * @param {Array<{pattern: string, isRegex: boolean, flags?: string}>} patterns - Array of pattern objects
 * @returns {Array<{start: number, end: number}>} Array of highlight spans, sorted and merged
 */
export function findHighlightSpans (text, patterns) {
  if (!text || !patterns || patterns.length === 0) {
    return null;
  }

  const textStr = text.toString();
  const spans = [];

  for (const { pattern, isRegex, flags } of patterns) {
    try {
      let regex;
      if (isRegex) {
        regex = new RegExp(pattern, flags);
      } else {
        // Escape special regex characters for literal matching
        const escaped = pattern.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
        regex = new RegExp(escaped, 'gi');
      }

      // Find all matches
      let match;
      // Reset lastIndex for global regex
      regex.lastIndex = 0;

      if (regex.global) {
        while ((match = regex.exec(textStr)) !== null) {
          spans.push({ start: match.index, end: match.index + match[0].length });
          // Prevent infinite loop on zero-length matches
          if (match.index === regex.lastIndex) {
            regex.lastIndex++;
          }
        }
      } else {
        // Non-global regex, find first match only
        match = regex.exec(textStr);
        if (match) {
          spans.push({ start: match.index, end: match.index + match[0].length });
        }
      }
    } catch (err) {
      console.warn(`Invalid highlight pattern: ${pattern}`, err);
    }
  }

  if (spans.length === 0) {
    return null;
  }

  // Sort spans by start position
  spans.sort((a, b) => a.start - b.start);

  // Merge overlapping spans
  const merged = [];
  let current = spans[0];

  for (let i = 1; i < spans.length; i++) {
    const next = spans[i];
    if (next.start <= current.end) {
      // Overlapping or adjacent - merge
      current.end = Math.max(current.end, next.end);
    } else {
      // No overlap - add current and move to next
      merged.push(current);
      current = next;
    }
  }
  merged.push(current);

  return merged;
}

/**
 * Applies highlight patterns to a value, returning highlight spans
 * @param {any} value - The value to highlight (will be converted to string)
 * @param {Array<{pattern: string, isRegex: boolean}>} patterns - Array of pattern objects
 * @returns {Array<{start: number, end: number}>|null} Highlight spans or null
 */
export function getHighlightsForValue (value, patterns) {
  if (value == null || !patterns || patterns.length === 0) {
    return null;
  }

  return findHighlightSpans(value, patterns);
}

/**
 * Applies highlight patterns to an array of values
 * @param {Array<any>} values - Array of values to highlight
 * @param {Array<{pattern: string, isRegex: boolean}>} patterns - Array of pattern objects
 * @returns {Array<Array<{start: number, end: number}>>|null} Array of highlight spans arrays
 */
export function getHighlightsForArray (values, patterns) {
  if (!values || !Array.isArray(values) || !patterns || patterns.length === 0) {
    return null;
  }

  const highlights = values.map(value => getHighlightsForValue(value, patterns));

  // Return null if no highlights found in any value
  if (highlights.every(h => h === null)) {
    return null;
  }

  return highlights;
}
