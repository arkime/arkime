/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const EmailValidator = require('email-deep-validator');
const emailValidator = new EmailValidator();

class EmailVerifyIntegration extends Integration {
  name = 'EmailVerify';
  itypes = {
    email: 'fetchEmail'
  };

  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  };

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchEmail (user, email) {
    try {
      const result = await emailValidator.verify(email);
      return result;
    } catch (err) {
      console.log(this.name, email, err);
      return undefined;
    }
  }
}

// eslint-disable-next-line no-new
new EmailVerifyIntegration();
