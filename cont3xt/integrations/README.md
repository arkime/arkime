# Creating Cont3xt Integrations

This guide walks you through creating a new integration for Cont3xt. We'll use real examples from the codebase to demonstrate best practices.

## Table of Contents
- [Overview](#overview)
- [File Structure](#file-structure)
- [Basic Integration Structure](#basic-integration-structure)
- [Integration Properties](#integration-properties)
- [Card Configuration](#card-configuration)
- [Tidbit Configuration](#tidbit-configuration)
- [Data Fetching](#data-fetching)
- [Error Handling](#error-handling)
- [Authentication](#authentication)
- [Testing](#testing)

## Overview

A Cont3xt integration connects to external data sources (APIs, databases, services) to enrich indicators like domains, IPs, emails, hashes, etc. Each integration:

1. Queries an external service
2. Transforms the response data
3. Displays results in cards and tidbits

See [descriptions.txt](../descriptions.txt) for detailed reference on card fields, tidbits, and post-processors.

## File Structure

Create a new directory under `cont3xt/integrations/` with the following structure:

```
cont3xt/integrations/yourservice/
├── index.js          # Main integration code
└── icon.png          # 200x200 icon (optional but recommended)
```

## Basic Integration Structure

Every integration extends the `Integration` base class and follows this pattern:

```javascript
/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class YourServiceIntegration extends Integration {
  name = 'Your Service Name';                  // Display name
  icon = 'integrations/yourservice/icon.png';  // Path to icon
  order = 100;                          // Display order (lower = earlier)
  itypes = {                            // Indicator types this integration handles
    domain: 'fetchDomain',              // Map itype to fetch method (only define this for the iTypes applicable to this service)
    url: 'fetchUrl',
    ip: 'fetchIp',
    hash: 'fetchHash',
    text: 'fetchText',
    phone: 'fetchPhone',
    email: 'fetchEmail'
  };

  homePage = 'https://yourservice.com/';  // Service homepage (optional)

  settings = {
    username: {
      help: 'Your Service Username',
      required: true
    }
    // Add more settings as needed (API keys, etc.)
  };

  card = {
    // Card configuration (see Card Configuration section)
  };

  tidbits = {
    // Tidbit configuration (see Tidbit Configuration section)
  };

  cacheTimeout = 24 * 60 * 60 * 1000;   // Cache results (milliseconds)

  constructor () {
    super();
    Integration.register(this);
  }

  async fetchDomain (user, domain) {
    // Fetch and return data (see Data Fetching section)
  }

  async fetchIp (user, ip) {
    // Fetch and return data (see Data Fetching section)
  }

  // other fetch functions for every applicable iType
}

new YourServiceIntegration();
```

## Integration Properties

### Required Properties

- **`name`**: Display name shown in the UI
- **`itypes`**: Object mapping indicator types to fetch methods
  - Supported iTypes: `domain`, `ip`, `email`, `phone`, `hash`, `url`, `text`
  - Value is the method name to call (e.g., `'fetchDomain'`)

### Optional Properties

- **`icon`**: Path to icon file (makes integration visible as a button)
- **`order`**: Number controlling display order (lower numbers appear first)
- **`homePage`**: URL to the service's homepage
- **`settings`**: User configuration options (see Authentication section)
- **`card`**: How to display detailed results (see Card Configuration)
- **`tidbits`**: Small bits of info shown in the indicator result tree
- **`cacheTimeout`**: How long to cache results in milliseconds
- **`configName`**: Use settings from another integration (for shared credentials)

### Cache Timeout Guidelines

```javascript
// Standard: 24 hours (most services)
cacheTimeout = 24 * 60 * 60 * 1000;

// Extended: 1 week (for flaky, slow, expensive, or rate-limited services)
cacheTimeout = 7 * 24 * 60 * 60 * 1000;

// Short: 1 hour (for rapidly changing data)
cacheTimeout = 60 * 60 * 1000;
```

## Card Configuration

Cards display detailed integration results. See [descriptions.txt](../descriptions.txt) for complete field type reference.

### Basic Card Example

```javascript
card = {
  title: 'Service Name for %{query}',    // %{query} replaced with indicator
  searchUrls: [{
    url: 'https://service.com/search/%{query}',
    itypes: ['domain', 'ip'],
    name: 'Search Service for %{query}'
  }],
  fields: [
    'simpleField',                       // String field (label = field name)
    {
      label: 'Custom Label',             // Display name
      field: 'nested.path.to.data'       // Dot notation for nested data
    },
    {
      label: 'Date Field',
      field: 'created_at',
      type: 'date'                       // Format as date
    },
    {
      label: 'Array Field',
      field: 'tags',
      type: 'array',
      join: ', '                         // Join array with comma
    },
    {
      label: 'Table Field',
      field: 'items',                    // Path to array
      type: 'table',
      fields: [                          // Columns in table
        {
          label: 'Name',
          field: 'name'
        },
        {
          label: 'Value',
          field: 'value'
        }
      ]
    }
  ]
};
```

### Field Types

- **`string`**: Default, plain text
- **`date`**: Formats date/timestamp
- **`array`**: Displays array items (use `join` to show on one line)
- **`table`**: Displays array of objects as a table
- **`url`**: Makes the value clickable
- **`json`**: Shows raw JSON with formatting
- **`ms`**: Millisecond timestamp
- **`seconds`**: Second timestamp

### Advanced Field Options

```javascript
{
  label: 'Complex Field',
  field: 'path.to.data',
  type: 'string',
  defang: true,                          // Change http->hXXp, .->[-]
  pivot: true,                           // Add pivot dropdown menu
  defaultSortField: 'name',              // For tables, default sort column
  defaultSortDirection: 'asc',           // Sort direction
  fieldRoot: 'nested.path',              // For tables, extract nested objects
  filterEmpty: true                      // Remove empty rows (default: true)
}
```

## Tidbit Configuration

Tidbits are small pieces of information shown in the Indicator Result Tree (IRT). See [descriptions.txt](../descriptions.txt) for complete reference.

### Basic Tidbit Example

```javascript
tidbits = {
  order: 100,                            // Default order for all fields
  fields: [
    {
      field: 'count',
      tooltip: 'total results'           // Hover text
    },
    {
      field: 'registration.created',
      postProcess: 'removeTime',         // Strip time from date
      tooltipTemplate: '<value>',        // Template for tooltip
      purpose: 'registered',             // Tidbit purpose
      precedence: 2,                     // Higher = preferred when duplicates
      order: 100                         // Sort order
    },
    {
      field: 'link',
      display: 'cont3xtCopyLink'         // Make it a copyable link
    }
  ]
};
```

### Tidbit Display Types

- **`badge`**: Default, simple badge
- **`cont3xtField`**: Clickable with copy/pivot options
- **`cont3xtCopyLink`**: Copyable link
- **`primaryGroup`**, **`successGroup`**, etc.: Colored group badges for arrays

### Purpose and Precedence

When multiple integrations provide the same type of information (e.g., "registered date"), use `purpose` and `precedence` to control which one is displayed:

```javascript
{
  field: 'createdDate',
  purpose: 'registered',      // Identifies this as registration date
  precedence: 1               // Lower = less preferred (1-3 typical)
}
```

## Data Fetching

### Basic Fetch Method

```javascript
async fetchDomain (user, domain) {
  try {
    // NOTE: authentication options depend on your service
    // check your service's API documentation for details and the Authentication section for examples
    const result = await axios.get(`https://api.service.com/domain/${domain}`, {
      // or it might be sent in the parameters and not the url
      params: {
          domain
      },
      headers: {
        Accept: 'application/json',     // or whatever content type the service supports
        'User-Agent': this.userAgent()  // Always include user agent
      }
    });

    // Return data with _cont3xt count
    result.data._cont3xt = { count: 1 };
    return result.data;
  } catch (err) {
    // Suppress 404s in non-debug mode
    if (Integration.debug <= 1 && err?.response?.status === 404) {
      return null;
    }
    console.log(this.name, domain, err);
    return null;
  }
}
```

### Return Values

- **`undefined`**: No data available (not an error)
- **`null`**: Error occurred
- **`object`**: Success, must include `_cont3xt: { count: N }` property

### Working with API Responses

```javascript
// Simple response - return as-is
result.data._cont3xt = { count: 1 };
return result.data;

// Array response - wrap it
const data = {
  items: result.data,
  count: result.data.length,
  _cont3xt: { count: result.data.length }
};
return data;

// Add convenience fields
result.data.link = result.data.links?.[0]?.value;
result.data._cont3xt = { count: 1 };
return result.data;
```

## Error Handling

### Standard Pattern

```javascript
async fetchDomain (user, domain) {
  try {
    const result = await axios.get(url, options);

    if (!result.data) {
      return undefined;                   // No data
    }

    result.data._cont3xt = { count: 1 };
    return result.data;
  } catch (err) {
    // Don't log 404s unless debugging
    if (Integration.debug <= 1 && err?.response?.status === 404) {
      return null;
    }

    // Log detailed API errors
    if (err?.response?.data) {
      console.log(this.name, domain, 'Error:', err.response.status,
                  JSON.stringify(err.response.data));
    } else {
      console.log(this.name, domain, err);
    }
    return null;
  }
}
```

### HTTP Options

```javascript
const result = await axios.get(url, {
  maxRedirects: 5,              // Follow redirects
  validateStatus: false,        // Don't throw on non-2xx
  headers: {
    'Accept': 'application/json',
    'User-Agent': this.userAgent()
  }
});

// Check status manually
if (result.status === 200) {
  // Process data
} else {
  return null;
}
```

## Authentication

### API Key in Settings

```javascript
settings = {
  disabled: {
    help: 'Disable integration for all queries',
    type: 'boolean'
  },
  apikey: {
    help: 'Your API key from service.com',
    password: true, // Hide value in UI
    required: true  // Must be set to use integration
  },
  username: {
    help: 'Your username from service.com',
    required: true // Must be set to use integration
  }
};

async fetchDomain (user, domain) {
  const apiKey = this.getUserConfig(user, 'apikey');      // fetch this data from this service's configuration
  const username = this.getUserConfig(user, 'username');  // fetch this data from this service's configuration
  if (!apiKey || !username) {
    return undefined;
  }

  // Use API key in request - there are many ways to do this
  // see the documentation for your service's API for details
  const result = await axios.get(url, {
    headers: {
      'Authorization': `Bearer ${apiKey}`, // might be in a bearer token
      'User-Agent': this.userAgent()
    },
    auth: { // might be in an auth object
      key: apiKey,
      username: username
    }
    // or a variety of other options
  });
  // ...
}
```

### Multiple Authentication Fields

```javascript
settings = {
  disabled: {
    help: 'Disable integration for all queries',
    type: 'boolean'
  },
  user: {
    help: 'Your API username',
    required: true
  },
  key: {
    help: 'Your API key',
    password: true,
    required: true
  }
};

async fetchDomain (user, domain) {
  const apiUser = this.getUserConfig(user, 'user');
  const apiKey = this.getUserConfig(user, 'key');

  if (!apiUser || !apiKey) {
    return undefined;
  }

  // Query parameters authentication
  const result = await axios.get(url, {
    params: {
      api_username: apiUser,
      api_key: apiKey
    },
    headers: {
      'User-Agent': this.userAgent()
    }
  });
  // ...
}
```

### Shared Configuration

If multiple integrations use the same service credentials:

```javascript
class SecondIntegration extends Integration {
  name = 'Service Feature B';
  configName = 'Service Feature A';    // Use settings from this integration
  // ...
}
```

## Testing

### Manual Testing

1. **Start Cont3xt**: Run the Cont3xt service with your integration
2. **Enable Integration**: Configure any required settings in the UI
3. **Query Indicator**: Search for a test indicator (domain, IP, etc.)
4. **Verify Results**:
   - Check that the integration button appears
   - Click the button to view the card
   - Verify all fields display correctly
   - Check tidbits appear in the IRT
   - Test error cases (invalid input, API errors)

### Debug Logging

Set debug level to see detailed logs:

```bash
# Start with debug logging
DEBUG=2 npm start
```

### Common Issues

1. **Button doesn't appear**: Add `icon` property or check `itypes`
2. **No data shown**: Verify `_cont3xt: { count: N }` is set
3. **Fields empty**: Check `field` paths match API response structure - you can check this by examining the raw data within the integration card
4. **Authentication fails**: Verify `getUserConfig()` calls and settings
5. **Tidbit not showing**: Check `field` path and add `tooltip`

## Best Practices

1. **Always include user agent**: Use `this.userAgent()` in headers
2. **Handle errors gracefully**: Suppress 404s, log API errors
3. **Cache appropriately**: Use longer cache for slow/flaky services
4. **Return correct values**: `undefined` for no data, `null` for errors
5. **Add _cont3xt count**: Always include `_cont3xt: { count: N }`
6. **Use existing patterns**: Look at similar integrations for reference
7. **Document flaky services**: Add comments about service limitations
8. **Validate API responses**: Check for expected data structure
9. **Use nested field paths**: Use dot notation like `'registration.created'`
10. **Test with real data**: Use actual API responses to verify fields

## Additional Resources

- **[descriptions.txt](../descriptions.txt)**: Complete reference for card fields, tidbits, and post-processors
- **Existing integrations**: Browse `cont3xt/integrations/` for more examples
- **Integration base class**: See `cont3xt/integration.js` for available methods

## Need Help?

Look at existing integrations for patterns:
- **Simple public API**: `whois`, `crtsh`
- **Authenticated API**: `domaintools`, `shodan`, `virustotal`
- **Multiple itypes**: `rdap`, `virustotal`
- **Complex tables**: `shodan`, `passivetotal`
