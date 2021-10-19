/******************************************************************************/
/* auth.js  -- common Auth apis
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
'use strict';

// eslint-disable-next-line no-shadow
const crypto = require('crypto');
const User = require('../common/user');
const passport = require('passport');
const DigestStrategy = require('passport-http').DigestStrategy;

class Auth {
  static httpRealm;
  static passwordSecret;
  static passwordSecret256;
  static serverSecret256;
  static basePath;
  static authFunc;

  static initialize (options) {
    Auth.debug = options.debug ?? 0;
    Auth.mode = options.mode ?? 'anonymous';
    Auth.basePath = options.basePath ?? '/';
    Auth.userNameHeader = options.userNameHeader;
    Auth.httpRealm = options.httpRealm ?? 'Moloch';
    Auth.passwordSecret = options.passwordSecret ?? 'password';
    Auth.passwordSecret256 = crypto.createHash('sha256').update(Auth.passwordSecret).digest();
    if (options.serverSecret) {
      Auth.serverSecret256 = crypto.createHash('sha256').update(options.serverSecret).digest();
    } else {
      Auth.serverSecret256 = Auth.passwordSecret256;
    }

    if (Auth.mode === 'digest') {
      passport.use(new DigestStrategy({ qop: 'auth', realm: Auth.httpRealm },
        function (userid, done) {
          User.getUserCache(userid, (err, user) => {
            if (err) { return done(err); }
            if (!user) { console.log('User', userid, "doesn't exist"); return done(null, false); }
            if (!user.enabled) { console.log('User', userid, 'not enabled'); return done('Not enabled'); }

            return done(null, user, { ha1: Auth.store2ha1(user.passStore) });
          });
        },
        function (poptions, done) {
          // TODO:  Should check nonce here
          return done(null, true);
        }
      ));
    }

    switch (Auth.mode) {
    case 'anonymous':
      Auth.authFunc = Auth.anonymousAuth;
      break;
    case 'anonymousWithDB':
      Auth.authFunc = Auth.anonymousWithDBAuth;
      break;
    case 'digest':
      Auth.authFunc = Auth.digestAuth;
      break;
    case 'header':
      Auth.authFunc = Auth.headerAuth;
      break;
    }
  }

  static anonymousAuth (req, res, next) {
    req.user = Object.assign(new User(), {
      userId: 'anonymous',
      enabled: true,
      createEnabled: true,
      webEnabled: true,
      headerAuthEnabled: false,
      emailSearch: true,
      removeEnabled: true,
      packetSearch: true,
      settings: {},
      welcomeMsgNum: 1
    });
  }

  static anonymousWithDBAuth (req, res, next) {
    req.user = Object.assign(new User(), {
      userId: 'anonymous',
      enabled: true,
      createEnabled: false,
      webEnabled: true,
      headerAuthEnabled: false,
      emailSearch: true,
      removeEnabled: true,
      packetSearch: true,
      settings: {},
      welcomeMsgNum: 1
    });
    User.getUserCache('anonymous', (err, user) => {
      if (user) {
        req.user.settings = user.settings;
        req.user.views = user.views;
        req.user.columnConfigs = user.columnConfigs;
        req.user.spiviewFieldConfigs = user.spiviewFieldConfigs;
        req.user.tableStates = user.tableStates;
      }
      return next();
    });
  }

  static regressionTestsAuth (req, res, next) {
    const userId = req.query.molochRegressionUser ?? 'anonymous';
    User.getUserCache(userId, (err, user) => {
      if (user) {
        req.user = user;
        return next();
      }
      req.user = Object.assign(new User(), {
        userId: userId,
        enabled: true,
        createEnabled: userId === 'anonymous',
        webEnabled: true,
        headerAuthEnabled: false,
        emailSearch: true,
        removeEnabled: true,
        packetSearch: true,
        settings: {},
        welcomeMsgNum: 1
      });
      return next();
    });
  }

  static digestAuth (req, res, next) {
    if (Auth.basePath !== '/') {
      req.url = req.url.replace('/', Auth.basePath);
    }
    passport.authenticate('digest', { session: false })(req, res, function (err) {
      if (Auth.basePath !== '/') {
        req.url = req.url.replace(Auth.basePath, '/');
      }
      if (err) {
        res.status(403);
        return res.send(JSON.stringify({ success: false, text: err }));
      } else {
        return next();
      }
    });
  }

  static headerAuth (req, res, next) {
    if (req.headers[Auth.userNameHeader] !== undefined) {
      return User.getUserCache(req.headers[Auth.userNameHeader], (err, user) => {
        if (err || !user) { return res.send(JSON.stringify({ success: false, text: 'Username not found' })); }
        if (!user.enabled) { return res.send(JSON.stringify({ success: false, text: 'Username not enabled' })); }
        req.user = user;
        return next();
      });
    } else if (Auth.debug > 0) {
      console.log(`AUTH: looking for header ${Auth.userNameHeader} in the headers`, req.headers);
      res.status(403);
      return res.send(JSON.stringify({ success: false, text: 'Username not found' }));
    }
  }

  // Need this wrapper because app.use seems to save the value and not the array
  static doAuth (req, res, next) {
    return Auth.authFunc(req, res, next);
  }

  static md5 (str, encoding) {
    return crypto
      .createHash('md5')
      .update(str)
      .digest(encoding || 'hex');
  };

  // Hash (MD5) and encrypt the password before storing.
  // Encryption is used because ES is insecure by default and we don't want others adding accounts.
  static pass2store (userid, password) {
    // md5 is required because of http digest
    const m = Auth.md5(userid + ':' + Auth.httpRealm + ':' + password);

    // New style with IV: IV.E
    const iv = crypto.randomBytes(16);
    const c = crypto.createCipheriv('aes-256-cbc', Auth.passwordSecret256, iv);
    let e = c.update(m, 'binary', 'hex');
    e += c.final('hex');
    return iv.toString('hex') + '.' + e;
  };

  // Decrypt the encrypted hashed password, it is still hashed
  static store2ha1 (passstore) {
    try {
      const parts = passstore.split('.');
      if (parts.length === 2) {
        // New style with IV: IV.E
        const c = crypto.createDecipheriv('aes-256-cbc', Auth.passwordSecret256, Buffer.from(parts[0], 'hex'));
        let d = c.update(parts[1], 'hex', 'binary');
        d += c.final('binary');
        return d;
      } else {
        // Old style without IV: E
        // eslint-disable-next-line node/no-deprecated-api
        const c = crypto.createDecipher('aes192', Auth.passwordSecret);
        let d = c.update(passstore, 'hex', 'binary');
        d += c.final('binary');
        return d;
      }
    } catch (e) {
      console.log("passwordSecret set in the [default] section can not decrypt information.  You may need to re-add users if you've changed the secret.", e);
      process.exit(1);
    }
  };

  // Encrypt an object into an auth string
  static obj2auth (obj, c2s, secret) {
    // New style with IV: IV.E.H
    if (secret) {
      secret = crypto.createHash('sha256').update(secret).digest();
    } else {
      secret = Auth.serverSecret256;
    }

    const iv = crypto.randomBytes(16);
    const c = crypto.createCipheriv('aes-256-cbc', secret, iv);
    let e = c.update(JSON.stringify(obj), 'binary', 'hex');
    e += c.final('hex');
    e = iv.toString('hex') + '.' + e;
    const h = crypto.createHmac('sha256', secret).update(e).digest('hex');
    return e + '.' + h;
  };

  // Decrypt the auth string into an object
  static auth2obj (auth, c2s, secret) {
    const parts = auth.split('.');

    if (parts.length === 3) {
      // New style with IV: IV.E.H
      if (secret) {
        secret = crypto.createHash('sha256').update(secret).digest();
      } else {
        secret = Auth.serverSecret256;
      }

      const signature = Buffer.from(parts[2], 'hex');
      const h = crypto.createHmac('sha256', secret).update(parts[0] + '.' + parts[1]).digest();

      if (!crypto.timingSafeEqual(signature, h)) {
        throw new Error('Incorrect signature');
      }

      try {
        const c = crypto.createDecipheriv('aes-256-cbc', secret, Buffer.from(parts[0], 'hex'));
        let d = c.update(parts[1], 'hex', 'binary');
        d += c.final('binary');
        return JSON.parse(d);
      } catch (error) {
        console.log(error);
        throw new Error('Incorrect auth supplied');
      }
    } else {
      // Old style without IV: E or E.H

      secret = secret || Auth.serverSecret;

      if (parts.length === 1) {
        throw new Error('Missing signature');
      }

      if (parts.length > 1) {
        const signature = Buffer.from(parts[1], 'hex');
        const h = crypto.createHmac('sha256', secret).update(parts[0], 'hex').digest();

        if (!crypto.timingSafeEqual(signature, h)) {
          throw new Error('Incorrect signature');
        }
      }

      try {
        // eslint-disable-next-line node/no-deprecated-api
        const c = crypto.createDecipher('aes192', secret);
        let d = c.update(parts[0], 'hex', 'binary');
        d += c.final('binary');
        return JSON.parse(d);
      } catch (error) {
        console.log(error);
        throw new Error('Incorrect auth supplied');
      }
    }
  };

  static addS2SAuth (options, user, node, path, secret) {
    if (!options.headers) {
      options.headers = {};
    }
    options.headers['x-moloch-auth'] = Auth.obj2auth({
      date: Date.now(),
      user: user.userId,
      node: node,
      path: path
    }, false, secret);
  }
}

module.exports = Auth;
